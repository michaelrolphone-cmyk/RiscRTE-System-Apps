/* Client-side adapter for existing shared apps; no board/chip/pin knowledge. */
#include "PortableApps.h"
#include "PortableTouch.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscDisplayOutputV1.h"
#include "T5BatteryApi.h"
#include "T5StorageApi.h"
#include "T5UiApi.h"
#include "T5VideoApi.h"
#ifdef PORTABLE_AUDIO_SESSION
#include "PortableAudioSession.h"
#ifndef PORTABLE_ALARM_CLIENT
#error "Audio lifecycle integration requires the foreground alarm client"
#endif
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
#include "PortableWifiView.h"
#ifdef PORTABLE_UPDATE_APP
#include "PortableUpdate.h"
#define portable_wifi_suspend portable_update_suspend
#define portable_wifi_resume portable_update_resume
#define portable_wifi_close portable_update_close
#define portable_wifi_services_safe portable_update_services_safe
#endif
#endif
#include <limits.h>
#include <stdlib.h>
static const risc_runtime_api_v1 *rt;
static risc_runtime_capability_v1 dg, bg;
static const risc_display_output_api_v1 *display;
static const risc_battery_gauge_api_v1 *gauge;
static portable_touch touch;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static bool failed, list_mode;
#ifdef PORTABLE_ALARM_CLIENT
#include "AlarmServiceV1.h"
static bool display_settled,alarm_pixels_valid,alarm_modal,native_sleep_retained;
bool portable_app_sleep_retained(void) { return native_sleep_retained; }
static uint16_t *alarm_pixels;
static bool alarm_foreground(bool *consumed);
#ifdef PORTABLE_APP_SLEEP_LOCAL
static const alarm_service_v1 *alarm_sleep_api(void);
#endif
static bool alarm_failure(void);
#endif
static unsigned first_row, last_rows;
#ifdef PORTABLE_SETTINGS_APP
#include "PortableRtcClock.h"
static bool back_exits_app = true, settings_editing;
static bool settings_view_poll(t5_app_input_t *out);
#endif
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
static bool back_exits_app = true;
static t5_app_contact_t app_contact;
static bool current_app_contact(t5_app_contact_t *out) {
  if(!out || failed)return false;
  *out=app_contact;return true;
}
static void set_back_exits(bool enabled) { back_exits_app=enabled; }
#endif
static uint32_t millis_now(void) {
  risc_runtime_health_v1 h = {.struct_size = sizeof(h)};
  if (!rt->health(&h)) {
    failed = true;
    return 0;
  }
  return h.uptime_ms;
}
/* Sample while a physical presentation is draining, rather than freezing
 * input for a whole frame. Keep only current movement and one completed tap;
 * overlapping unconsumed gestures cancel safely instead of replaying a backlog. */
