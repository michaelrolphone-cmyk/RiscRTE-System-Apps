/* Memory-only asynchronous RGB565 provider, loaded through the real provider
 * registry. No Runtime acquisition, invocation or stream-client substitutes. */
#include "bridge_fixture.h"
#include <RiscProviderV2.h>
#include <RiscDisplayOutputV1.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define MAX_WIDTH 480u
#define MAX_HEIGHT 800u
#define MAX_PIXELS (MAX_WIDTH * MAX_HEIGHT)
static struct {
    uint8_t before[16];
    uint16_t pixels[MAX_PIXELS];
    uint8_t after[16];
} memory;
static uint16_t frozen[MAX_PIXELS];
static bool started, acquired;
static unsigned width, height;
static uint32_t submitted_at, latency_ms;
static risc_display_present_token_v1 pending, next_token;

static void observe(const char *event) {
    monitor_fixture_event(event);
    assert(started);
}

static void bounds(void) {
    for (size_t i = 0; i < sizeof(memory.before); ++i)
        assert(memory.before[i] == 0xa5 && memory.after[i] == 0x5a);
    for (size_t i = (size_t)width * height; i < MAX_PIXELS; ++i)
        assert(memory.pixels[i] == 0xc3c3);
}

static void immutable(void) {
    bounds();
    if (pending) assert(!memcmp(frozen, memory.pixels, sizeof(frozen)));
}

static bool get_info(void *context, risc_display_info_v1 *out) {
    (void)context;
    observe("display.get-info");
    assert(out);
    *out = (risc_display_info_v1){
        .api_version = 1, .struct_size = sizeof(*out),
        .width = width, .height = height,
        .supported_formats = RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),
        .preferred_format = RISC_DISPLAY_FORMAT_RGB565,
        .supported_rotations = RISC_DISPLAY_ROTATION_0,
        .flags = RISC_DISPLAY_INFO_ASYNC_PRESENT,
        .typical_present_latency_us = latency_ms * 1000u
    };
    immutable();
    return true;
}

static bool acquire(void *context, uint32_t format, risc_display_surface_v1 *out) {
    (void)context;
    observe("display.acquire");
    immutable();
    assert(!acquired && !pending && out && format == RISC_DISPLAY_FORMAT_RGB565);
    acquired = true;
    *out = (risc_display_surface_v1){
        .frame = 1, .pixels = memory.pixels,
        .width = width, .height = height,
        .stride_bytes = width * 2u, .size_bytes = width * height * 2u,
        .pixel_format = format
    };
    return true;
}

static void release(void *context, risc_display_frame_v1 frame) {
    (void)context;
    observe("display.release");
    immutable();
    assert(acquired && !pending && frame == 1);
    acquired = false;
}

static bool submit(void *context, risc_display_frame_v1 frame,
                   const risc_display_rect_v1 *damage, size_t count,
                   const risc_display_present_options_v1 *options,
                   risc_display_present_token_v1 *token) {
    (void)context;
    observe("display.submit");
    assert(acquired && !pending && frame == 1 && options && token);
    assert(count <= RISC_DISPLAY_MAX_DAMAGE_RECTS && (!count || damage));
    for (size_t i = 0; i < count; ++i) {
        assert(damage[i].x >= 0 && damage[i].y >= 0);
        assert((uint32_t)damage[i].x <= width && (uint32_t)damage[i].y <= height);
        assert(damage[i].width <= width - (uint32_t)damage[i].x);
        assert(damage[i].height <= height - (uint32_t)damage[i].y);
    }
    bounds();
    size_t black = 0;
    for (size_t i = 0; i < (size_t)width * height; ++i)
        black += memory.pixels[i] == 0;
    assert(black > 50 && black < (size_t)width * height);
    acquired = false;
    pending = ++next_token;
    assert(pending);
    *token = pending;
    submitted_at = monitor_fixture_now();
    memcpy(frozen, memory.pixels, sizeof(frozen));
    monitor_fixture_present_pending(true);
    monitor_fixture_frame(memory.pixels, width, height, width * 2u, pending);
    immutable();
    return true;
}

static bool present_status(void *context, risc_display_present_token_v1 token,
                           risc_display_present_status_v1 *out) {
    (void)context;
    observe("display.present-status");
    assert(token && token == pending && out && !acquired);
    immutable();
    *out = (risc_display_present_status_v1){0};
    if ((uint32_t)(monitor_fixture_now() - submitted_at) < latency_ms) {
        out->state = RISC_DISPLAY_PRESENT_ACTIVE;
        monitor_fixture_event("display.immutable-active");
    } else {
        out->state = RISC_DISPLAY_PRESENT_COMPLETE;
        pending = 0;
        monitor_fixture_present_pending(false);
        monitor_fixture_event("display.immutable-complete");
    }
    return true;
}

static const risc_display_output_api_v1 api = {
    .api_version = 1, .struct_size = sizeof(api),
    .get_info = get_info, .acquire = acquire, .release = release,
    .submit = submit, .present_status = present_status
};

static bool start(const risc_provider_dependency_v1 *deps, size_t count) {
    (void)deps;
    monitor_fixture_event("display.start");
    assert(!started && !count);
    width = monitor_fixture_width();
    height = monitor_fixture_height();
    assert(width >= 160 && width <= MAX_WIDTH && height >= 160 && height <= MAX_HEIGHT);
    memset(&memory, 0, sizeof(memory));
    memset(memory.before, 0xa5, sizeof(memory.before));
    memset(memory.after, 0x5a, sizeof(memory.after));
    memset(memory.pixels, 0xc3, sizeof(memory.pixels));
    latency_ms = !strcmp(monitor_fixture_mode(), "slow-frame") ? 600u : 20u;
    next_token = pending = 0;
    acquired = false;
    started = true;
    return true;
}

static bool quiesce(void) {
    observe("display.quiesce");
    immutable();
    return !acquired && !pending;
}

static void stop(void) {
    observe("display.stop");
    immutable();
    assert(!acquired && !pending);
    started = false;
}

static const risc_driver_v2 driver = {
    2, sizeof(driver), "monitor-display", "display.output", 1, &api,
    start, stop, quiesce
};

__attribute__((visibility("default")))
const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    monitor_fixture_event("display.get");
    return abi == 2 ? &driver : NULL;
}

__attribute__((destructor)) static void unloaded(void) {
    monitor_fixture_event("display.unload");
}
