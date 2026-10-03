#include "PortableApps.h"
#include "T5BatteryApi.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void);
int app_module_init(void);
void app_module_fini(void);
const t5_app_manifest_t portable_catalog[] = {{.display_name = "Clock",
                                               .file_name = "default.elf",
                                               .icon = "solid:f017",
                                               .compatible = true},
                                              {.display_name = "Battery",
                                               .file_name = "battery.elf",
                                               .icon = "solid:f240",
                                               .compatible = true}};
const unsigned portable_catalog_count = 2;
static unsigned ms, polls, subs, grants, frames, presents, reads, scenario;
static uint16_t pixels[240 * 240];
static char launched[128];
static bool health(risc_runtime_health_v1 *h) {
  h->uptime_ms = ms;
  return polls < 24;
}
static void yield(uint32_t n) { ms += n; }
static bool log_line(const char *s) {
  (void)s;
  return true;
}
static bool launch(const char *s) {
  strcpy(launched, s);
  return true;
}
static bool info(void *c, risc_display_info_v1 *o) {
  (void)c;
  memset(o, 0, sizeof(*o));
  o->width = o->height = 240;
  o->supported_formats = RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);
  return true;
}
static bool acquire_frame(void *c, uint32_t f, risc_display_surface_v1 *s) {
  (void)c;
  assert(!frames);
  frames = 1;
  *s = (risc_display_surface_v1){.frame = 1,
                                 .pixels = pixels,
                                 .size_bytes = sizeof(pixels),
                                 .width = 240,
                                 .height = 240,
                                 .stride_bytes = 480,
                                 .pixel_format = f};
  return true;
}
static void release_frame(void *c, risc_display_frame_v1 f) {
  (void)c;
  assert(f == 1 && frames);
  frames = 0;
}
static bool submit(void *c, risc_display_frame_v1 f,
                   const risc_display_rect_v1 *r, size_t n,
                   const risc_display_present_options_v1 *o,
                   risc_display_present_token_v1 *token) {
  (void)c;
  (void)r;
  (void)n;
  (void)o;
  assert(f == 1 && frames);
  if (scenario == 2)
    return false;
  frames = 0;
  *token = ++presents;
  return true;
}
static bool status(void *c, risc_display_present_token_v1 t,
                   risc_display_present_status_v1 *s) {
  (void)c;
  (void)t;
  s->state = RISC_DISPLAY_PRESENT_COMPLETE;
  return true;
}
static uint64_t subscribe(void *c) {
  (void)c;
  subs++;
  return 1;
}
static bool unsubscribe(void *c, uint64_t n) {
  (void)c;
  assert(n == 1 && subs);
  subs--;
  return true;
}
static bool poll_touch(void *c, size_t n) {
  (void)c;
  assert(n == 1);
  polls++;
  return true;
}
static int32_t next_touch(void *c, uint64_t n, risc_touch_event_v1 *e) {
  (void)c;
  (void)n;
  (void)e;
  return scenario == 3 && polls == 3 ? -1 : 0;
}
static bool snapshot(void *c, risc_touch_snapshot_v1 *s) {
  (void)c;
  memset(s, 0, sizeof(*s));
  s->width = s->height = 240;
#ifdef BATTERY_TEST
  if (polls == 2) {
    s->contact_count = 1;
    s->contacts[0] = (risc_touch_contact_v1){.x = 220, .y = 18};
  }
  if (polls == 5) {
    s->contact_count = 1;
    s->contacts[0] = (risc_touch_contact_v1){.x = 20, .y = 18};
  }
#else
  if ((scenario == 1 && polls <= 2) || polls == 4 ||
      (scenario == 3 && polls == 7)) {
    s->contact_count = 1;
    s->contacts[0] = (risc_touch_contact_v1){.x = 160, .y = 100};
  }
#endif
  return true;
}
static bool battery_read(void *c, risc_battery_sample_v1 *s) {
  (void)c;
  reads++;
  *s = (risc_battery_sample_v1){
      .percent = 73, .millivolts = 3970, .flags = RISC_BATTERY_CHARGING};
  if (scenario == 5) s->percent = 255;
  if (scenario == 6) s->flags |= RISC_BATTERY_PROFILE_MISSING;
  if (scenario == 7) s->percent = 0;
  return true;
}
static const risc_display_output_api_v1 d = {.api_version = 1,
                                             .struct_size = sizeof(d),
                                             .get_info = info,
                                             .acquire = acquire_frame,
                                             .release = release_frame,
                                             .submit = submit,
                                             .present_status = status};
static const risc_touch_api_v1 t = {1,          sizeof(t),   NULL,
                                    subscribe,  unsubscribe, poll_touch,
                                    next_touch, snapshot};
static const risc_battery_gauge_api_v1 b = {1, sizeof(b), NULL, battery_read};
static bool acquire(const char *name, uint32_t v, uint64_t id,
                    risc_runtime_capability_v1 *g) {
  assert(id == 0 && v == 1 && g->struct_size == sizeof(*g));
  if (!strcmp(name, "display.output"))
    g->api = &d;
  else if (!strcmp(name, "input.touch.raw")) {
    if (scenario == 4)
      return false;
    g->api = &t;
  } else if (!strcmp(name, "board.battery"))
    g->api = &b;
  else
    return false;
  grants++;
  return true;
}
static bool release(risc_runtime_capability_v1 *g) {
  assert(grants && g->api);
  grants--;
  g->api = NULL;
  return true;
}
static const risc_runtime_api_v1 rt = {1,        sizeof(rt), health,  yield,
                                       log_line, launch,     acquire, release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v) {
  return v == 1 ? &rt : NULL;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  scenario = (unsigned)atoi(argv[1]);
  int init = app_module_init();
  if (scenario == 4) {
    assert(init != 0 && !grants && !subs);
    return 0;
  }
  assert(init == 0);
#ifdef BATTERY_TEST
  if (scenario >= 5) {
    t5_battery_state_t state;
    const t5_battery_api_v1 *api = t5_battery_get_api(1);
    assert(api && api->read(&state));
    assert(state.available && state.gauge_read_ok && state.gauge_voltage_mv == 3970 && state.charging);
    assert(state.soc_percent == (scenario == 7 ? 0 : UINT16_MAX));
    reads = 0;
  }
#endif
  app_main();
  app_module_fini();
  assert(!frames && !grants && !subs);
#ifdef BATTERY_TEST
  assert(reads >= 1);
  if (scenario == 0 || scenario >= 5)
    assert(reads == 2 && presents >= 2);
#else
  if (scenario != 2)
    assert(!strcmp(launched, "battery.elf"));
  else
    assert(!launched[0]);
#endif
  printf("portable real app scenario %u: lifecycle/touch/presentation passed\n",
         scenario);
  return 0;
}