static portable_touch_sample input_sample;
static bool input_pending;
static uint32_t input_sampled_at,last_poll_at;
static uint32_t navigation_pending;
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
static bool nu_gesture;
static int nu_start_x,nu_start_y;
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
#include "PortableAppSleep.h"
static uint32_t last_activity;
#endif
#ifdef PORTABLE_INPUT_NAVIGATION
#include "PortableNavigation.h"
#ifndef PORTABLE_INPUT_NAVIGATION_LOCAL
static risc_runtime_capability_v1 navigation_grant;
#endif
static const risc_input_navigation_api_v1 *navigation;
static bool navigation_neutral,navigation_ready;
static bool input_navigation_open(void) {
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
 navigation=portable_input_navigation_open(rt);
#else
 navigation_grant.struct_size=sizeof(navigation_grant);
 if(!rt->acquire("input.navigation",1,0,&navigation_grant))return false;
 navigation=navigation_grant.api;
#endif
 if(!navigation || navigation->api_version!=1 || navigation->struct_size<sizeof(*navigation) ||
    !navigation->poll || !navigation->foreground || !navigation->reset)return false;
 navigation_ready=true;
 const risc_input_foreground_v1 claims[]={{"input.touch.raw",1}};
 navigation_neutral=false;
 return navigation->foreground(navigation->context,claims,1) && navigation->reset(navigation->context);
}
static inline void input_navigation_reset(void) {
 navigation_pending=0;navigation_neutral=false;
 if(navigation_ready && !navigation->reset(navigation->context))failed=true;
}
static void input_navigation_close(void) {
 if(navigation_ready) {
  bool cleared=navigation->foreground(navigation->context,NULL,0),reset=navigation->reset(navigation->context);
  if(!cleared||!reset)rt->diagnostic("PORTABLE_APP error=navigation-reset");
  navigation_ready=false;
 }
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
 portable_input_navigation_close(rt);
#else
 if(navigation_grant.api && !rt->release(&navigation_grant))rt->diagnostic("PORTABLE_APP error=navigation-release");
#endif
 navigation=NULL;navigation_pending=0;
}
#endif
static void input_service(void) {
 portable_touch_sample next;portable_touch_read(&touch,&next);
 input_sampled_at=millis_now();
 if(input_pending && input_sample.released && next.down) {
  input_sample=(portable_touch_sample){.cancelled=true};touch.neutral=touch.down=false;
 } else if(!(input_pending && input_sample.released && next.valid && !next.down)) {
  if(input_pending && input_sample.down && next.down) {
   next.began|=input_sample.began;next.moved|=input_sample.moved;
   next.tap_eligible&=input_sample.tap_eligible;
  }
  input_sample=next;
 }
 input_pending=true;
#ifdef PORTABLE_APP_SLEEP_LOCAL
 if(next.valid && (next.down || next.began || next.released))last_activity=input_sampled_at;
#endif
#ifdef PORTABLE_INPUT_NAVIGATION
 if(navigation_ready) {
  risc_input_navigation_frame_v1 frame={0};
  if(!navigation->poll(navigation->context,&frame))navigation_neutral=false;
  else if(!navigation_neutral){if(!frame.buttons)navigation_neutral=true;}
  else navigation_pending|=frame.pressed;
#ifdef PORTABLE_APP_SLEEP_LOCAL
  if(frame.buttons || frame.pressed || frame.released)last_activity=input_sampled_at;
#endif
 }
#endif
}
static void input_take(portable_touch_sample *out) {
 if(!input_pending)input_service();
 *out=input_sample;input_pending=false;
}
static void input_navigation_take(t5_app_input_t*out) {
 out->buttons|=navigation_pending & (T5_APP_BUTTON_BACK|T5_APP_BUTTON_CONFIRM|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN);
 navigation_pending=0;
}
static uint16_t *previous_pixels;
static bool previous_valid;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
#include "PortableTransition.h"
/* Deployment opt-in ONLY: acquire returns the exact last completed RGB565
 * frame from a persistent provider. It is not a display.output@1 guarantee.
 * See docs/PORTABLE_TRANSITIONS.md. Copy before clear, never retain a lease
 * or an outgoing app pointer. Unsupported builds do not compile this path. */
