/* Scheduled raw-touch events exercise the unchanged monitor's own reducer and
 * Back navigation, not a replacement controller or Runtime exit callback. */
#include "bridge_fixture.h"
#include <RiscProviderV2.h>
#include <RiscTouchV1.h>
#include <assert.h>
#include <string.h>

typedef struct { uint32_t at; uint16_t x, y; const char *event; } tap;
static const tap normal_taps[] = {
    {100, 20, 64, "touch.select-instance-41"},
    {300, 20, 88, "touch.select-device-17-generation-9"},
    {1600, 20, 12, "touch.back-close"},
    {1900, 20, 12, "touch.back-exit"}
};
static const tap reopen_taps[] = {
    {100, 20, 64, "touch.select-instance-41"},
    {300, 20, 88, "touch.select-device-17-generation-9"},
    {1600, 20, 12, "touch.back-close"},
    {1900, 20, 64, "touch.reselect-instance-41"},
    {2200, 20, 88, "touch.reselect-device-17-generation-9"},
    {3500, 20, 12, "touch.back-close-reopened"},
    {3800, 20, 12, "touch.back-exit"}
};
static const tap *schedule;
static size_t taps, next_event;
static bool started, subscribed, down, select_second;
static uint32_t began_at;
static uint64_t sequence;
static uint16_t width, height, x, y;

static void observe(const char *event) {
    monitor_fixture_event(event);
    assert(started);
}

static uint64_t subscribe(void *context) {
    (void)context;
    observe("touch.subscribe");
    assert(!subscribed);
    subscribed = true;
    return 1;
}

static bool unsubscribe(void *context, uint64_t subscription) {
    (void)context;
    observe("touch.unsubscribe");
    assert(subscribed && subscription == 1);
    subscribed = false;
    return true;
}

static bool poll(void *context, size_t max_reports) {
    (void)context;
    observe("touch.poll");
    assert(subscribed && max_reports == 1);
    return true;
}

static int32_t next(void *context, uint64_t subscription, risc_touch_event_v1 *out) {
    (void)context;
    observe("touch.next");
    assert(subscribed && subscription == 1 && out);
    if (next_event >= taps * 2u) return 0;
    const tap *item = &schedule[next_event / 2u];
    bool release = (next_event & 1u) != 0;
    uint32_t at = item->at + (release ? 20u : 0u);
    uint32_t now = monitor_fixture_now();
    if ((uint32_t)(now - began_at) < at) return 0;
    assert(down == release);
    x = item->x;
    y = select_second && next_event < 2u ? 88u : item->y;
    assert(x < width && y < height);
    down = !release;
    ++next_event;
    *out = (risc_touch_event_v1){
        .sequence = ++sequence, .timestamp_ms = began_at + at,
        .kind = release ? RISC_TOUCH_EVENT_UP : RISC_TOUCH_EVENT_DOWN,
        .id = 1, .x = x, .y = y
    };
    if (release && select_second && next_event == 2u)
        monitor_fixture_event("touch.select-instance-42");
    else
        monitor_fixture_event(release ? item->event : "touch.down");
    return 1;
}

static bool snapshot(void *context, risc_touch_snapshot_v1 *out) {
    (void)context;
    observe("touch.snapshot");
    assert(subscribed && out);
    *out = (risc_touch_snapshot_v1){
        .sequence = sequence, .timestamp_ms = monitor_fixture_now(),
        .width = width, .height = height, .contact_count = down ? 1u : 0u
    };
    if (down) out->contacts[0] = (risc_touch_contact_v1){.id = 1, .x = x, .y = y};
    return true;
}

static const risc_touch_api_v1 api = {
    1, sizeof(api), NULL, subscribe, unsubscribe, poll, next, snapshot
};

static bool start(const risc_provider_dependency_v1 *deps, size_t count) {
    (void)deps;
    monitor_fixture_event("touch.start");
    assert(!started && !count);
    unsigned w = monitor_fixture_width(), h = monitor_fixture_height();
    assert(w >= 160 && w <= UINT16_MAX && h >= 160 && h <= UINT16_MAX);
    width = (uint16_t)w;
    height = (uint16_t)h;
    bool reopen = !strcmp(monitor_fixture_mode(), "reopen");
    select_second = !strcmp(monitor_fixture_mode(), "select-second");
    schedule = reopen ? reopen_taps : normal_taps;
    taps = reopen ? sizeof(reopen_taps) / sizeof(*reopen_taps)
                  : sizeof(normal_taps) / sizeof(*normal_taps);
    began_at = monitor_fixture_now();
    sequence = next_event = 0;
    subscribed = down = false;
    started = true;
    return true;
}

static bool quiesce(void) {
    observe("touch.quiesce");
    return !subscribed;
}

static void stop(void) {
    observe("touch.stop");
    assert(!subscribed);
    started = false;
}

static const risc_driver_v2 driver = {
    2, sizeof(driver), "monitor-touch", "input.touch.raw", 1, &api,
    start, stop, quiesce
};

__attribute__((visibility("default")))
const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    monitor_fixture_event("touch.get");
    return abi == 2 ? &driver : NULL;
}

__attribute__((destructor)) static void unloaded(void) {
    monitor_fixture_event("touch.unload");
}
