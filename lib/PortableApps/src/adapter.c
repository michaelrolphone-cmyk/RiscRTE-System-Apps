#ifdef PORTABLE_FILE_BROWSER_APP
#include "PortableFileBrowser.h"
#endif
/* Client-side adapter for existing shared apps; no board/chip/pin knowledge. */
#include "PortableApps.h"
#include "PortableTouch.h"
#if defined(PORTABLE_DESK_CLOCK) || defined(PORTABLE_SETTINGS_X4_DESK_CLOCK)
#ifndef PORTABLE_PAPER_PREFERENCES
#define PORTABLE_PAPER_PREFERENCES
#endif
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
#include "PortableReaderPreferences.h"
static bool paper_flip_ui,paper_orientation_dirty,paper_preferences_retained;
#ifndef PORTABLE_DESK_CLOCK_SPARSE_START
static risc_runtime_capability_v1 paper_preferences_grant;
#endif
static void paper_orient_input(portable_touch_sample *sample);
#endif
#include "RiscBatteryGaugeV1.h"
#include "RiscDisplayOutputV1.h"
#ifdef PORTABLE_DESK_CLOCK
#include "PortableDeskClockApp.h"
#include <RiscDisplayOutputPowerV1.h>
static bool desk_present_complete;
#ifdef PORTABLE_QUICK_RADIOS
static bool desk_radios_loaded;
#endif
#endif
#include "T5BatteryApi.h"
#include "T5StorageApi.h"
#include "T5UiApi.h"
#include "T5VideoApi.h"
#ifdef PORTABLE_RADIO_SESSION
#include "PortableRadioSession.h"
#ifndef PORTABLE_ALARM_CLIENT
#error "Radio lifecycle integration requires the foreground alarm client"
#endif
#endif
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
#ifdef PORTABLE_APP_LAUNCH_GUARD
/* The application may consume one navigation attempt to resolve an edit.
 * This is client policy, not native authority or an uncertain cleanup state. */
#include "PortableAppLaunchGuard.h"
#define app_allows_launch(destination) portable_app_before_launch(destination)
#else
#define app_allows_launch(destination) (true)
#endif
static const risc_runtime_api_v1 *rt;
static risc_runtime_capability_v1 dg, bg;
static const risc_display_output_api_v1 *display;
static const risc_battery_gauge_api_v1 *gauge;
static portable_touch touch;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
#ifndef PORTABLE_DISPLAY_ROTATION
#define PORTABLE_DISPLAY_ROTATION 0
#endif
#if PORTABLE_DISPLAY_ROTATION != 0 && PORTABLE_DISPLAY_ROTATION != 90
#error "Portable display supports explicit native or 90-degree portrait mapping"
#endif
static bool paper_rotated;
static bool failed, list_mode;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
#if !defined(PORTABLE_DESK_CLOCK) || !defined(PORTABLE_ALARM_CLIENT) || !defined(PORTABLE_APP_SLEEP_LOCAL)
#error "Sparse startup requires the desk clock, alarm closure and local sleep hook"
#endif
#if defined(PORTABLE_SETTINGS_APP) || defined(PORTABLE_INPUT_NAVIGATION_LOCAL) || defined(PORTABLE_RADIO_SESSION) || defined(PORTABLE_AUDIO_SESSION) || defined(PORTABLE_FILE_BROWSER_APP) || defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
#error "Sparse startup is an explicit desk-clock deployment contract"
#endif
/* The mode is adapter-owned state, not a claim about arbitrary hardware.
 * Only the selected deployment may rely on TIMER's unopened foreground set. */
enum { DESK_COLD, DESK_INITIALIZED, DESK_STARTING, DESK_TIMER,
       DESK_FOREGROUND, DESK_FAILED, DESK_FINALIZED };
static unsigned desk_phase;
#ifndef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
#error "Sparse Clock requires the canonical Runtime invocation-retention SDK"
#endif
static bool desk_touch_neutral,desk_native_fenced;
static volatile risc_display_surface_v1 desk_retained_surface;
static const risc_runtime_api_v1 *desk_runtime;
static risc_runtime_api_v1 desk_guarded_runtime;
#ifdef PORTABLE_QUICK_RADIOS
static risc_runtime_api_v1 desk_quick_runtime;
static bool desk_quick_acquire(const char *,uint32_t,uint64_t,risc_runtime_capability_v1 *);
#endif
static bool desk_acquire(const char *,uint32_t,uint64_t,risc_runtime_capability_v1 *);
static bool desk_release(risc_runtime_capability_v1 *);
static bool desk_guard_health(risc_runtime_health_v1 *);
static void desk_guard_yield(uint32_t);
static bool desk_guard_diagnostic(const char *);
static bool desk_guard_launch(const char *);
#endif
#ifdef PORTABLE_QUICK_ACTIONS
#include "PortableQuickSession.h"
#include "PortableQuickRender.h"
static pqa_session quick;
#ifdef PORTABLE_QUICK_RADIOS
#include "PortableQuickRadios.h"
static pqa_radios quick_radios;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
static bool desk_radios_load(pqa_radios *,pqa_state *,const risc_runtime_api_v1 *);
static bool desk_radios_apply(pqa_radios *,pqa_state *,const risc_runtime_api_v1 *,uint32_t);
static bool desk_radios_suspend(const risc_runtime_api_v1 *);
static bool desk_radios_resume(pqa_radios *,pqa_state *,const risc_runtime_api_v1 *);
#define pqa_radios_load desk_radios_load
#define pqa_radios_apply desk_radios_apply
#define pqa_radios_suspend desk_radios_suspend
#define pqa_radios_resume desk_radios_resume
#endif
#endif
static uint16_t *quick_background;
static bool quick_modal,quick_launch_pending,quick_replay_pending,quick_replay_delivery;
static bool quick_foreground(bool *consumed);
static bool quick_interrupt(void);
unsigned portable_quick_brightness(void) {return quick.brightness;}
#endif
#ifdef PORTABLE_ALARM_CLIENT
#include "AlarmServiceV1.h"
static bool display_settled,alarm_pixels_valid,alarm_modal,native_sleep_retained;
bool portable_app_sleep_retained(void) { return native_sleep_retained; }
static uint16_t *alarm_pixels;
static bool alarm_foreground(bool *consumed);
#if defined(PORTABLE_APP_SLEEP_LOCAL) || defined(PORTABLE_QUICK_ACTIONS)
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
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || desk_phase<DESK_STARTING || desk_phase>=DESK_FAILED)return 0;
#endif
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
static bool home_pending,crown_pending,handoff_requested;
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
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(navigation_ready && !navigation->reset(navigation->context))portable_desk_adapter_retain();
#else
 if(navigation_ready && !navigation->reset(navigation->context))failed=true;