static uint16_t *handoff_old,*handoff_scratch;
static bool handoff_pending,handoff_active,handoff_first;
static uint32_t handoff_started;
bool springboard_transition_active(void) { return handoff_active && !failed; }
static void handoff_finish(void) {
 free(handoff_old);free(handoff_scratch);handoff_old=handoff_scratch=NULL;handoff_active=handoff_pending=handoff_first=false;
}
#endif
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
static void clear_color(uint16_t color) {
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
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_pending) {
    handoff_pending=false;
    if(info.width<=PORTABLE_TRANSITION_MAX_SIDE &&
       info.height<=PORTABLE_TRANSITION_MAX_SIDE &&
       info.nominal_refresh_millihz>=20000 &&
       !(info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) &&
       info.typical_present_latency_us<=50000) {
      size_t bytes=(size_t)info.width*info.height*2;
      handoff_old=malloc(bytes);
      handoff_scratch=handoff_old?malloc(bytes):NULL;
      if(!handoff_scratch){free(handoff_old);handoff_old=NULL;}
      else {
        for(unsigned y=0;y<info.height;y++)memcpy(handoff_old+(size_t)y*info.width,
          (uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,info.width*2);
        handoff_first=true;handoff_active=true;
#ifdef PORTABLE_HANDOFF_EAGER_MS
        /* The outgoing image is already visible. Count initial drawing time
         * and start with incoming content, rather than retransmitting an
         * identical alpha-zero frame before beginning the transition. */
        handoff_started=millis_now();handoff_first=false;
#endif
      }
    }
  }
#endif
  fill(0, 0, width(), height(), color);
}
static void clear(void) { clear_color(0xffff); }
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
#include "nova.inc"
#ifdef PORTABLE_NOVA_UI
#include "nova_ui.inc"
#endif
static bool icon(int32_t x, int32_t y, const char *name, uint8_t size, bool black) {
  /* Preserve the existing call contract; genuine glyph raster, no handmade FA IDs. */
  if (black) {
    /* np_icon normally draws white; invert its bounded output on a white tile. */
    if (!name || size < 1 || size > 96) return false;
    const rpi_glyph *g = NULL;
    for (unsigned i = 0; i < sizeof(rpi_icons)/sizeof(rpi_icons[0]); ++i)
      if (!strcmp(rpi_icons[i].name, name)) { g = &rpi_icons[i]; break; }
    if (!g) return false;
    int max = g->width > g->height ? g->width : g->height;
    int w = g->width*size/max, h = g->height*size/max;
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) {
      unsigned n = (unsigned)(j*g->height/h)*g->width+(unsigned)(i*g->width/w);
      unsigned a = (g->bits[n/4] >> (6-2*(n%4))) & 3;
      np_pixel(x+(size-w)/2+i, y+(size-h)/2+j, 0, a*85);
    }
    return true;
  }
  return np_icon(x+size/2,y+size/2,size,name,255);
}
static void present(bool full) {
  (void)full;
  if (failed || !surface.frame)
    return;
  risc_display_present_token_v1 token = 0;
  risc_display_rect_v1 damage={0};size_t damage_count=0;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_active) {
    uint32_t now=millis_now();
    if(failed)return;
    /* Start at the first submission, not during allocation/initial rendering.
     * The first frame is byte-exact outgoing content even on a busy target. */
    if(handoff_first){handoff_started=now;handoff_first=false;}
    #ifdef PORTABLE_HANDOFF_EAGER_MS
    _Static_assert(PORTABLE_HANDOFF_EAGER_MS > 0 && PORTABLE_HANDOFF_EAGER_MS <= 180,
                   "Bounded eager handoff duration required");
    uint32_t elapsed=now-handoff_started;
    unsigned alpha=elapsed>=PORTABLE_HANDOFF_EAGER_MS?256u:elapsed*256u/PORTABLE_HANDOFF_EAGER_MS;
    if(!alpha)alpha=1; /* Even a zero-cost host paint must not duplicate old RAM. */
#else
    unsigned alpha=portable_transition_alpha(now-handoff_started);
#endif
    if(alpha==256u || !portable_transition_rgb565(surface.pixels,surface.stride_bytes,
        handoff_old,info.width*2,handoff_scratch,(size_t)info.width*info.height*2,
        info.width,info.height,alpha))handoff_finish();
  }
