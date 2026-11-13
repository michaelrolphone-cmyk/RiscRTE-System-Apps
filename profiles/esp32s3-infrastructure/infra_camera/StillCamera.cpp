#include "../../../Services/infra_node/CameraFrame.h"
#include <img_converters.h>
#include <esp_heap_caps.h>
#include <cstdlib>
#include <algorithm>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <cstdio>
#include <esp_camera.h>
#include <cstring>

namespace {
// GOOUUU ESP32-S3-CAM V1.5 OV2640 socket, data pins in D0..D7 order.
camera_config_t configuration() {
    camera_config_t c{};
    c.pin_pwdn = -1;
    c.pin_reset = -1;
    c.pin_xclk = 15;
    c.pin_sccb_sda = 4;
    c.pin_sccb_scl = 5;
    c.pin_d0 = 11;
    c.pin_d1 = 9;
    c.pin_d2 = 8;
    c.pin_d3 = 10;
    c.pin_d4 = 12;
    c.pin_d5 = 18;
    c.pin_d6 = 17;
    c.pin_d7 = 16;
    c.pin_vsync = 6;
    c.pin_href = 7;
    c.pin_pclk = 13;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size = FRAMESIZE_QQVGA;
    c.jpeg_quality = 25;
    c.fb_count = 1;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    return c;
}

int32_t readJpeg(uint8_t* destination, uint32_t capacity) {
    const auto c = configuration();
    if (esp_camera_init(&c) != ESP_OK) return 0;
    auto* frame = esp_camera_fb_get();
    int32_t size = 0;
    if (frame) {
        const size_t n = frame->len;
        if (frame->format == PIXFORMAT_JPEG && frame->width == 160 && frame->height == 120 &&
            n >= 4 && n <= capacity && frame->buf[0] == 0xff && frame->buf[1] == 0xd8 &&
            frame->buf[n-2] == 0xff && frame->buf[n-1] == 0xd9) {
            if (destination) std::memcpy(destination, frame->buf, n);
            size = static_cast<int32_t>(n);
        }
        esp_camera_fb_return(frame);
    }
    return esp_camera_deinit() == ESP_OK ? size : 0;
}
struct Encoded {
    uint8_t* data = nullptr;
    size_t size = 0, capacity = 0;
    bool failed = false;
};
size_t append(void* ctx, size_t index, const void* data, size_t count) {
    auto& b = *static_cast<Encoded*>(ctx);
    if (!count || b.failed) return 0;
    if (!data || index != b.size || count > UINT32_MAX - index) {
        b.failed = true;
        return 0;
    }
    if (index + count > b.capacity) {
        size_t capacity = std::max(index + count, b.capacity ? b.capacity * 2 : size_t(4096));
        auto* next = static_cast<uint8_t*>(heap_caps_realloc(b.data, capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!next) { b.failed = true; return 0; }
        b.data = next;
        b.capacity = capacity;
    }
    std::memcpy(b.data + index, data, count);
    b.size += count;
    return count;
}
bool upload(const char* url, const char* bearer, const char* site, const InfraCameraFrame* frame) {
    esp_http_client_config_t config{};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = true;
    config.timeout_ms = 30000;
    config.method = HTTP_METHOD_PUT;
    auto client = esp_http_client_init(&config);
    if (!client) return false;
    char header[1032];
    std::snprintf(header, sizeof(header), "Bearer %s", bearer);
    bool ok = esp_http_client_set_header(client, "Authorization", header) == ESP_OK;
    if (*site) {
        std::snprintf(header, sizeof(header), "Bearer %s", site);
        ok = ok && esp_http_client_set_header(client, "OAI-Sites-Authorization", header) == ESP_OK;
    }
    ok = ok && esp_http_client_set_header(client, "Content-Type", "image/jpeg") == ESP_OK &&
         esp_http_client_set_post_field(client, reinterpret_cast<const char*>(frame->bytes), frame->length) == ESP_OK &&
         esp_http_client_perform(client) == ESP_OK && esp_http_client_get_status_code(client) == 200;
    esp_http_client_cleanup(client);
    return ok;
}
void release(InfraCameraFrame* frame) {
    std::free(frame->bytes);
    *frame = {};
}
bool capture(uint16_t width, uint16_t height, InfraCameraFrame* out) {
    if (!out || (!width != !height)) return false;
    *out = {};
    auto c = configuration();
    // The pinned esp32-camera driver clamps this to the detected model's maximum
    // before configuring DMA, PSRAM and the sensor.
    c.frame_size = static_cast<framesize_t>(FRAMESIZE_INVALID - 1);
    c.jpeg_quality = 12;
    if (esp_camera_init(&c) != ESP_OK) return false;
    camera_fb_t* frame = esp_camera_fb_get();
    Encoded jpeg;
    bool ok = frame && frame->format == PIXFORMAT_JPEG && frame->len >= 4 &&
              frame->buf[0] == 0xff && frame->buf[1] == 0xd8 &&
              frame->buf[frame->len-2] == 0xff && frame->buf[frame->len-1] == 0xd9;
    if (ok) {
        const unsigned w = frame->width, h = frame->height;
        if (!width) { width = w; height = h; }
        if (width == w && height == h) {
            ok = append(&jpeg, 0, frame->buf, frame->len) == frame->len;
        } else if (width <= w && height <= h) {
            size_t size = size_t(w) * h * 2;
            auto* pixels = static_cast<uint16_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            ok = pixels && jpg2rgb565_bounded(frame->buf, frame->len,
                        reinterpret_cast<uint8_t*>(pixels), size, JPEG_IMAGE_SCALE_0);
            if (ok) {
                for (unsigned y=0; y<height; ++y)
                    for (unsigned x=0; x<width; ++x)
                        pixels[y*width+x] = pixels[(y*h/height)*w+x*w/width];
                jpgSetRgb565BE(false);
                ok = fmt2jpg_cb(reinterpret_cast<uint8_t*>(pixels), size_t(width)*height*2,
                                width, height, PIXFORMAT_RGB565, 85, append, &jpeg);
                jpgSetRgb565BE(true);
            }
            std::free(pixels);
        } else ok = false;
    }
    if (frame) esp_camera_fb_return(frame);
    if (esp_camera_deinit() != ESP_OK || !ok || jpeg.failed || !jpeg.size) {
        std::free(jpeg.data);
        return false;
    }
    *out = {jpeg.data, static_cast<uint32_t>(jpeg.size), width, height};
    return true;
}

}
extern "C" const risc_still_camera_v1* risc_product_still_camera() {
    static const InfraCameraApi api{{1, sizeof(api), readJpeg}, capture, upload, release};
    return &api.legacy;
}