#endif
}
#ifndef PORTABLE_DESK_CLOCK_SPARSE_START
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
#endif
static void input_service(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(failed || desk_phase!=DESK_FOREGROUND || !touch.subscription)return;
#endif
 portable_touch_sample next;portable_touch_read(&touch,&next);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 /* Discard replayed events and the complete initial held contact/Home cycle,
  * including drag gestures. A fresh neutral snapshot arms the next contact. */
 if(!desk_touch_neutral) {
  if(next.valid && !next.down && !touch.home_down)desk_touch_neutral=true;
  touch.neutral=desk_touch_neutral;touch.down=false;
  next=(portable_touch_sample){.valid=desk_touch_neutral};
 }
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
 paper_orient_input(&next);
#endif
 input_sampled_at=millis_now();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(failed)return;
#endif
 if(next.home_pressed) {
  home_pending=true;navigation_pending|=T5_APP_BUTTON_BACK;
  next=(portable_touch_sample){.valid=true,.cancelled=true};touch.neutral=touch.down=false;
 }
#ifdef PORTABLE_APP_SLEEP_LOCAL
 if(next.valid && (next.down || next.began || next.released))last_activity=input_sampled_at;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 if(!alarm_modal) {
  bool reserved=pqa_input(&quick.ui,input_sampled_at,next.valid&&!next.cancelled,
      next.down?1u:0u,touch.contact_id,
      quick.ui.paper?(int)next.x*480/(int)(paper_rotated?info.height:info.width):next.x,
      quick.ui.paper?(int)next.y*800/(int)(paper_rotated?info.width:info.height):next.y,
      next.tap_eligible&&(quick.ui.paper || (info.width==240&&info.height==240)));
  if(quick.ui.paper && reserved && next.valid && !next.down && !next.cancelled &&
     !pqa_visible(&quick.ui) && !pqa_capture(&quick.ui))reserved=false;
  if(reserved) {
   input_pending=false;input_sample=(portable_touch_sample){0};
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
   nu_gesture=false;
#endif
  } else if(quick.ui.route==PQA_REPLAY) {
   quick_replay_pending=true;next.began=true;
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
   nu_gesture=true;nu_start_x=quick.ui.start_x;nu_start_y=quick.ui.start_y;
#endif
  }
  if(reserved)next=(portable_touch_sample){0};
 }
#endif
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
  else {navigation_pending|=frame.pressed;
    if(frame.pressed&RISC_NAV_HOME){crown_pending=true;navigation_pending|=T5_APP_BUTTON_BACK;}
  }
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
static uint8_t *paper_previous;
static bool paper_previous_valid;
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
static int32_t width(void) { return paper_rotated?info.height:info.width; }
static int32_t height(void) { return paper_rotated?info.width:info.height; }
#ifdef PORTABLE_PAPER_PREFERENCES
static void paper_orient_input(portable_touch_sample *sample) {
  if(paper_flip_ui && sample->valid && !sample->cancelled && (sample->down || sample->released)) {
    if(sample->x>=width() || sample->y>=height()) {
      *sample=(portable_touch_sample){.cancelled=true};touch.neutral=touch.down=false;return;
    }
    sample->x=(uint16_t)(width()-1-sample->x);sample->y=(uint16_t)(height()-1-sample->y);
  }
}
#endif
static void native_point(int *x,int *y) {
#ifdef PORTABLE_PAPER_PREFERENCES
  if(paper_flip_ui){*x=width()-1-*x;*y=height()-1-*y;}
#endif
  if(paper_rotated){int old=*x;*x=*y;*y=(int)info.height-1-old;}
}
static void fill(int x, int y, int w, int h, uint16_t color) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed)return;
#endif
  if (!surface.frame || w <= 0 || h <= 0)
    return;
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w, y1 = y + h;
  if (x1 > width())
    x1 = width();
  if (y1 > height())
    y1 = height();
  for (int j = y0; j < y1; ++j)
    for (int i = x0; i < x1; ++i) {
      int px=i,py=j;native_point(&px,&py);
      uint8_t *p=(uint8_t *)surface.pixels+(size_t)py*surface.stride_bytes;
      if(surface_format==RISC_DISPLAY_FORMAT_MONO1) {
        unsigned luminance=((color>>11)&31)*299/31+((color>>5)&63)*587/63+(color&31)*114/31;
        uint8_t bit=(uint8_t)(0x80u>>(px&7));
        if(luminance<500)p[px/8]|=bit;else p[px/8]&=(uint8_t)~bit;
      } else {p[i*2]=(uint8_t)color;p[i*2+1]=(uint8_t)(color>>8);}
    }
}
static void display_failure(const char *detail) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  (void)detail;portable_desk_adapter_retain();return;
#endif
  if(!failed && surface_format==RISC_DISPLAY_FORMAT_MONO1 &&
     (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE))rt->diagnostic(detail);
  failed=true;
}
static void clear_color(uint16_t color) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(desk_phase!=DESK_TIMER && desk_phase!=DESK_FOREGROUND)return;
#endif
  if (failed)
    return;
  if (surface.frame) {
    display->release(display->context, surface.frame);
    surface.frame = 0;
  }
  if (!display->acquire(display->context, surface_format,
                        &surface)) {
    display_failure("PORTABLE_APP error=display-acquire");
    return;
  }
  if (!surface.frame || !surface.pixels || surface.width != info.width ||
      surface.height != info.height ||
      surface.pixel_format != surface_format ||
      surface.stride_bytes < (surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2) ||
      surface.stride_bytes > UINT32_MAX / info.height ||
      surface.size_bytes < surface.stride_bytes * info.height) {
    display_failure("PORTABLE_APP error=display-surface");
    return;
  }
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_pending) {
    handoff_pending=false;
    if(surface_format==RISC_DISPLAY_FORMAT_RGB565 && info.width<=PORTABLE_TRANSITION_MAX_SIDE &&
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
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
#define paper_presentation_get desk_paper_presentation_get_unchecked
#endif
#include "paper.inc"
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
#undef paper_presentation_get
const paper_presentation *paper_presentation_get(void) {
 if(!portable_desk_adapter_ready() || !pp_enabled())return NULL;
 /* The selected Clock owns native realtime in both modes. Never bind the
  * legacy toolbar RTC merely to obtain presentation callbacks. */
 nova_mode=true;memset(&nova_contact,0,sizeof(nova_contact));return &pp_view;
}
#endif
static void present(bool full) {
#ifdef PORTABLE_PAPER_PREFERENCES
  if(paper_orientation_dirty)full=true;
#endif
#ifdef PORTABLE_DESK_CLOCK
  desk_present_complete=false;
#endif
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
  if(paper_previous) {
    unsigned row_bytes=(info.width+7)/8,left=row_bytes,right=0,top=info.height,bottom=0;
    if(paper_previous_valid && !full)for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<row_bytes;x++) {
      uint8_t value=((uint8_t*)surface.pixels)[(size_t)y*surface.stride_bytes+x];
      if(value!=paper_previous[(size_t)y*row_bytes+x]) {
        if(x<left)left=x;
        if(x+1>right)right=x+1;
        if(y<top)top=y;
        if(y+1>bottom)bottom=y+1;
      }
    }
    if(paper_previous_valid && !full && top==info.height){display->release(display->context,surface.frame);surface.frame=0;
#ifdef PORTABLE_DESK_CLOCK
      desk_present_complete=true;
#endif
      return;}
    if(paper_previous_valid && !full) {
      unsigned xa=info.damage_x_alignment?info.damage_x_alignment:1,ya=info.damage_y_alignment?info.damage_y_alignment:1;
      unsigned wa=info.damage_width_alignment?info.damage_width_alignment:1,ha=info.damage_height_alignment?info.damage_height_alignment:1;
      unsigned x=left*8/xa*xa,y=top/ya*ya,w=((right*8-x+wa-1)/wa)*wa,h=((bottom-y+ha-1)/ha)*ha;
      if(x+w<=info.width && y+h<=info.height){damage=(risc_display_rect_v1){(int32_t)x,(int32_t)y,w,h};damage_count=1;}
    }
    for(unsigned y=0;y<info.height;y++)memcpy(paper_previous+(size_t)y*row_bytes,(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,row_bytes);
    paper_previous_valid=false;
  }
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
  const risc_display_present_options_v1 options = {
    full && (info.flags&RISC_DISPLAY_INFO_CLEAN_PRESENT)?RISC_DISPLAY_PRESENT_CLEAN:
      surface_format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_PRESENT_QUALITY:RISC_DISPLAY_PRESENT_DEFAULT,
    RISC_DISPLAY_QUEUE_FIFO, 0};
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=false;
  if(!alarm_modal) {
    alarm_pixels_valid=false;
    unsigned bytes=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
    for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)alarm_pixels+(size_t)y*bytes,
      (uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,bytes);
  }
#endif
  if (!display->submit(display->context, surface.frame, damage_count?&damage:NULL, damage_count, &options,
                       &token)) {
    display_failure("PORTABLE_APP error=display-submit");
    return;
  }
  surface.frame = 0;
  uint32_t start = millis_now();
  for (unsigned n = 0; n < 10000 && !failed; ++n) {
    risc_display_present_status_v1 s = {0};
    uint32_t elapsed=(uint32_t)(millis_now()-start);
    if(failed)break;
    if(elapsed>=10000){display_failure("PORTABLE_APP error=display-timeout");break;}
    /* A synchronous retaining panel may defer the physical transfer until
     * wait_present. Status polling alone does not advance that provider.
     * Give it the remaining existing frame budget, not a short poll timeout
     * that could abort the transfer. Async and RGB565 clients keep servicing
     * input through the existing polling path. No chip/board policy lives here. */
    bool waited=surface_format==RISC_DISPLAY_FORMAT_MONO1 &&
      (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) &&
      !(info.flags&RISC_DISPLAY_INFO_ASYNC_PRESENT) && display->wait_present;
    bool ok=waited?display->wait_present(display->context,token,10000-elapsed,&s):
      display->present_status(display->context,token,&s);
    if(!ok){display_failure(waited?"PORTABLE_APP error=display-wait":"PORTABLE_APP error=display-status");break;}
    if(s.state==RISC_DISPLAY_PRESENT_FAILED || s.state==RISC_DISPLAY_PRESENT_SUPERSEDED) {
      display_failure("PORTABLE_APP error=display-failed");break;
    }
    if (s.state == RISC_DISPLAY_PRESENT_COMPLETE){
#ifdef PORTABLE_PAPER_PREFERENCES
      paper_orientation_dirty=false;
#endif
#ifdef PORTABLE_DESK_CLOCK
      desk_present_complete=true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
      display_settled=true;if(!alarm_modal)alarm_pixels_valid=true;
#endif
      previous_valid=previous_pixels!=NULL;paper_previous_valid=paper_previous!=NULL;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
      if(handoff_active)previous_valid=false;
#endif
      return;
    }
    if ((uint32_t)(millis_now() - start) >= 10000) {
      display_failure("PORTABLE_APP error=display-timeout");
      break;
    }
    if((uint32_t)(millis_now()-input_sampled_at)>=16)input_service();
    rt->yield_ms(1);
  }
  display_failure("PORTABLE_APP error=display-timeout");
}
#ifdef PORTABLE_APP_SLEEP_LOCAL
static bool idle_sleep(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || (desk_phase!=DESK_TIMER && desk_phase!=DESK_FOREGROUND))return false;
  if(desk_phase==DESK_TIMER) {
    if(surface.frame || !display_settled)return false;
    /* The alarm-service dependency closure remains live. Do not acquire touch,
     * navigation, battery, preferences or radios just to prepare them for sleep. */
    int status=portable_app_alarm_sleep(rt,display,NULL,alarm_sleep_api());
    if(status==-2){portable_desk_adapter_retain();return false;}
    if(status<0)failed=true;
    return !failed;
  }
#endif
#ifdef PORTABLE_DESK_CLOCK
  int desk_mode=portable_desk_clock_mode();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(desk_mode<0){portable_desk_adapter_retain();return false;}
#else
  if(desk_mode<0){native_sleep_retained=true;failed=true;return false;}
#endif
#endif
  /* Existing app stack, editor draft and private storage grants stay live.
   * No handoff, unload or settings grant is introduced by idle sleeping. */
  if(surface.frame)return true;
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close()){last_activity=millis_now();return !failed;}
#endif
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_active || handoff_pending)return true;
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  /* A radio drain refusal is recoverable app UI, not a native sleep entry.
   * Keep touch/navigation live so the user can explicitly retry cleanup. */
  if(!portable_wifi_suspend()){last_activity=millis_now();return !failed;}
