/* Watch Settings uses an external RTC with checked native custody. */
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_CHECKED_STORAGE_RUNTIME
#define PORTABLE_INPUT_NAVIGATION
#define TEST_SETTINGS_CUSTOM_RUNTIME
#define main original_settings_fixture_main
#include "portable_settings_test.c"
#undef main

static unsigned release_calls, retained_calls, refused;
static risc_runtime_api_v1 checked_runtime;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
  return version == 1 ? &checked_runtime : NULL;
}
static bool battery_read(void *context, risc_battery_sample_v1 *out) {
  (void)context;
  assert(!retained_calls);
  *out = (risc_battery_sample_v1){.millivolts=4000, .percent=80};
  return true;
}
static const risc_battery_gauge_api_v1 battery_api = {
  .api_version=1, .struct_size=sizeof(battery_api), .read=battery_read
};
static bool acquire_checked(const char *name, uint32_t version, uint64_t id,
                            risc_runtime_capability_v1 *grant) {
  assert(!retained_calls);
  if (!strcmp(name, RISC_BATTERY_GAUGE_CAPABILITY)) {
    assert(version == 1 && !id && grant->struct_size == sizeof(*grant));
    grant->api = &battery_api;
    ++grants;
    return true;
  }
  return test_acquire(name, version, id, grant);
}
static bool retain_native(void) { ++retained_calls; return true; }
static bool release_checked(risc_runtime_capability_v1 *grant) {
  assert(!retained_calls);
  ++release_calls;
  if ((refused == 1 && grant->api == &rtc_api) ||
      (refused == 2 && grant->api == &kv_api)) return false;
  return test_release(grant);
}
int main(int argc, char **argv) {
  assert(argc == 2); refused = (unsigned)atoi(argv[1]); assert(refused <= 2);
  scenario = 100;
  checked_runtime = runtime_api;
  checked_runtime.retain_invocation = retain_native;
  checked_runtime.acquire = acquire_checked;
  checked_runtime.release = release_checked;
  assert(app_module_init() == 0);
  assert(native_custody_runtime == &checked_runtime && rt == &settings_runtime);
  assert(rt->acquire == custody_acquire && rt->release == custody_release);
  assert(grants == 6 && bg.api == &battery_api && navigation_ready && subscriptions == 1);
  assert(settings_store && rtc && settings_grant.api && rtc_grant.api);
  const risc_key_value_v1 *held_store = settings_store;
#ifdef PORTABLE_ALARM_SETTINGS
  assert(alert_store == held_store);
#endif
  const t5_app_api_v1 *api = t5_app_get_api(T5_APP_ABI_VERSION);
  assert(api);
  api->settings_render(0, 0);
  assert(sv_active);
  app_module_fini();
  if (refused) {
    assert(retained_calls == 1 && native_custody_retained);
    assert(grants == (refused == 1 ? 4u : 3u) && settings_grant.api);
    assert(settings_store == held_store && sv_active);
    assert(bg.api == &battery_api && dg.api == &display_api);
    assert((refused == 1) == (rtc_grant.api != NULL));
    assert((refused == 1) == (rtc != NULL));
#ifdef PORTABLE_ALARM_SETTINGS
    assert(alert_store == held_store);
#endif
    unsigned held = grants, calls = release_calls;
    app_module_fini();
    assert(grants == held && release_calls == calls && retained_calls == 1);
  } else {
    assert(!retained_calls && !grants && !subscriptions && !frame_count);
    assert(!rtc && !settings_store && !settings_grant.api && !rtc_grant.api && !sv_active);
#ifdef PORTABLE_ALARM_SETTINGS
    assert(!alert_store);
#endif
    unsigned calls = release_calls;
    app_module_fini();
    assert(release_calls == calls && !retained_calls && !grants);
  }
  puts("Watch checked Settings cleanup: RTC/store custody and repeat-fini passed");
  return 0;
}