#endif
  if(previous_pixels
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
     && !handoff_active
#endif
  ) {
    unsigned first=info.height,last=0;
    if(previous_valid)for(unsigned y=0;y<info.height;y++) {
      if(memcmp(previous_pixels+(size_t)y*info.width,(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,info.width*2)) {
        if(first==info.height)first=y;
        last=y+1;
      }
    }
    if(previous_valid && first==info.height){display->release(display->context,surface.frame);surface.frame=0;return;}
    if(previous_valid){damage=(risc_display_rect_v1){0,(int32_t)first,info.width,last-first};damage_count=1;}
    for(unsigned y=0;y<info.height;y++)memcpy(previous_pixels+(size_t)y*info.width,(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,info.width*2);
    previous_valid=false;
  }
  const risc_display_present_options_v1 options = {RISC_DISPLAY_PRESENT_DEFAULT,
                                                   RISC_DISPLAY_QUEUE_FIFO, 0};
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=false;
  if(!alarm_modal) {
    alarm_pixels_valid=false;
    for(unsigned y=0;y<info.height;y++)memcpy(alarm_pixels+(size_t)y*info.width,
      (uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,info.width*2);
  }
#endif
  if (!display->submit(display->context, surface.frame, damage_count?&damage:NULL, damage_count, &options,
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
    if (s.state == RISC_DISPLAY_PRESENT_COMPLETE){
#ifdef PORTABLE_ALARM_CLIENT
      display_settled=true;if(!alarm_modal)alarm_pixels_valid=true;
#endif
      previous_valid=previous_pixels!=NULL;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
      if(handoff_active)previous_valid=false;
#endif
      return;
    }
    if ((uint32_t)(millis_now() - start) >= 10000) {
      failed = true;
      break;
    }
    if((uint32_t)(millis_now()-input_sampled_at)>=16)input_service();
    rt->yield_ms(1);
  }
  failed = true;
}
#ifdef PORTABLE_APP_SLEEP_LOCAL
static bool idle_sleep(void) {
  /* Existing app stack, editor draft and private storage grants stay live.
   * No handoff, unload or settings grant is introduced by idle sleeping. */
  if(surface.frame)return true;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_active || handoff_pending)return true;
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  /* A radio drain refusal is recoverable app UI, not a native sleep entry.
   * Keep touch/navigation live so the user can explicitly retry cleanup. */
  if(!portable_wifi_suspend()){last_activity=millis_now();return !failed;}
#endif
#ifdef PORTABLE_AUDIO_SESSION
  /* Close app-owned audio before sleep preparation can call storage or alarm
   * output. Wake never restarts capture/playback without a fresh user action. */
  if(!portable_audio_suspend()){failed=true;return false;}
#endif
  if(!portable_touch_close(&touch,rt)){failed=true;return false;}
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();
#endif
  if(failed)return false;
  input_pending=false;navigation_pending=0;
  rt->diagnostic("PORTABLE_APP sleep=idle");
#ifdef PORTABLE_ALARM_CLIENT
  int status=portable_app_alarm_sleep(rt,display,gauge,alarm_sleep_api());
#else
  int status=portable_app_sleep(rt,display,gauge);
#endif
#ifdef PORTABLE_ALARM_CLIENT
  if(status==-2){native_sleep_retained=true;failed=true;return false;}
#endif
  if(status<0){failed=true;return false;}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  portable_wifi_resume();
#endif
  if(!portable_touch_open(&touch,rt)){failed=true;return false;}
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();
#endif
  input_sample=(portable_touch_sample){0};input_pending=false;navigation_pending=0;
  last_activity=last_poll_at=input_sampled_at=millis_now();previous_valid=false;
  rt->diagnostic(status?"PORTABLE_APP sleep=resumed":"PORTABLE_APP sleep=refused");
  return !failed;
}
#endif
static bool poll_input(t5_app_input_t *out, uint32_t wait) {
  memset(out, 0, sizeof(*out));
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
  if (failed)
    return false;
#ifdef PORTABLE_ALARM_CLIENT
  bool consumed=false;
  if(!alarm_foreground(&consumed))return false;
  if(consumed)return true;
#endif
  uint32_t now=millis_now(),spent=now-last_poll_at;
  rt->yield_ms(spent<wait?wait-spent:1);
  last_poll_at=millis_now();
  input_service();
#ifdef PORTABLE_ALARM_CLIENT
  if(!alarm_foreground(&consumed))return false;
  if(consumed)return true;
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
  if(!failed && (uint32_t)(millis_now()-last_activity)>=60000u &&
     !navigation_pending && !(input_pending && (input_sample.down || input_sample.released))) {
    if(!idle_sleep())return false;
    return !failed; /* Waking crown/contact never becomes an app action. */
  }
#endif
  if (nova_mode) { np_poll(out); input_navigation_take(out); return !failed; }
#ifdef PORTABLE_SETTINGS_APP
  if (settings_view_poll(out)) return !failed;
#endif
  input_navigation_take(out);
  if(out->buttons&T5_APP_BUTTON_BACK)return !failed;
  portable_touch_sample sample;input_take(&sample);
  uint16_t x=sample.x,y=sample.y;
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  if(sample.valid && !sample.cancelled && sample.tap_eligible && sample.down && x<info.width && y<info.height)
    app_contact=(t5_app_contact_t){true,(int16_t)x,(int16_t)y};
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
  if(sample.cancelled || !sample.valid)nu_gesture=false;
  if(sample.began){nu_gesture=true;nu_start_x=x;nu_start_y=y;}
  if(sample.released && nu_gesture) {
    nu_gesture=false;
    int dx=(int)x-nu_start_x,dy=(int)y-nu_start_y;
    if(sample.moved && !sample.cancelled) {
      if(dy>=28 && dy>abs(dx)*2)out->buttons|=T5_APP_BUTTON_UP;
      else if(dy<=-28 && -dy>abs(dx)*2)out->buttons|=T5_APP_BUTTON_DOWN;
      else if(dx>=36 && dx>abs(dy)*2)out->buttons|=T5_APP_BUTTON_BACK;
      return !failed;
    }
  }
#endif
  if (sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled) {
    if (x >= info.width || y >= info.height)
      return !failed;
    if (
#ifdef PORTABLE_APP_OWNS_TOUCH_CHROME
        false /* App tabs and drag gestures own the whole touch surface. */
#elif defined(PORTABLE_NOVA_UI)
        y>=4 && y<48 && x>=8 && x<52
#else
        y<40 && x<56
#endif
       ) {
#ifdef PORTABLE_SETTINGS_APP
      out->exit_requested = back_exits_app;
#else
      out->exit_requested = true;
#endif
      out->buttons = T5_APP_BUTTON_BACK;
    }
#ifdef PORTABLE_NOVA_UI
    else if(list_mode && y>=184 && y<228) {
      if(x>=12 && x<64)out->buttons=T5_APP_BUTTON_UP;
      else if(x>=72 && x<168)out->buttons=T5_APP_BUTTON_CONFIRM;
      else if(x>=176 && x<228)out->buttons=T5_APP_BUTTON_DOWN;
    }
#endif
#ifndef PORTABLE_NOVA_UI
    else if (list_mode && y >= info.height - 32)
#ifdef PORTABLE_SETTINGS_APP
      out->buttons = settings_editing
                         ? (x < info.width / 2 ? T5_APP_BUTTON_LEFT : T5_APP_BUTTON_RIGHT)
                         : (x < info.width / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN);
#else
      out->buttons = x < info.width / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN;
#endif
    else if (list_mode && y < 40 && x >= info.width - 56)
      out->buttons = T5_APP_BUTTON_CONFIRM;
#endif
    else {
      out->tapped = true;
      out->touch_x = x;
      out->touch_y = y;
    }
  }
  return !failed;
}
/* Explicit deployment return destination. Queue only a deliberate root exit,
 * while this invocation is active; nested Settings Back remains app-owned.
 * Launch requests and error/health exits never acquire a synthetic return. */
static bool poll(t5_app_input_t *out, uint32_t wait) {
  bool ok=poll_input(out,wait);
#ifdef PORTABLE_ALARM_CLIENT
  if(!ok)return alarm_failure();
#endif
#ifdef PORTABLE_RETURN_APP
  bool returning=out->exit_requested;
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  returning|=back_exits_app && !!(out->buttons&T5_APP_BUTTON_BACK);
#elif !defined(PORTABLE_SETTINGS_APP)
  returning|=!!(out->buttons&T5_APP_BUTTON_BACK);
#endif
  if(ok && returning) {
#ifdef PORTABLE_AUDIO_SESSION
    if(!portable_audio_suspend())return alarm_failure();
#endif
    if(!rt->request_launch(PORTABLE_RETURN_APP)) {
      rt->diagnostic("PORTABLE_APP error=return-request");failed=true;return false;
    }
    out->exit_requested=true;
  }
#endif
  return ok;
}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
#include "wifi_view.inc"
#endif
#ifdef PORTABLE_ALARM_CLIENT
#include "alarm.inc"
#endif
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
#ifdef PORTABLE_ALARM_CLIENT
  bool consumed=false;
  if(!alarm_foreground(&consumed) || consumed)return false;
#endif
  return !failed &&
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
      !handoff_active &&
#endif
      !(input_pending && (input_sample.down || input_sample.cancelled)) && !(navigation_pending&T5_APP_BUTTON_BACK) && i < count() && rt->request_launch(portable_catalog[i].file_name);
}
static bool read_battery(t5_battery_state_t *out) {
  risc_battery_sample_v1 b = {0};
  if (!gauge || !out || !gauge->read(gauge->context, &b))
    return false;
  const bool known_soc = b.percent <= 100 && !(b.flags & RISC_BATTERY_PROFILE_MISSING);
  memset(out, 0, sizeof(*out));
  strcpy(out->board_name, "Battery provider");
  out->available = out->gauge_ready = out->gauge_read_ok = 1;
  out->soc_percent = known_soc ? b.percent : UINT16_MAX;
  out->gauge_voltage_mv = b.millivolts;
  out->charging = !!(b.flags & RISC_BATTERY_CHARGING);
  out->gauge_state =
      out->charging ? T5_BATTERY_GAUGE_CHARGE : T5_BATTERY_GAUGE_UNKNOWN;
  return true;
}
static void list(const t5_ui_chrome_t *chrome, const t5_ui_list_row_t *rows,
                 uint32_t n, int32_t selected) {
#ifdef PORTABLE_NOVA_UI
  portable_nova_begin();list_mode=true;
  portable_nova_header(chrome->title);
  if(selected<0)selected=0;
  first_row=(unsigned)selected/2*2;last_rows=n;
  for(unsigned j=0;j<2 && first_row+j<n;j++)
    portable_nova_row(16,56+(int)j*60,208,54,rows[first_row+j].title,rows[first_row+j].value,(int)(first_row+j)==selected);
  portable_nova_center(2,16,169,208,chrome->status,NOVA_CAP);
  portable_nova_button(12,184,52,44,"Prev",false);
  portable_nova_button(72,184,96,44,"Update",false);
  portable_nova_button(176,184,52,44,"Next",false);
  present(false);
#else
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
#endif
}
static int32_t hit(int16_t x, int16_t y) {
#ifdef PORTABLE_NOVA_UI
  if(x<16 || x>=224)return -1;
  for(unsigned row=0;row<2;row++)if(y>=56+(int)row*60 && y<110+(int)row*60)
    return first_row+row<last_rows?(int32_t)(first_row+row):-1;
  return -1;
#else
  (void)x;
  if (y < 48)
    return -1;
  unsigned i = first_row + (y - 48) / 28;
  return i < last_rows ? (int32_t)i : -1;
#endif
}
static int32_t next(int32_t i, uint32_t n) {
  return n ? (int32_t)(((uint32_t)i + 1) % n) : 0;
}
static int32_t prev(int32_t i, uint32_t n) {
  return n ? (i > 0 ? i - 1 : (int32_t)n - 1) : 0;
}
#ifdef PORTABLE_SETTINGS_APP
#include "settings.inc"
#endif
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
#if defined(PORTABLE_SETTINGS_APP) || defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
                                  .set_back_exits_app = set_back_exits,
#endif
#ifdef PORTABLE_SETTINGS_APP
                                  .settings_category_count = settings_categories,
                                  .settings_category_get = settings_category,
                                  .settings_count = settings_count,
                                  .settings_get = settings_get,
                                  .settings_activate = settings_activate,
                                  .settings_render = settings_render,
                                  .settings_touch = settings_touch,
#endif
                                  .installed_apps_refresh = refresh,
                                  .installed_apps_count = count,
                                  .installed_apps_get = get,
                                  .request_app_launch = launch,
                                  .draw_icon = icon,
                                  .draw_label = label,
                                  .fill_rounded_rect_tone = rounded,
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
                                  .touch_contact = current_app_contact,
#endif
};
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
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=true;alarm_pixels_valid=alarm_modal=native_sleep_retained=false;alarm_pixels=NULL;
  alarm_error_seen=alarm_failed_cleaned=false;memset(&alarms,0,sizeof(alarms));