#endif
#ifdef PORTABLE_RADIO_SESSION
  if(!portable_radio_suspend()){failed=true;return false;}
#endif
#ifdef PORTABLE_AUDIO_SESSION
  /* Close app-owned audio before sleep preparation can call storage or alarm
   * output. Wake never restarts capture/playback without a fresh user action. */
  if(!portable_audio_suspend()){failed=true;return false;}
#endif
#ifdef PORTABLE_QUICK_RADIOS
  if(
#ifdef PORTABLE_DESK_CLOCK
     desk_mode==0 &&
#endif
     !pqa_radios_suspend(rt)){rt->diagnostic("QUICK Bluetooth cleanup-unconfirmed");failed=true;return false;}
#endif
  if(!portable_touch_close(&touch,rt)){
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
    portable_desk_adapter_retain();return false;
#endif
#ifdef PORTABLE_DESK_CLOCK
    if(desk_mode==1)native_sleep_retained=true;
#endif
    failed=true;return false;
  }
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();
#endif
  if(failed){
#ifdef PORTABLE_DESK_CLOCK
    if(desk_mode==1)native_sleep_retained=true;
#endif
    return false;
  }
  input_pending=false;navigation_pending=0;
  rt->diagnostic("PORTABLE_APP sleep=idle");
#ifdef PORTABLE_ALARM_CLIENT
  int status=portable_app_alarm_sleep(rt,display,gauge,alarm_sleep_api());
