/* Client-side adapter for existing shared apps; no board/chip/pin knowledge. */
#include "PortableApps.h"
#include "PortableTouch.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscDisplayOutputV1.h"
#include "T5BatteryApi.h"
#include "T5StorageApi.h"
#include "T5UiApi.h"
#include "T5VideoApi.h"
#include <limits.h>
static const risc_runtime_api_v1 *rt;
static risc_runtime_capability_v1 dg, bg;
static const risc_display_output_api_v1 *display;
static const risc_battery_gauge_api_v1 *gauge;
static portable_touch touch;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static bool failed, list_mode;
static unsigned first_row, last_rows;
static uint32_t millis_now(void) {
  risc_runtime_health_v1 h = {.struct_size = sizeof(h)};
  if (!rt->health(&h)) {
    failed = true;
    return 0;
  }
  return h.uptime_ms;
}
static int32_t width(void) { return info.width; }
static int32_t height(void) { return info.height; }
static void fill(int x, int y, int w, int h, uint16_t color) {
  if (!surface.frame || w <= 0 || h <= 0)
    return;
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w, y1 = y + h;
  if (x1 > width())
    x1 = width();
  if (y1 > height())
    y1 = height();
  for (int j = y0; j < y1; ++j)
    for (int i = x0; i < x1; ++i) {
      uint8_t *p =
          (uint8_t *)surface.pixels + (size_t)j * surface.stride_bytes + i * 2;
      p[0] = color;
      p[1] = color >> 8;
    }
}
static void clear(void) {
  if (failed)
    return;
  if (surface.frame) {
    display->release(display->context, surface.frame);
    surface.frame = 0;
  }
  if (!display->acquire(display->context, RISC_DISPLAY_FORMAT_RGB565,
                        &surface)) {
    failed = true;
    return;
  }
  if (!surface.frame || !surface.pixels || surface.width != info.width ||
      surface.height != info.height ||
      surface.pixel_format != RISC_DISPLAY_FORMAT_RGB565 ||
      surface.stride_bytes < (uint32_t)info.width * 2 ||
      surface.stride_bytes > UINT32_MAX / info.height ||
      surface.size_bytes < surface.stride_bytes * info.height) {
    failed = true;
    return;
  }
  fill(0, 0, width(), height(), 0xffff);
}
static void rect(int32_t x, int32_t y, int32_t w, int32_t h, bool black) {
  fill(x, y, w, h, black ? 0 : 0xffff);
}
static void rounded(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                    uint8_t tone) {
  static const uint16_t colors[] = {0xffff, 0xbdf7, 0x630c, 0};
  if (w <= 0 || h <= 0 || w > 2048 || h > 2048)
    return;
  if (radius < 0)
    radius = 0;
  if (radius > w / 2)
    radius = w / 2;
  if (radius > h / 2)
    radius = h / 2;
  for (int row = 0; row < h; ++row) {
    int dy = row < radius ? radius - 1 - row
                          : (row >= h - radius ? row - (h - radius) : 0),
        inset = 0;
    while (inset < radius &&
           (radius - inset) * (radius - inset) + dy * dy > radius * radius)
      ++inset;
    fill(x + inset, y + row, w - 2 * inset, 1, colors[tone < 4 ? tone : 3]);
  }
}
/* Original compact uppercase glyphs; bounded text avoids font/filesystem
 * service. */