#endif
  nova_mode = false;
#ifdef PORTABLE_APP_OWNS_TOUCH_CHROME
  back_exits_app=true;
#ifndef PORTABLE_SETTINGS_APP
  app_contact=(t5_app_contact_t){0};
#endif
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
  nu_gesture=false;
#endif
  list_mode = false;input_pending=false;navigation_pending=0;previous_valid=false;
  input_sampled_at=last_poll_at=0;previous_pixels=NULL;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_old=handoff_scratch=NULL;handoff_pending=false;handoff_active=false;handoff_first=false;
#endif
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
#ifdef PORTABLE_NOVA_UI
  if(info.width!=240 || info.height!=240)return -1;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  alarm_pixels=malloc((size_t)info.width*info.height*2);
  if(!alarm_pixels || !portable_alarm_open(&alarms,rt))return -1;
#endif
  if (!portable_touch_open(&touch, rt))return -1;
#ifndef PORTABLE_FORCE_FULL_FRAMES
  if((info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE) && info.width<=320 && info.height<=320)
    previous_pixels=malloc((size_t)info.width*info.height*2); /* optional; full-frame fallback */
#endif
#if defined(PORTABLE_INPUT_NAVIGATION) && !defined(PORTABLE_SETTINGS_APP)
  if(!input_navigation_open())return -1;