#else
  int status=portable_app_sleep(rt,display,gauge);
#endif
#ifdef PORTABLE_ALARM_CLIENT
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(status==-2){portable_desk_adapter_retain();return false;}
#else
  if(status==-2){native_sleep_retained=true;failed=true;return false;}
#endif
#endif
  if(status<0){failed=true;return false;}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  portable_wifi_resume();
#endif
#ifdef PORTABLE_QUICK_RADIOS
  if(
#ifdef PORTABLE_DESK_CLOCK
     desk_mode==0 &&
#endif
     !pqa_radios_resume(&quick_radios,&quick.ui,rt)){failed=true;return false;}
#endif
  if(!portable_touch_open(&touch,rt)){
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
    portable_desk_adapter_retain();return false;
#endif
#ifdef PORTABLE_DESK_CLOCK
    if(desk_mode==1)native_sleep_retained=true;
#endif
    failed=true;return false;
  }
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();
#endif
#ifdef PORTABLE_DESK_CLOCK
  if(failed && desk_mode==1){native_sleep_retained=true;return false;}
#endif
  input_sample=(portable_touch_sample){0};input_pending=false;navigation_pending=0;
#ifdef PORTABLE_DESK_CLOCK
  home_pending=crown_pending=false;
  nova_contact=(springboard_contact){0};
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  quick_replay_pending=quick_replay_delivery=false;
#endif
#endif
  last_activity=last_poll_at=input_sampled_at=millis_now();previous_valid=false;