static const uint8_t glyphs[][5] = {
    {62, 81, 73, 69, 62},  {0, 66, 127, 64, 0},   {98, 81, 73, 73, 70},
    {34, 65, 73, 73, 54},  {24, 20, 18, 127, 16}, {39, 69, 69, 69, 57},
    {60, 74, 73, 73, 48},  {1, 113, 9, 5, 3},     {54, 73, 73, 73, 54},
    {6, 73, 73, 41, 30},   {126, 9, 9, 9, 126},   {127, 73, 73, 73, 54},
    {62, 65, 65, 65, 34},  {127, 65, 65, 34, 28}, {127, 73, 73, 73, 65},
    {127, 9, 9, 9, 1},     {62, 65, 73, 73, 58},  {127, 8, 8, 8, 127},
    {0, 65, 127, 65, 0},   {32, 64, 65, 63, 1},   {127, 8, 20, 34, 65},
    {127, 64, 64, 64, 64}, {127, 2, 12, 2, 127},  {127, 4, 8, 16, 127},
    {62, 65, 65, 65, 62},  {127, 9, 9, 9, 6},     {62, 65, 81, 33, 94},
    {127, 9, 25, 41, 70},  {70, 73, 73, 73, 49},  {1, 1, 127, 1, 1},
    {63, 64, 64, 64, 63},  {31, 32, 64, 32, 31},  {127, 32, 24, 32, 127},
    {99, 20, 8, 20, 99},   {3, 4, 120, 4, 3},     {97, 81, 73, 69, 67}};