#endif
  if (rt->acquire("board.battery", 1, 0, &bg)) {
    gauge = bg.api;
    if (!gauge || gauge->api_version != 1 ||
        gauge->struct_size < sizeof(*gauge) || !gauge->read)
      return -1;
  }
#ifdef PORTABLE_SETTINGS_APP
  if (!settings_open()) return -1;
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
  last_activity=millis_now();
#endif
  return failed?-1:0;
}
__attribute__((visibility("default"))) void app_module_fini(void) {
  if (!rt)
    return;
#ifdef PORTABLE_ALARM_CLIENT
  if(native_sleep_retained)return; /* Runtime normally blocks fini first. */
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  if(!portable_wifi_close()) {
    rt->diagnostic("WIFI cleanup-unconfirmed; invocation retained");
    for(;;)rt->yield_ms(50);
  }
#endif
#ifdef PORTABLE_AUDIO_SESSION
  if(!portable_audio_suspend()) {
    rt->diagnostic("AUDIO cleanup-unconfirmed; invocation retained");
    for(;;)rt->yield_ms(50);
  }
#endif
#ifdef PORTABLE_ALARM_CLIENT
  if(alarms.api && !alarm_failed_cleaned &&
      (failed || !display_settled || !portable_alarm_status(&alarms) || portable_alarm_owned(&alarms)) &&
      !portable_alarm_failure_stop(&alarms)) {
    rt->diagnostic("ALARM fini output-stop-unconfirmed; invocation retained");
    for(;;)rt->yield_ms(50);
  }
  if(!portable_alarm_close(&alarms,rt))rt->diagnostic("ALARM error=release");
  free(alarm_pixels);alarm_pixels=NULL;alarm_pixels_valid=false;
#endif
  if (surface.frame)
    display->release(display->context, surface.frame);
  surface.frame = 0;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_finish();
#endif
  np_close();free(previous_pixels);previous_pixels=NULL;previous_valid=false;
#if defined(PORTABLE_INPUT_NAVIGATION) && !defined(PORTABLE_SETTINGS_APP)
  input_navigation_close();
#endif
  if (!portable_touch_close(&touch, rt))
    rt->diagnostic("PORTABLE_APP error=touch-release");
#ifdef PORTABLE_SETTINGS_APP
  settings_close();
#endif
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