#ifdef PORTABLE_DESK_CLOCK
  if(failed && desk_mode==1)native_sleep_retained=true;
#endif
  rt->diagnostic(status?"PORTABLE_APP sleep=resumed":"PORTABLE_APP sleep=refused");
  return !failed;
}
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
/* Only a settled foreground can change orientation. Invalidate both physical
 * history and modal restoration images before the next complete repaint.
 * Do not touch retained custody; do not apply while a modal owns resources. */
static inline bool paper_apply_flip(unsigned value) {
  if(failed || value>1u || !pp_enabled() || surface_format!=RISC_DISPLAY_FORMAT_MONO1)return false;
  if(paper_flip_ui==(value!=0))return true;
#ifdef PORTABLE_ALARM_CLIENT
  if(native_sleep_retained || alarm_modal || !display_settled)return false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick_modal || pqa_visible(&quick.ui) || pqa_capture(&quick.ui) || quick.ui.pending)return false;
  pqa_cancel(&quick.ui);quick_replay_pending=quick_replay_delivery=false;
#endif
  if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
  paper_previous_valid=previous_valid=false;
#ifdef PORTABLE_ALARM_CLIENT
  alarm_pixels_valid=false;
#endif
  input_pending=false;input_sample=(portable_touch_sample){0};
  touch.neutral=touch.down=false;touch.home_neutral=touch.home_down=false;
  navigation_pending=0;home_pending=crown_pending=false;
  nova_contact=(springboard_contact){0};
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
  nu_gesture=false;
#endif
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();if(failed)return false;
#endif
  paper_flip_ui=value!=0;paper_orientation_dirty=true;return true;
}
#endif
#ifdef PORTABLE_DESK_CLOCK
bool portable_desk_adapter_flip(unsigned value) {return paper_apply_flip(value);}
void portable_desk_adapter_caption(int y,const char *text) {
  int size=0;
  for(unsigned i=0;text && text[i] && i<96;++i) {
    unsigned c=(unsigned char)text[i];if(c<32||c>126)c='?';
    size+=rpp_text[95+c-32].advance_q4;
  }
  pp_text((width()-(size+15)/16)/2,y,width(),text,1|PAPER_TEXT_LITERAL,false,true);
}
/* These are hidden app links, never ELF exports. Seed both the provider's
 * physical-image reconstruction and the adapter's damage comparison cache. */