static void text_color(int x, int y, const char *s, int limit, uint16_t color) {
  if (!s)
    return;
  for (int k = 0; k < limit && k < 128 && s[k]; ++k) {
    char c = s[k];
    if (c >= 'a' && c <= 'z')
      c -= 32;
    int g = c >= '0' && c <= '9' ? c - '0'
                                 : (c >= 'A' && c <= 'Z' ? c - 'A' + 10 : -1);
    for (int col = 0; col < 5; ++col) {
      unsigned bits =
          g >= 0
              ? glyphs[g][col]
              : (c == '-'
                     ? 8
                     : (c == ':' && col == 2
                            ? 36
                            : (c == '%' ? (col == 0 ? 99 : (col == 4 ? 99 : 8))
                                        : 0)));
      for (int row = 0; row < 7; ++row)
        if (bits & (1u << row))
          fill(x + k * 6 + col, y + row, 1, 1, color);
    }
  }
}
static void text(int32_t x, int32_t y, const char *s) {
  text_color(x, y, s, (width() - x) / 6, 0);
}
static void label(int32_t x, int32_t y, int32_t w, const char *s) {
  int n = 0;
  if (!s)
    return;
  while (n < 128 && s[n])
    ++n;
  if (n > w / 6)
    n = w / 6;
  text_color(x + (w - n * 6) / 2, y, s, n, 0);
}
static bool icon(int32_t x, int32_t y, const char *name, uint8_t size,
                 bool black) {
  uint16_t c = black ? 0 : 0xffff;
  if (!name || size < 8)
    return false;
  if (!strcmp(name, "solid:f240")) {
    fill(x, y + 3, size - 2, size - 6, c);
    fill(x + size - 2, y + size / 3, 2, size / 3, c);
    return true;
  }
  if (!strcmp(name, "solid:f017")) {
    for (int i = 0; i < size; ++i) {
      fill(x + i, y, 1, 1, c);
      fill(x + i, y + size - 1, 1, 1, c);
      fill(x, y + i, 1, 1, c);
      fill(x + size - 1, y + i, 1, 1, c);
    }
    fill(x + size / 2, y + 3, 1, size / 2, c);
    fill(x + size / 2, y + size / 2, size / 3, 1, c);
    return true;
  }
  return false;
}
static void present(bool full) {
  (void)full;
  if (failed || !surface.frame)
    return;
  risc_display_present_token_v1 token = 0;
  const risc_display_present_options_v1 options = {RISC_DISPLAY_PRESENT_DEFAULT,
                                                   RISC_DISPLAY_QUEUE_FIFO, 0};
  if (!display->submit(display->context, surface.frame, NULL, 0, &options,
                       &token)) {
    failed = true;
    return;
  }
  surface.frame = 0;
  uint32_t start = millis_now();
  for (unsigned n = 0; n < 10000 && !failed; ++n) {
    risc_display_present_status_v1 s = {0};
    if (!display->present_status(display->context, token, &s) ||
        s.state == RISC_DISPLAY_PRESENT_FAILED ||
        s.state == RISC_DISPLAY_PRESENT_SUPERSEDED) {
      failed = true;
      break;
    }
    if (s.state == RISC_DISPLAY_PRESENT_COMPLETE)
      return;
    if ((uint32_t)(millis_now() - start) >= 10000) {
      failed = true;
      break;
    }
    rt->yield_ms(1);
  }
  failed = true;
}
static bool poll(t5_app_input_t *out, uint32_t wait) {
  memset(out, 0, sizeof(*out));
  if (failed)
    return false;
  rt->yield_ms(wait);
  (void)millis_now();
  uint16_t x, y;
  if (portable_touch_tap(&touch, &x, &y)) {
    if (x >= info.width || y >= info.height)
      return !failed;
    if (y < 40 && x < 56) {
      out->exit_requested = true;
      out->buttons = T5_APP_BUTTON_BACK;
    } else if (list_mode && y >= info.height - 32)
      out->buttons = x < info.width / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN;
    else if (list_mode && y < 40 && x >= info.width - 56)
      out->buttons = T5_APP_BUTTON_CONFIRM;
    else {
      out->tapped = true;
      out->touch_x = x;
      out->touch_y = y;
    }
  }
  return !failed;
}
static bool refresh(void) { return portable_catalog_count <= 16; }
static uint32_t count(void) {
  return portable_catalog_count <= 16 ? portable_catalog_count : 0;
}
static bool get(uint32_t i, t5_app_manifest_t *out) {
  if (!out || i >= count())
    return false;
  *out = portable_catalog[i];
  return true;
}
static bool launch(uint32_t i) {
  return i < count() && rt->request_launch(portable_catalog[i].file_name);
}
static bool read_battery(t5_battery_state_t *out) {
  risc_battery_sample_v1 b = {0};
  if (!gauge || !out || !gauge->read(gauge->context, &b))
    return false;
  if (b.percent > 100 || (b.flags & RISC_BATTERY_PROFILE_MISSING))
    return false;
  memset(out, 0, sizeof(*out));
  strcpy(out->board_name, "Battery provider");
  out->available = out->gauge_ready = out->gauge_read_ok = 1;
  out->soc_percent = b.percent;
  out->gauge_voltage_mv = b.millivolts;
  out->charging = !!(b.flags & RISC_BATTERY_CHARGING);
  out->gauge_state =
      out->charging ? T5_BATTERY_GAUGE_CHARGE : T5_BATTERY_GAUGE_UNKNOWN;
  return true;
}
static void list(const t5_ui_chrome_t *chrome, const t5_ui_list_row_t *rows,
                 uint32_t n, int32_t selected) {
  list_mode = true;
  clear();
  text(8, 16, "BACK");
  label(52, 16, width() - 108, chrome->title);
  text(width() - 48, 16, "UPDATE");
  unsigned visible = (height() - 72) / 28;
  if (!visible)
    visible = 1;
  if (selected < 0)
    selected = 0;
  first_row = (unsigned)selected / visible * visible;
  last_rows = n;
  for (unsigned j = 0; j < visible && j + first_row < n; ++j) {
    int y = 48 + (int)j * 28;
    text(8, y, rows[j + first_row].title);
    text(width() / 2, y, rows[j + first_row].value);
  }
  label(0, height() - 18, width(), "PREVIOUS         NEXT");
  present(false);
}
static int32_t hit(int16_t x, int16_t y) {
  (void)x;
  if (y < 48)
    return -1;
  unsigned i = first_row + (y - 48) / 28;
  return i < last_rows ? (int32_t)i : -1;
}
static int32_t next(int32_t i, uint32_t n) {
  return n ? (int32_t)(((uint32_t)i + 1) % n) : 0;
}
static int32_t prev(int32_t i, uint32_t n) {
  return n ? (i > 0 ? i - 1 : (int32_t)n - 1) : 0;
}
static const t5_app_api_v1 app = {.abi_version = 1,
                                  .struct_size = sizeof(app),
                                  .screen_width = width,
                                  .screen_height = height,
                                  .clear = clear,
                                  .draw_text = text,
                                  .fill_rect = rect,
                                  .present = present,
                                  .poll = poll,
                                  .millis = millis_now,
                                  .installed_apps_refresh = refresh,
                                  .installed_apps_count = count,
                                  .installed_apps_get = get,
                                  .request_app_launch = launch,
                                  .draw_icon = icon,
                                  .draw_label = label,
                                  .fill_rounded_rect_tone = rounded};
