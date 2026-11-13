#include <RiscStillCameraV1.h>
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
}
extern "C" const risc_still_camera_v1* risc_product_still_camera() {
    static const risc_still_camera_v1 api{1, sizeof(api), readJpeg};
    return &api;
}