bool portable_desk_adapter_ready(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 return !failed && (desk_phase==DESK_TIMER || desk_phase==DESK_FOREGROUND);
#else
 return !failed;
#endif
}
void portable_desk_adapter_retain(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 /* App-local flags alone cannot keep the Runtime from freeing an invocation.
  * This native owner fence does no provider I/O, polling or release, and also
  * accepts an already Runtime-retained current invocation. */
 if(!desk_native_fenced) {
  if(!desk_runtime || !desk_runtime->retain_invocation ||
     !desk_runtime->retain_invocation()) {
   /* Unsupported/foreign custody cannot safely return or yield. Valid startup
    * admits the owner-only suffix, so this is an invariant-failure fail-stop. */
   for(;;) {}
  }
  desk_native_fenced=true;
 }
 if(!native_sleep_retained) {
  /* Pin the lease descriptor while making already-copied drawing callbacks
   * inert. Never release or free this retained surface after uncertain I/O. */
  desk_retained_surface=surface;surface=(risc_display_surface_v1){0};
 }
#endif
 native_sleep_retained=true;failed=true;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 /* Existing presentation callbacks may have been copied by the app. Their
  * optional battery/clock sources must stop too; grants remain pinned. */
 gauge=NULL;
#if defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
 np_rtc=NULL;
#endif
#endif
}
void portable_desk_adapter_invalidate(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(!portable_desk_adapter_ready())return;
#endif
  if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
  paper_previous_valid=false;previous_valid=false;
}
void portable_desk_adapter_begin(void) {
  list_mode=false;
  /* History is attached to this lease. Repaint it in place: releasing and
   * reacquiring here would invalidate the provider's one-shot seeded image. */
  if(!failed && surface.frame)fill(0,0,width(),height(),0xffff);
  else pp_begin();
}
int portable_desk_adapter_seed(void) {
  const risc_display_output_api_v1_history *history=risc_display_output_history(display);
  if(failed || !surface.frame || surface_format!=RISC_DISPLAY_FORMAT_MONO1 ||
     !paper_previous || !history)return 0;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(!history->seed_previous(display->context,surface.frame)){portable_desk_adapter_retain();return -2;}
#else
  if(!history->seed_previous(display->context,surface.frame))return -2;
#endif
  unsigned bytes=(info.width+7)/8;
  for(unsigned y=0;y<info.height;y++)memcpy(paper_previous+(size_t)y*bytes,
    (uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,bytes);
  paper_previous_valid=true;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  /* Fresh timer reconstruction proves the retained orientation of the actual
   * panel image; it is not a foreground preference-change full-refresh debt. */
  if(desk_phase==DESK_TIMER)paper_orientation_dirty=false;
#endif
  return 1;
}
bool portable_desk_adapter_present(bool full) {
  present(full);return !failed && desk_present_complete;
}
bool portable_desk_adapter_sleep(void) { return idle_sleep(); }
bool portable_desk_adapter_foreground(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(failed || desk_phase!=DESK_FOREGROUND)return false;
#endif
#ifdef PORTABLE_QUICK_RADIOS
 if(!desk_radios_loaded){
  if(!pqa_radios_load(&quick_radios,&quick.ui,rt)){portable_desk_adapter_retain();return false;}
  desk_radios_loaded=true;
 }
#endif
 return !failed;
}
#endif
#ifdef PORTABLE_QUICK_ACTIONS
#include "quick_adapter.inc"
#endif
static void clear_contact_snapshots(void) {
  nova_contact=(springboard_contact){0};
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
}
static bool poll_input(t5_app_input_t *out, uint32_t wait) {
  memset(out, 0, sizeof(*out));
  clear_contact_snapshots();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  /* TIMER has no foreground modal or subscriptions. The selected clock owns
   * alarm reconciliation and the promotion decision before normal polling. */
  if(!portable_desk_adapter_ready())return false;
  if(desk_phase==DESK_TIMER){rt->yield_ms(wait?wait:1);return !failed;}
#endif
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
  if (failed)
    return false;
#ifdef PORTABLE_ALARM_CLIENT
  bool consumed=false;
  if(!alarm_foreground(&consumed))return false;
  if(consumed){crown_pending=false;return true;}
#endif
  /* A rejected paper top-edge gesture replays its original down followed
   * by the saved current sample before sampling another contact. */
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick_replay_delivery){quick_replay_delivery=false;goto replay_input;}
#endif
  uint32_t now=millis_now(),spent=now-last_poll_at;
  rt->yield_ms(spent<wait?wait-spent:1);
  last_poll_at=millis_now();
  input_service();
#ifdef PORTABLE_ALARM_CLIENT
  if(!alarm_foreground(&consumed))return false;
  if(consumed){crown_pending=false;return true;}
#endif
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
  portable_audio_capture_resume();
#endif
#if defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_SLEEP_MANUAL_ONLY)
  if(!failed && (uint32_t)(millis_now()-last_activity)>=60000u &&
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
     !portable_audio_capture_active() &&
#endif
     !navigation_pending && !(input_pending && (input_sample.down || input_sample.released))) {
    if(!idle_sleep())return false;
    return !failed; /* Waking crown/contact never becomes an app action. */
  }
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  bool quick_consumed=false;
  if(!quick_foreground(&quick_consumed))return false;
  if(quick_consumed){
    #if !defined(PORTABLE_UPDATE_APP) || !defined(PORTABLE_HOME_APP)
    crown_pending=false;
#endif
    out->exit_requested=quick_launch_pending;
    /* A reserved gesture is not app contact. Keep the presentation neutral
     * without an invented release, so deliberate replay can begin normally. */
    if(quick.ui.gesture==PQA_TOP_PENDING && !pqa_visible(&quick.ui))nova_contact.valid=true;
    return true;
  }
replay_input:
#endif
#ifdef PORTABLE_HOME_APP
  /* Physical root navigation must not first trigger a nested app Back/redraw. */
  if(home_pending||crown_pending){navigation_pending=0;input_pending=false;return !failed;}
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
  if(sample.valid && !sample.cancelled && sample.tap_eligible && sample.down && x<width() && y<height())
    app_contact=(t5_app_contact_t){true,(int16_t)x,(int16_t)y};
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
  if(sample.cancelled || !sample.valid)nu_gesture=false;
  if(sample.began){nu_gesture=true;nu_start_x=x;nu_start_y=y;
#ifdef PORTABLE_QUICK_ACTIONS
   if(quick_replay_pending){nu_start_x=quick.ui.start_x;nu_start_y=quick.ui.start_y;}
#endif
  }
#ifdef PORTABLE_QUICK_ACTIONS
  quick_replay_pending=false;
#endif
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
    if (x >= width() || y >= height())
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
    else if (list_mode && y >= height() - 32)
#ifdef PORTABLE_SETTINGS_APP
      out->buttons = settings_editing
                         ? (x < width() / 2 ? T5_APP_BUTTON_LEFT : T5_APP_BUTTON_RIGHT)
                         : (x < width() / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN);
#else
      out->buttons = x < width() / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN;
#endif
    else if (list_mode && y < 40 && x >= width() - 56)
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
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed){memset(out,0,sizeof(*out));return false;}
#endif
  if(handoff_requested){clear_contact_snapshots();memset(out,0,sizeof(*out));out->exit_requested=true;return true;}
  bool ok=poll_input(out,wait);
#ifdef PORTABLE_ALARM_CLIENT
  if(!ok)return alarm_failure();
#endif
#ifdef PORTABLE_CROWN_SLEEP_LOCAL
  if(ok && crown_pending) {
    crown_pending=home_pending=false;navigation_pending=0;input_pending=false;
    memset(out,0,sizeof(*out));
    return idle_sleep();
  }
#endif
#if defined(PORTABLE_RETURN_APP) || defined(PORTABLE_HOME_APP)
  const char *destination=NULL;
#ifdef PORTABLE_HOME_APP
  if(home_pending||crown_pending)destination=PORTABLE_HOME_APP;
#endif
#ifdef PORTABLE_RETURN_APP
  bool returning=out->exit_requested;
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  returning|=back_exits_app && !!(out->buttons&T5_APP_BUTTON_BACK);
#elif !defined(PORTABLE_SETTINGS_APP)
  returning|=!!(out->buttons&T5_APP_BUTTON_BACK);
#endif
  if(returning && !destination)destination=PORTABLE_RETURN_APP;