static const t5_ui_api_v1 ui = {.api_version = 1,
                                .struct_size = sizeof(ui),
                                .render_list = list,
                                .hit_test = hit,
                                .next_index = next,
                                .previous_index = prev};
static const t5_battery_api_v1 battery = {1, sizeof(battery), read_battery};
const t5_app_api_v1 *t5_app_get_api(uint32_t v) {
  return v == 1 && !failed ? &app : NULL;
}
const t5_ui_api_v1 *t5_ui_get_api(uint32_t v) {
  return v == 1 && !failed ? &ui : NULL;
}
const t5_battery_api_v1 *t5_battery_get_api(uint32_t v) {
  return v == 1 && gauge ? &battery : NULL;
}
const t5_storage_api_v1 *t5_storage_get_api(uint32_t v) {
  (void)v;
  return NULL;
}
const t5_video_api_v1 *t5_video_get_api(uint32_t v) {
  (void)v;
  return NULL;
}
static int initialize(void) {
  rt = risc_runtime_get_api(1);
  if (!rt || rt->api_version != 1 ||
      rt->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE || !rt->acquire ||
      !rt->release || !rt->health || !rt->yield_ms || !rt->request_launch ||
      !rt->diagnostic)
    return -1;
  dg.struct_size = sizeof(dg);
  bg.struct_size = sizeof(bg);
  failed = false;
  if (!rt->acquire("display.output", 1, 0, &dg))
    return -1;
  display = dg.api;
  if (!display || display->api_version != 1 ||
      display->struct_size < sizeof(*display) || !display->get_info ||
      !display->acquire || !display->release || !display->submit ||
      !display->present_status)
    return -1;
  if (!display->get_info(display->context, &info) || info.width < 160 ||
      info.height < 160 || info.width > 1024 || info.height > 1024 ||
      !(info.supported_formats &
        RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)))
    return -1;
  if (!portable_touch_open(&touch, rt))
    return -1;
  if (rt->acquire("board.battery", 1, 0, &bg)) {
    gauge = bg.api;
    if (!gauge || gauge->api_version != 1 ||
        gauge->struct_size < sizeof(*gauge) || !gauge->read)
      return -1;
  }
  return 0;
}
__attribute__((visibility("default"))) void app_module_fini(void) {
  if (!rt)
    return;
  if (surface.frame)
    display->release(display->context, surface.frame);
  surface.frame = 0;
  if (!portable_touch_close(&touch, rt))
    rt->diagnostic("PORTABLE_APP error=touch-release");
  if (bg.api && !rt->release(&bg))
    rt->diagnostic("PORTABLE_APP error=battery-release");
  if (dg.api && !rt->release(&dg))
    rt->diagnostic("PORTABLE_APP error=display-release");
}

__attribute__((visibility("default"))) int app_module_init(void) {
  int status = initialize();
  if (status)
    app_module_fini();
  return status;
}

/* Minimal runtime does not export abs. App-local implementation; gesture
 * differences are bounded, with saturation guarding malformed coordinates. */
int abs(int value) {
  return value >= 0 ? value : (value == INT_MIN ? INT_MAX : -value);
}
