/* Software-only battery capability for the unchanged native adapter. */
#include "bridge_fixture.h"
#include <RiscProviderV2.h>
#include <RiscBatteryGaugeV1.h>
#include <assert.h>

static bool started;

static bool read_sample(void *context, risc_battery_sample_v1 *out) {
    (void)context;
    monitor_fixture_event("battery.read");
    assert(started && out);
    *out = (risc_battery_sample_v1){.millivolts = 3900, .percent = 70, .flags = 0};
    return true;
}

static const risc_battery_gauge_api_v1 api = {
    1, sizeof(api), NULL, read_sample
};

static bool start(const risc_provider_dependency_v1 *deps, size_t count) {
    (void)deps;
    monitor_fixture_event("battery.start");
    assert(!started && !count);
    started = true;
    return true;
}

static bool quiesce(void) {
    monitor_fixture_event("battery.quiesce");
    assert(started);
    return true;
}

static void stop(void) {
    monitor_fixture_event("battery.stop");
    assert(started);
    started = false;
}

static const risc_driver_v2 driver = {
    2, sizeof(driver), "monitor-battery", "board.battery", 1, &api,
    start, stop, quiesce
};

__attribute__((visibility("default")))
const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    monitor_fixture_event("battery.get");
    return abi == 2 ? &driver : NULL;
}

__attribute__((destructor)) static void unloaded(void) {
    monitor_fixture_event("battery.unload");
}