#endif
  home_pending=false;
  if(ok && destination
#ifdef PORTABLE_QUICK_ACTIONS
     && !quick_launch_pending
#endif
  ) {
    if(!app_allows_launch(destination)) {
      home_pending=crown_pending=false;navigation_pending=0;input_pending=false;
      clear_contact_snapshots();memset(out,0,sizeof(*out));return true;
    }
#ifdef PORTABLE_UPDATE_APP
    if(!portable_update_close()){home_pending=crown_pending=false;navigation_pending=0;input_pending=false;clear_contact_snapshots();memset(out,0,sizeof(*out));return true;}
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
    if(!portable_file_browser_close())return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
    if(!portable_radio_suspend())return alarm_failure();
#endif
#ifdef PORTABLE_AUDIO_SESSION
    if(!portable_audio_suspend())return alarm_failure();
#endif
#ifdef PORTABLE_WIFI_SETTINGS_APP
    /* Wi-Fi owns checked cleanup and nested Back. Home is a direct root exit;
     * refusal leaves its controller available for an explicit cleanup retry. */
    if(!portable_wifi_close()) {
      crown_pending=false;memset(out,0,sizeof(*out));return true;
    }
#endif
    if(!rt->request_launch(destination)) {
      rt->diagnostic("PORTABLE_APP error=return-request");
#ifdef PORTABLE_APP_LAUNCH_GUARD
      /* Admission refusal is not a cleanup failure. The guarded app remains
       * paused and can offer a fresh explicit attempt without losing edits. */
      home_pending=crown_pending=false;navigation_pending=0;input_pending=false;
      clear_contact_snapshots();memset(out,0,sizeof(*out));return true;
#else
      failed=true;return false;
#endif
    }
#ifdef PORTABLE_UPDATE_APP
    extern void portable_update_handoff(void);
    portable_update_handoff();
#endif
    handoff_requested=true;out->exit_requested=true;
  }
#endif
#ifdef PORTABLE_CROWN_SLEEP_UNAVAILABLE
  if(ok && crown_pending){out->buttons|=PAPER_BUTTON_SLEEP_UNAVAILABLE;rt->diagnostic("PAPER_CLOCK sleep=unavailable");}
#endif
  home_pending=crown_pending=false;
  return ok;
}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
#include "wifi_view.inc"
#endif
#ifdef PORTABLE_ALARM_CLIENT
#include "alarm.inc"
#endif
static bool refresh(void) { return portable_catalog_count <= 17; }
static uint32_t count(void) {
  return portable_catalog_count <= 17 ? portable_catalog_count : 0;
}
static bool get(uint32_t i, t5_app_manifest_t *out) {
  if (!out || i >= count())
    return false;
  *out = portable_catalog[i];
  return true;
}
static bool launch(uint32_t i) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || desk_phase!=DESK_FOREGROUND)return false;
#endif
  /* A terminal Home/return is already accepted. Do not replace its target or
   * report a false launch failure to an unwinding nested app. */
  if(handoff_requested)return true;
#ifdef PORTABLE_ALARM_CLIENT
  bool consumed=false;
  if(!alarm_foreground(&consumed) || consumed)return false;
#endif
  return !failed &&
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
      !handoff_active &&
#endif
      !(input_pending && (input_sample.down || input_sample.cancelled)) && !(navigation_pending&T5_APP_BUTTON_BACK) && i < count() && app_allows_launch(portable_catalog[i].file_name) && rt->request_launch(portable_catalog[i].file_name);
}
#ifdef PORTABLE_POWER_STATUS
#include "PortablePowerStatus.h"
bool portable_power_read(risc_battery_sample_v1 *out) {
  if (!out) return false;
  *out=(risc_battery_sample_v1){0,255,RISC_BATTERY_PROFILE_MISSING};
  risc_battery_sample_v1 sample={0,255,RISC_BATTERY_PROFILE_MISSING};
  if (failed || !bg.api || !gauge || !gauge->read || !gauge->read(gauge->context,&sample)) return false;
  *out=sample;
  return true;
}
#endif
static bool read_battery(t5_battery_state_t *out) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || desk_phase!=DESK_FOREGROUND)return false;
#endif
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
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(!portable_desk_adapter_ready())return NULL;
#endif
  return v == 1 && !failed ? &app : NULL;
}
const t5_ui_api_v1 *t5_ui_get_api(uint32_t v) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(!portable_desk_adapter_ready())return NULL;
#endif
  return v == 1 && !failed ? &ui : NULL;
}
const t5_battery_api_v1 *t5_battery_get_api(uint32_t v) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || desk_phase!=DESK_FOREGROUND)return NULL;
#endif
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
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(desk_phase!=DESK_COLD || native_sleep_retained)return -1;
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
  if(paper_preferences_retained)return -1;
#endif
  rt = risc_runtime_get_api(1);
  if (!rt || rt->api_version != 1 ||
      rt->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE || !rt->acquire ||
      !rt->release || !rt->health || !rt->yield_ms || !rt->request_launch ||
      !rt->diagnostic)
    return -1;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE || !rt->retain_invocation)return -1;
#endif
  dg.struct_size = sizeof(dg);
  bg.struct_size = sizeof(bg);
  failed = false;
#ifdef PORTABLE_PAPER_PREFERENCES
  paper_flip_ui=paper_orientation_dirty=false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=true;alarm_pixels_valid=alarm_modal=native_sleep_retained=false;alarm_pixels=NULL;
  alarm_error_seen=alarm_failed_cleaned=false;memset(&alarms,0,sizeof(alarms));
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  pqa_session_init(&quick);quick_background=NULL;quick_modal=quick_launch_pending=quick_replay_pending=quick_replay_delivery=false;
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
  list_mode = false;input_pending=home_pending=crown_pending=handoff_requested=false;navigation_pending=0;previous_valid=false;
  input_sampled_at=last_poll_at=0;previous_pixels=NULL;paper_previous=NULL;paper_previous_valid=false;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_old=handoff_scratch=NULL;handoff_pending=false;handoff_active=false;handoff_first=false;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  desk_runtime=rt;desk_guarded_runtime=*rt;
  desk_guarded_runtime.acquire=desk_acquire;desk_guarded_runtime.release=desk_release;
  desk_guarded_runtime.health=desk_guard_health;desk_guarded_runtime.yield_ms=desk_guard_yield;
  desk_guarded_runtime.diagnostic=desk_guard_diagnostic;desk_guarded_runtime.request_launch=desk_guard_launch;
#ifdef PORTABLE_QUICK_RADIOS
  desk_quick_runtime=desk_guarded_runtime;desk_quick_runtime.acquire=desk_quick_acquire;
#endif
  rt=&desk_guarded_runtime;desk_phase=DESK_INITIALIZED;
  return 0;
#else
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
      !(info.supported_formats & (RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)|
                                 RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1))))
    return -1;
  surface_format=(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565))?
    RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1;
  paper_rotated=PORTABLE_DISPLAY_ROTATION==90 && surface_format==RISC_DISPLAY_FORMAT_MONO1 &&
    (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) && info.width>info.height;
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && !pp_enabled())return -1;
#ifdef PORTABLE_PAPER_PREFERENCES
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && pp_enabled()) {
    paper_preferences_grant=(risc_runtime_capability_v1){.struct_size=sizeof(paper_preferences_grant)};unsigned value=0;
    if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_READER_STORE_INSTANCE,&paper_preferences_grant)) {
      (void)portable_reader_preference_load(paper_preferences_grant.api,false,&value);
      if(!rt->release(&paper_preferences_grant)) {
        /* Preserve the actual grant and every resource on uncertain cleanup. */
        paper_preferences_retained=failed=true;
#ifdef PORTABLE_ALARM_CLIENT
        native_sleep_retained=true;
#endif
        return -1;
      }
    }
    paper_flip_ui=value!=0;
  }
#endif
#ifdef PORTABLE_NOVA_UI
  if(!pp_enabled() && (info.width!=240 || info.height!=240))return -1;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  alarm_pixels=malloc((size_t)(surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2)*info.height);
  if(!alarm_pixels || !portable_alarm_open(&alarms,rt))return -1;
#endif
  if (!portable_touch_open(&touch, rt))return -1;
#ifndef PORTABLE_FORCE_FULL_FRAMES
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && (info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE))
    paper_previous=malloc((size_t)((info.width+7)/8)*info.height); /* optional, 48 KB at 800x480 */
  if(surface_format==RISC_DISPLAY_FORMAT_RGB565 && (info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE) && info.width<=320 && info.height<=320)
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
#ifdef PORTABLE_QUICK_ACTIONS
  quick.ui.paper=pp_enabled();
  if(!pqa_session_load(&quick,rt))return -1;
  quick_paper_capabilities();
#ifdef PORTABLE_QUICK_RADIOS
#ifdef PORTABLE_DESK_CLOCK
  desk_radios_loaded=false;
#else
  if(!pqa_radios_load(&quick_radios,&quick.ui,rt))return -1;
#endif
#endif
#endif
  return failed?-1:0;
#endif
}
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
#include "sparse_clock_adapter.inc"
#endif
__attribute__((visibility("default"))) void app_module_fini(void) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  desk_finalize();
#else
#ifdef PORTABLE_PAPER_PREFERENCES
  if(paper_preferences_retained)return;
#endif
  if (!rt)
    return;
#ifdef PORTABLE_ALARM_CLIENT
  if(native_sleep_retained)return; /* Runtime normally blocks fini first. */
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close()) {
    rt->diagnostic("FILE_BROWSER cleanup-unconfirmed; invocation retained");
    for(;;)rt->yield_ms(50);
  }
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  if(!portable_wifi_close()) {
    rt->diagnostic("WIFI cleanup-unconfirmed; invocation retained");
    for(;;)rt->yield_ms(50);
  }
#endif
#ifdef PORTABLE_RADIO_SESSION
  if(!portable_radio_suspend()) {
    rt->diagnostic("RADIO cleanup-unconfirmed; invocation retained");
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
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick.ui.torch && !pqa_session_restore(&quick,display))rt->diagnostic("QUICK brightness-restore-failed");
  free(quick_background);quick_background=NULL;
#endif
  if (surface.frame)
    display->release(display->context, surface.frame);
  surface.frame = 0;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_finish();
#endif
  np_close();free(previous_pixels);previous_pixels=NULL;previous_valid=false;
  free(paper_previous);paper_previous=NULL;paper_previous_valid=false;
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
#endif
}

__attribute__((visibility("default"))) int app_module_init(void) {
  int status = initialize();
#ifndef PORTABLE_DESK_CLOCK_SPARSE_START
  if (status)
    app_module_fini();
#endif
  return status;
}

/* Minimal runtime does not export abs. App-local implementation; gesture
 * differences are bounded, with saturation guarding malformed coordinates. */
int abs(int value) {
  return value >= 0 ? value : (value == INT_MIN ? INT_MAX : -value);
}
