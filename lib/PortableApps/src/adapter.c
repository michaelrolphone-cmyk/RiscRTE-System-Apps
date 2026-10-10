#if defined(PORTABLE_SETTINGS_LIST_SCROLL) && (!defined(PORTABLE_TOUCH_SCROLL) || !defined(PORTABLE_SETTINGS_APP))
#error "Settings list scrolling requires selected Settings touch scrolling"
#endif
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#error "Native toolbar requires the canonical native custody fence"
#endif
#if defined(PORTABLE_SETTINGS_APP) || defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_DESK_CLOCK) || defined(PORTABLE_DESK_CLOCK_SPARSE_START) || defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
#error "Native toolbar requires one unambiguous app-owned time source"
#endif
#endif
#if defined(PORTABLE_DESK_CLOCK_SPARSE_START) && !defined(PORTABLE_NATIVE_CUSTODY_FENCE)
#define PORTABLE_NATIVE_CUSTODY_FENCE
#endif
#if defined(PORTABLE_SETTINGS_NATIVE_TIME) && (!defined(PORTABLE_SETTINGS_APP) || !defined(PORTABLE_SETTINGS_TIME_ZONE) || !defined(PORTABLE_SETTINGS_X4_DESK_CLOCK) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE))
#error "Native time Settings requires explicit Settings, timezone, X4 paper and native custody profiles"
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_SETTINGS_APP) && !defined(PORTABLE_SETTINGS_NATIVE_TIME)
#error "Settings touch scrolling requires the selected native paper profile"
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP) && !defined(PORTABLE_NATIVE_CUSTODY_FENCE)
#error "Wi-Fi touch scrolling requires the selected native paper profile"
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
#include "PortableFileBrowser.h"
#endif
#if defined(PORTABLE_APP_TOUCH_SCROLL) && (!defined(PORTABLE_TOUCH_SCROLL) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE) || (!defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_UPDATE_TOUCH_SCROLL)))
#error "App touch scrolling requires selected native paper, scroll and app profiles"
#endif
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
#include "PortableSpringboardCatalog.h"
void portable_springboard_scroll_interrupt(void);
#endif
#ifdef PORTABLE_USB_TRANSFER_APP
#include "PortableUsbTransfer.h"
#if !defined(PORTABLE_APP_LAUNCH_GUARD) || defined(PORTABLE_APP_SLEEP_LOCAL) || defined(PORTABLE_QUICK_ACTIONS)
#error "USB transfer requires guarded navigation and an awake dedicated owner"
#endif
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_SHELL_CLIENT)
#error "Select exactly one resident role"
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) || defined(PORTABLE_RESIDENT_SHELL_CLIENT)
#if !defined(PORTABLE_NATIVE_CUSTODY_FENCE) || !defined(PORTABLE_ALARM_CLIENT)
#error "Resident adapters require settled alarm and native custody checkpoints"
#endif
#include "RiscResidentShellV1.h"
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={
 RISC_RESIDENT_SHELL_API_V1,sizeof(risc_resident_app_descriptor_v1_t),
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 RISC_RESIDENT_ROLE_HOST,
#else
 RISC_RESIDENT_ROLE_FOREGROUND,
#endif
 0};
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && !defined(PORTABLE_QUICK_ACTIONS)
#error "The resident host owns Quick Actions"
#endif
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && defined(PORTABLE_QUICK_ACTIONS)
#error "Converted foreground ELFs must not contain Quick Actions"
#endif
#if defined(PORTABLE_RESIDENT_LOADING) && !defined(PORTABLE_RESIDENT_SHELL_HOST)
#error "App loading presentation belongs to the resident host"
#endif
/* Client-side adapter for existing shared apps; no board/chip/pin knowledge. */
#include "PortableApps.h"
#include "PortablePerformance.h"
#include "PortableStageLog.h"
#include "PortableTouch.h"
#ifdef PORTABLE_TOUCH_SCROLL
#include "PortableTouchScroll.h"
static portable_scroll_viewport raster_clip;
static bool raster_clip_active;
#ifdef PORTABLE_APP_TOUCH_SCROLL
#include "PortablePaperScroll.h"
static portable_touch_sample paper_scroll_sample;
static uint32_t paper_scroll_sampled_at;
void portable_paper_scroll_sample(portable_touch_sample *sample,uint32_t *now) {
  *sample=paper_scroll_sample;*now=paper_scroll_sampled_at;
}
void portable_paper_scroll_clip(const portable_scroll_viewport *view) {
  raster_clip_active=view!=NULL;if(view)raster_clip=*view;
}
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
static portable_touch_sample file_scroll_sample;
static uint32_t file_scroll_sampled_at;
void portable_file_browser_touch_sample(portable_touch_sample *sample,uint32_t *now) {
  *sample=file_scroll_sample;*now=file_scroll_sampled_at;
}
void portable_file_browser_scroll_clip(const portable_scroll_viewport *view) {
  raster_clip_active=view!=NULL;if(view)raster_clip=*view;
}
#endif
#endif
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
#if defined(PORTABLE_PERFORMANCE_DISPLAY_METRICS) || defined(PORTABLE_STAGE_DISPLAY_METRICS)
#include "RiscDisplayOutputMetricsV1.h"
#endif
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
#define portable_wifi_idle_ready portable_update_idle_ready
#endif
#endif
#include <limits.h>
#include <stdlib.h>
#include "PortableBackgroundServices.h"
#ifdef PORTABLE_CONTEXTS_CLIENT
#include "ContextsServiceV1.h"
#if !defined(PORTABLE_NATIVE_CUSTODY_FENCE) || !defined(PORTABLE_ALARM_CLIENT) || (!defined(PORTABLE_QUICK_ACTIONS) && !defined(PORTABLE_RESIDENT_SHELL_CLIENT))
#error "X4 Contexts requires native custody, alarms and Quick Controls"
#endif
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && !defined(ALARM_SERVICE_TAGGED_V2)
#error "Resident Contexts requires the tagged alarm capability view"
#endif
#if !defined(PORTABLE_NATIVE_TIME_TOOLBAR) && !defined(PORTABLE_SETTINGS_NATIVE_TIME) && !defined(PORTABLE_CONTEXTS_CLOCK_RF_ONLY)
#error "X4 Contexts requires the checked native storage runtime"
#endif
#if defined(PORTABLE_DESK_CLOCK_SPARSE_START) && (!defined(PORTABLE_CONTEXTS_CLOCK_RF_ONLY) || !defined(PORTABLE_X4_IDLE_POLICY))
#error "Sparse Clock Contexts requires the explicit RF-only profile and checked idle storage"
#endif
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
#if !defined(PORTABLE_DESK_CLOCK_SPARSE_START)
#error "The Contexts owner rendezvous belongs only to the sparse default Clock"
#endif
#define PORTABLE_CONTEXTS_SOURCE_MASK CONTEXTS_RADIO
static bool contexts_clock_recovered,contexts_clock_handoff;
#endif
static bool contexts_tick(void);
static bool contexts_capture_checkpoint(void);
static bool contexts_before_storage(void);
static bool contexts_suspend(void);
static uint32_t contexts_pixels;
#endif
#ifdef PORTABLE_APP_LAUNCH_GUARD
/* The application may consume one navigation attempt to resolve an edit.
 * This is client policy, not native authority or an uncertain cleanup state. */
#include "PortableAppLaunchGuard.h"
#define app_allows_launch(destination) portable_app_before_launch(destination)
#else
#define app_allows_launch(destination) (true)
#endif
#ifdef PORTABLE_APP_HOME_GUARD
#include "PortableAppLaunchGuard.h"
#define app_allows_home(destination) portable_app_before_home()
#else
#define app_allows_home(destination) app_allows_launch(destination)
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
#ifdef PORTABLE_TEXT_INPUT_CLIENT
#include "PortableTextInputHandoff.h"
static bool text_input_suspended,text_input_retained;
#endif
#if defined(PORTABLE_STAGE_LOGS) && defined(PORTABLE_STAGE_DISPLAY_METRICS)
static void stage_display_complete(risc_display_present_token_v1 token) {
 const risc_display_output_api_v1_metrics *ext=risc_display_output_metrics(display);
 if(!ext)return;
 risc_display_present_metrics_v1 value={.api_version=1,.struct_size=sizeof(value)};
 if(!ext->snapshot(display->context,&value) || value.token!=token)return;
 char line[176];
 snprintf(line,sizeof(line),"bytes=%lu gpio_calls=%lu mode=%s window=%ld,%ld,%lu,%lu",
  (unsigned long)value.bytes_sent,(unsigned long)value.gpio_write_calls,
  value.mode==RISC_DISPLAY_METRICS_PARTIAL?"partial":"full",
  (long)value.effective_update.x,(long)value.effective_update.y,
  (unsigned long)value.effective_update.width,(unsigned long)value.effective_update.height);
 portable_stage_log(rt,"display-transfer",line);
 snprintf(line,sizeof(line),"queued_ms=%llu start_ms=%llu end_ms=%llu valid=%lu",
  (unsigned long long)value.queued_ms,(unsigned long long)value.transfer_start_ms,
  (unsigned long long)value.transfer_end_ms,(unsigned long)value.valid_times);
 portable_stage_log(rt,"display-transfer-times",line);
 snprintf(line,sizeof(line),"refresh_ms=%llu assert_ms=%llu complete_ms=%llu valid=%lu",
  (unsigned long long)value.refresh_ms,(unsigned long long)value.busy_assert_ms,
  (unsigned long long)value.busy_done_ms,(unsigned long)value.valid_times);
 portable_stage_log(rt,"display-busy-times",line);
}
#else
#define stage_display_complete(token) ((void)0)
#endif
#include "performance.inc"
/* A controller opts into cooperative presentation completion through PaperFrame.h.
 * The submitted lease stays provider-owned until this token completes. */
static bool paper_async_frames;
static risc_display_present_token_v1 paper_token;
static uint32_t paper_submitted_at;
static bool paper_present_progress(void);
bool portable_paper_frame_ready(void);
static bool paper_token_progress(void);
bool portable_paper_frame_drain(void);
static bool paper_token_clean;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#include "PortableNativeCustody.h"
#ifndef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
#error "Native custody requires the complete canonical Runtime retention SDK"
#endif
static bool native_custody_retained;
static const risc_runtime_api_v1 *native_custody_runtime;
static volatile risc_display_surface_v1 native_custody_surface;
#if defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_NATIVE_TIME_TOOLBAR)
#include "RiscRealtimeV1.h"
#include "RiscKeyValueV1.h"
#endif
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
static int initialize_providers(void);
#endif
#endif
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
static bool desk_touch_neutral;
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
#ifdef PORTABLE_RESIDENT_SHELL_HOST
static bool quick_clean_present,resident_failure_present;
#ifdef PORTABLE_RESIDENT_POLICY
static bool resident_sleep_present;
#ifdef PORTABLE_DISPLAY_SETTLED
static risc_display_present_token_v1 resident_sleep_token;
#endif
#endif
#ifdef PORTABLE_RESIDENT_LOADING
static bool resident_loading_present;
#endif
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
static bool quick_open_requested;
#endif
static bool quick_foreground(bool *consumed);
static bool quick_interrupt(void);
unsigned portable_quick_brightness(void) {return quick.brightness;}
#ifdef PORTABLE_LOW_BATTERY
#include "PortableLowBattery.h"
#ifndef PORTABLE_QUICK_RADIOS
#error Low battery policy requires existing Quick Controls radio lifecycle
#endif
static portable_low_battery low_battery;
static bool low_battery_sampled;
static uint32_t low_battery_sampled_at;
#endif
#endif
#ifdef PORTABLE_ALARM_CLIENT
#ifdef ALARM_SERVICE_TAGGED_V2
#include "PortableAlarmClient.h"
#else
#include "AlarmServiceV1.h"
#endif
static bool display_settled,alarm_pixels_valid,alarm_modal,native_sleep_retained;
bool portable_app_sleep_retained(void) { return native_sleep_retained; }
static uint16_t *alarm_pixels;
static bool alarm_foreground(bool *consumed);
#if defined(PORTABLE_CONTEXTS_CLIENT) || (defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_RESIDENT_SHELL_CLIENT)) || (defined(PORTABLE_QUICK_ACTIONS) && !defined(ALARM_SERVICE_TAGGED_V2))
static const alarm_service_v1 *alarm_sleep_api(void);
#endif
#if (defined(PORTABLE_QUICK_ACTIONS) || defined(PORTABLE_CONTEXTS_CLIENT)) && defined(ALARM_SERVICE_TAGGED_V2)
static uint32_t alarm_output_modes(void);
#endif
#if defined(PORTABLE_QUICK_ACTIONS) && (defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION))
static bool alarm_refresh(void);
#endif
static bool alarm_failure(void);
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#include "native_custody_adapter.inc"
#ifdef PORTABLE_CONTEXTS_CLIENT
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() (!native_custody_retained)
#define PORTABLE_CONTEXTS_PAUSE_POLICY_READS
uint16_t portable_contexts_action_mask(void);
#define PORTABLE_CONTEXTS_ACTION_MASK() portable_contexts_action_mask()
#define PORTABLE_CONTEXTS_CLEANUP_FAILURE() portable_adapter_retain()
static bool contexts_alert_allowed(unsigned mode);
#define PORTABLE_CONTEXTS_ALERT_ALLOWED(mode) contexts_alert_allowed(mode)
#include "PortableContextsClient.h"
#include "contexts_adapter.inc"
#endif
#ifdef PORTABLE_BLE_BROADCAST
#define PORTABLE_BROADCAST_CUSTODY_SAFE() (!native_custody_retained)
#define PORTABLE_BROADCAST_PAUSE_POLICY_READS
#include "PortableBroadcastClient.h"
#include "broadcast_adapter.inc"
#endif
#endif
static unsigned first_row, last_rows;
#ifdef PORTABLE_SETTINGS_APP
#include "PortableRtcClock.h"
static bool back_exits_app = true, settings_editing;
#if defined(PORTABLE_SETTINGS_NATIVE_TIME) && defined(PORTABLE_QUICK_ACTIONS)
static bool settings_native_clock(uint8_t *hour,uint8_t *minute);
#endif
static bool settings_view_poll(t5_app_input_t *out);
#ifdef PORTABLE_TOUCH_SCROLL
static void settings_view_interrupt(void);
#endif
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
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
  if(rt->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE && rt->monotonic_ms) {
    uint32_t now=0;
    if(!rt->monotonic_ms(&now)){failed=true;return 0;}
    return now;
  }
#endif
  risc_runtime_health_v1 h = {.struct_size = sizeof(h)};
  if (!rt->health(&h)) {
    failed = true;
    return 0;
  }
  return h.uptime_ms;
}
/* Capture raw reports separately from ordered foreground reduction. */
static portable_touch_sample input_sample;
static bool input_pending,input_progressed;
static uint32_t input_sampled_at,input_delivered_at,last_poll_at;
static void input_reset(void) {
 input_pending=input_progressed=false;input_sample=(portable_touch_sample){0};
 if(!failed)portable_touch_reset(&touch);
}
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && defined(PORTABLE_RESIDENT_POLICY)
static bool resident_activity_pending=true;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
static void resident_input(portable_touch_sample *sample);
static int resident_checkpoint(uint32_t reason);
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
static bool resident_queue_launch(const char *path);
#endif
static uint32_t navigation_pending;
static bool home_pending,crown_pending,handoff_requested;
#if defined(PORTABLE_WIFI_SETTINGS_APP) && (defined(PORTABLE_RETURN_APP) || defined(PORTABLE_HOME_APP))
static const char *wifi_deferred_return;
#endif
#ifdef PORTABLE_DESK_LOCK_HOME
static bool desk_lock_armed,navigation_home_down,desk_quality_present;
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
static bool nu_gesture;
static int nu_start_x,nu_start_y;
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
#include "PortableAppSleep.h"
static uint32_t last_activity;
static inline bool idle_capture_active(void) {
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
 if(portable_audio_capture_active())return true;
#endif
#ifdef PORTABLE_RADIO_CONTINUOUS_CAPTURE
 if(portable_radio_capture_active())return true;
#endif
 return false;
}
static inline uint32_t portable_idle_ms(void) {
#ifdef PORTABLE_LOW_BATTERY
 return quick.idle_ms;
#else
 return 60000u;
#endif
}
#ifdef PORTABLE_X4_IDLE_POLICY
#if !defined(PORTABLE_NATIVE_CUSTODY_FENCE) || !defined(ALARM_SERVICE_TAGGED_V2)
#error X4 idle requires native custody and tagged alarm lifecycle
#endif
static bool automatic_idle;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
static void desk_kv_reset(void);
#endif
#endif
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
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(native_custody_retained)return;
 if(navigation_ready && !navigation->reset(navigation->context))portable_adapter_retain();
#else
 if(navigation_ready && !navigation->reset(navigation->context))failed=true;
#endif
}
#if !defined(PORTABLE_DESK_CLOCK_SPARSE_START) && !defined(PORTABLE_SETTINGS_NATIVE_TIME) && !defined(PORTABLE_NATIVE_TIME_TOOLBAR) && !defined(PORTABLE_RESIDENT_SHELL_CLIENT)
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
#ifdef PORTABLE_TEXT_INPUT_CLIENT
 if(text_input_suspended || text_input_retained)return;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_capture_checkpoint())return;
#endif
#ifdef PORTABLE_USB_TRANSFER_APP
 portable_usb_transfer_service();
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(failed)return;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(failed || desk_phase!=DESK_FOREGROUND || !touch.subscription)return;
#endif
 portable_perf_count(PORTABLE_PERF_INPUT_READS);
 (void)portable_touch_collect(&touch);
 if(failed)return;
 input_sampled_at=millis_now();
 if(failed)return;

}
/* Exactly one edge is reduced per controller pass; actions are applied before
 * another edge can toggle the same control. */
static void input_dispatch(void) {
 if(input_pending || failed)return;
#ifdef PORTABLE_TEXT_INPUT_CLIENT
 if(text_input_suspended || text_input_retained)return;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(desk_phase!=DESK_FOREGROUND || !touch.subscription)return;
#endif
 bool navigation_progressed=false;
 /* Navigation poll reduces one provider frame only in the logical pass.
  * Capture-only raster work must not collapse several pressed frames. */
#ifdef PORTABLE_INPUT_NAVIGATION
 if(navigation_ready) {
  const bool was_neutral=navigation_neutral;
  risc_input_navigation_frame_v1 frame={0};
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(!navigation->poll(navigation->context,&frame)) {portable_adapter_retain();return;}
#else
  if(!navigation->poll(navigation->context,&frame))navigation_neutral=false;
#endif
  else if(!navigation_neutral){if(!frame.buttons)navigation_neutral=true;}
  else {perf_navigation(frame.buttons,frame.pressed);navigation_pending|=frame.pressed;
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
    if(frame.pressed&(RISC_NAV_HOME|RISC_NAV_BACK))portable_springboard_scroll_interrupt();
#endif
    if(frame.pressed&RISC_NAV_HOME)portable_stage_log(rt,"action","name=crown");
    else if(frame.pressed&RISC_NAV_BACK)portable_stage_log(rt,"action","name=back");
    else if(frame.pressed)portable_stage_log(rt,"action","name=navigation");
    if(frame.pressed&RISC_NAV_HOME){crown_pending=true;navigation_pending|=T5_APP_BUTTON_BACK;}
  }
  navigation_progressed=frame.buttons || frame.pressed || frame.released || (!was_neutral && navigation_neutral);
#ifdef PORTABLE_DESK_LOCK_HOME
  navigation_home_down=!!(frame.buttons&RISC_NAV_HOME);
#endif
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && defined(PORTABLE_RESIDENT_POLICY)
  if(frame.buttons || frame.pressed || frame.released)resident_activity_pending=true;
#endif
#ifdef PORTABLE_APP_SLEEP_LOCAL
  if(frame.buttons || frame.pressed || frame.released)last_activity=input_sampled_at;
#endif
 }
#endif
 if(failed)return;
 portable_touch_sample next;
 input_progressed=portable_touch_next(&touch,&next);
 /* Hardware timestamps belong to ordered edges. Idle/held level passes use
  * current time so animation/inertia never freeze at the last report. */
 input_delivered_at=input_progressed&&next.timestamp_ms?(uint32_t)next.timestamp_ms:input_sampled_at;
 input_progressed|=navigation_progressed;
 input_progressed|=next.began||next.released||next.cancelled||next.home_pressed;
#if defined(PORTABLE_DISPLAY_SETTLED) && defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
 if(resident_sleep_present && (native_custody_retained || failed))return;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 /* Discard replayed events and the complete initial held contact/Home cycle,
  * including drag gestures. A fresh neutral snapshot arms the next contact. */
 if(!desk_touch_neutral) {
  if(next.valid && !next.down && !touch.home_down)desk_touch_neutral=true;
  next=(portable_touch_sample){.valid=desk_touch_neutral};
 }
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
 paper_orient_input(&next);
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(failed)return;
#endif
 if(next.valid && next.began && next.tap_eligible && !next.cancelled)perf_input_begin(1u);
 if(next.valid && next.began && !next.cancelled)portable_stage_log(rt,"touch-picked-up","source=software-sample");
 if(next.valid && next.released && !next.cancelled)portable_stage_log(rt,"touch-released",next.tap_eligible?"tap=eligible":"tap=no");
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && defined(PORTABLE_RESIDENT_POLICY)
 if(next.valid && (next.down || next.began || next.released || next.home_pressed))resident_activity_pending=true;
#endif
 if(next.home_pressed) {
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
  portable_springboard_scroll_interrupt();
#endif
  portable_stage_log(rt,"action","name=physical-home");
  perf_input_begin(3u);
  home_pending=true;navigation_pending|=T5_APP_BUTTON_BACK;
  next=(portable_touch_sample){.valid=true,.cancelled=true};input_reset();
 }
#ifdef PORTABLE_APP_SLEEP_LOCAL
 if(next.valid && (next.down || next.began || next.released))last_activity=input_sampled_at;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 if(!alarm_modal) {
  bool reserved=pqa_input(&quick.ui,input_delivered_at,next.valid&&!next.cancelled,
      next.down?1u:0u,next.contact_id,
      quick.ui.paper?(int)next.x*480/(int)(paper_rotated?info.height:info.width):next.x,
      quick.ui.paper?(int)next.y*800/(int)(paper_rotated?info.width:info.height):next.y,
      next.tap_eligible&&(quick.ui.paper || (info.width==240&&info.height==240)));
  if(quick.ui.paper && reserved && next.valid && !next.down && !next.cancelled &&
     !pqa_visible(&quick.ui) && !pqa_capture(&quick.ui))reserved=false;
  if(reserved) {
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
   portable_springboard_scroll_interrupt();
#endif
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
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
 resident_input(&next);
#endif
 input_sample=next;input_pending=true;
#ifdef PORTABLE_APP_SLEEP_LOCAL
 if(next.valid && (next.down || next.began || next.released))last_activity=input_sampled_at;
#endif
}
static void input_take(portable_touch_sample *out) {
 if(!input_pending)input_dispatch();
 *out=input_sample;input_pending=false;
}
static void input_navigation_take(t5_app_input_t*out) {
 out->buttons|=navigation_pending & (T5_APP_BUTTON_BACK|T5_APP_BUTTON_CONFIRM|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN);
 navigation_pending=0;
}
/* Raster work captures raw input without reentrant logical dispatch. */
static unsigned raster_pixels;
static bool raster_checkpoint(void) {
 if(failed)return false;
#ifdef PORTABLE_RESIDENT_LOADING
 if(resident_loading_present)return true;
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
 if(resident_sleep_present)return true;
#endif
 if((uint32_t)(millis_now()-input_sampled_at)>=2u)input_service();
 return !failed;
}
#ifdef PORTABLE_RASTER_SNAPSHOT
#include "raster_snapshot_state.inc"
#endif
static inline bool raster_surface_writable(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  return surface.frame || (raster_replaying&&raster_offscreen&&surface.pixels==raster_offscreen) ||
    (raster_layer_paint&&surface.pixels==raster_layer_paint->pixels);
#else
  return surface.frame!=0;
#endif
}
static inline bool raster_lease_mutable(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  return surface.frame&&(!raster_sealed||raster_replaying);
#else
  return surface.frame!=0;
#endif
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
      *sample=(portable_touch_sample){.cancelled=true};input_reset();return;
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
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_FILL,(int32_t[8]){x,y,w,h,color},NULL))return;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(failed)return;
#endif
  if (!raster_surface_writable() || w <= 0 || h <= 0)
    return;
#ifdef PORTABLE_TOUCH_SCROLL
  if(raster_clip_active && !portable_scroll_clip(&raster_clip,&x,&y,&w,&h))return;
#endif
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w, y1 = y + h;
  if (x1 > width())
    x1 = width();
  if (y1 > height())
    y1 = height();
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_replaying){
    if(raster_band_columns){if(x0<raster_band_top)x0=raster_band_top;if(x1>raster_band_bottom)x1=raster_band_bottom;}
    else {if(y0<raster_band_top)y0=raster_band_top;if(y1>raster_band_bottom)y1=raster_band_bottom;}
  }
#endif
  if(x0>=x1 || y0>=y1)return;
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1) {
    /* Rotate/flip the clipped rectangle once, then write packed row spans.
     * Preserve untouched edge bits and stride padding. No pixel allocation or
     * per-pixel coordinate transform/luminance division is required. */
    int left=x0,top=y0,right=x1-1,bottom=y1-1;
    native_point(&left,&top);native_point(&right,&bottom);
    if(left>right){int swap=left;left=right;right=swap;}
    if(top>bottom){int swap=top;top=bottom;bottom=swap;}
    unsigned luminance=((color>>11)&31)*299/31+((color>>5)&63)*587/63+(color&31)*114/31;
    bool black=luminance<500;
    unsigned first=(unsigned)left/8,last=(unsigned)right/8;
    uint8_t first_mask=(uint8_t)(0xffu>>(left&7)),last_mask=(uint8_t)(0xffu<<(7-(right&7)));
    for(int row=top;row<=bottom;row++) {
      uint8_t *p=(uint8_t*)surface.pixels+(size_t)row*surface.stride_bytes;
      if(first==last){uint8_t mask=first_mask&last_mask;if(black)p[first]|=mask;else p[first]&=(uint8_t)~mask;}
      else {
        if(black){p[first]|=first_mask;p[last]|=last_mask;}else{p[first]&=(uint8_t)~first_mask;p[last]&=(uint8_t)~last_mask;}
        if(last>first+1)memset(p+first+1,black?0xff:0,last-first-1);
      }
      /* A logical replay row becomes one-pixel-wide physical rows when
       * rotated. Charge pixels, not physical rows: a health/input call per
       * pixel makes an 800x480 clear perform 384,000 runtime queries. */
      unsigned written=(unsigned)(right-left+1);
      bool input_due=(raster_pixels&511u)+written>=512u;
      raster_pixels+=written;
      if(input_due&&!raster_checkpoint())return;
#ifdef PORTABLE_CONTEXTS_CLIENT
      bool capture_due=(contexts_pixels&511u)+written>=512u;
      contexts_pixels+=written;
      if(capture_due&&!contexts_capture_checkpoint())return;
#endif
    }
    return;
  }
  for (int j = y0; j < y1; ++j)
    for (int i = x0; i < x1; ++i) {
      if(!(++raster_pixels&511u)&&!raster_checkpoint())return;
#ifdef PORTABLE_CONTEXTS_CLIENT
      if(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint())return;
#endif
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
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  /* A synchronous status failure can fall through the trailing timeout path.
   * Retention already owns all grants; even diagnostics must be idempotent. */
  if(native_custody_retained)return;
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
  /* A failed sleep-overlay display callback may already retain custody. Do
   * not ask health/diagnostics for a log timestamp before installing the fence. */
  if(resident_sleep_present){portable_adapter_retain_silent();return;}
#endif
  portable_stage_log(rt,"display-failed",detail);portable_adapter_retain();return;
#endif
  portable_stage_log(rt,"display-failed",detail);
  if(!failed && surface_format==RISC_DISPLAY_FORMAT_MONO1 &&
     (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE))rt->diagnostic(detail);
  failed=true;
}
/* A false callback may already have crossed the provider's terminal boundary.
 * A returned FAILED/SUPERSEDED state or a local timeout still has live Runtime
 * custody and keeps its ordinary diagnostic through display_failure. */
static void display_callback_failure(const char *detail) {
#if defined(PORTABLE_NATIVE_CUSTODY_FENCE) && defined(PORTABLE_QUICK_ACTIONS) && defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_ALARM_TERMINAL_RETENTION)
  if(quick_clean_present || paper_token_clean){portable_adapter_retain_silent();return;}
#endif
  display_failure(detail);
}
/* Acquire the same checked surface for either painting or complete image copy. */
static bool acquire_surface(void) {
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  if(text_input_suspended || text_input_retained)return false;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(desk_phase!=DESK_TIMER && desk_phase!=DESK_FOREGROUND)return false;
#endif
  if (failed)
    return false;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_sealed&&!raster_replaying&&!portable_paper_frame_drain())return false;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  /* Non-cooperative callers retain the old blocking acquisition contract. */
  if(!portable_paper_frame_drain())return false;
#endif
  if (surface.frame) {
    display->release(display->context, surface.frame);
    surface.frame = 0;
  }
  portable_stage_log(rt,"draw-begin","");
  portable_perf_span(PORTABLE_PERF_SPAN_ACQUIRE,true);
  if (!display->acquire(display->context, surface_format,
                        &surface)) {
    display_failure("PORTABLE_APP error=display-acquire");
    return false;
  }
  portable_perf_span(PORTABLE_PERF_SPAN_ACQUIRE,false);
  perf_draw_begin();
  if (!surface.frame || !surface.pixels || surface.width != info.width ||
      surface.height != info.height ||
      surface.pixel_format != surface_format ||
      surface.stride_bytes < (surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2) ||
      surface.stride_bytes > UINT32_MAX / info.height ||
      surface.size_bytes < surface.stride_bytes * info.height) {
    display_failure("PORTABLE_APP error=display-surface");
    return false;
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
        handoff_started=millis_now();handoff_first=false;handoff_active=true;
#ifdef PORTABLE_HANDOFF_EAGER_MS
        /* The outgoing image is already visible. Count initial drawing time
         * and start with incoming content, rather than retransmitting an
         * identical alpha-zero frame before beginning the transition. */
        handoff_started=millis_now();handoff_first=false;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
        if(raster_replaying)handoff_started=raster_handoff_origin;
#endif
      }
    }
  }
#endif
  return true;
}
static void clear_color(uint16_t color) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_begin(color))return;
#endif
  if(acquire_surface())fill(0, 0, width(), height(), color);
}
static void clear(void) { clear_color(0xffff); }
static void rect(int32_t x, int32_t y, int32_t w, int32_t h, bool black) {
  fill(x, y, w, h, black ? 0 : 0xffff);
}
static void rounded(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                    uint8_t tone) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_ROUNDED,(int32_t[8]){x,y,w,h,radius,tone},NULL))return;
#endif
  static const uint16_t colors[] = {0xffff, 0xbdf7, 0x630c, 0};
  if (w <= 0 || h <= 0 || w > 2048 || h > 2048)
    return;
  if (radius < 0)
    radius = 0;
  if (radius > w / 2)
    radius = w / 2;
  if (radius > h / 2)
    radius = h / 2;
  int row_first=0,row_end=h;
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_rows(y,&row_first,&row_end);
#endif
  for (int row = row_first; row < row_end; ++row) {
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
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_TEXT,(int32_t[8]){x,y,limit,color},s))return;
#endif
  if (!s)
    return;
  int row_first=0,row_end=7;
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_rows(y,&row_first,&row_end);
#endif
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
      for (int row = row_first; row < row_end; ++row)
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
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
#include "native_toolbar.inc"
#endif
#include "nova.inc"
#ifdef PORTABLE_NOVA_UI
#include "nova_ui.inc"
#endif
static bool icon(int32_t x, int32_t y, const char *name, uint8_t size, bool black) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_recording&&!raster_replaying) {
    if(!name||size<1||size>96)return false;
    bool known=false;for(unsigned k=0;k<sizeof(rpi_icons)/sizeof(rpi_icons[0]);k++)if(!strcmp(name,rpi_icons[k].name)){known=true;break;}
#if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || defined(PORTABLE_RESIDENT_LOADING)
    if(!strcmp(name,rpi_springboard_gamepad.name))known=true;
#endif
    if(!known)return false;
    if(raster_record(RS_ICON,(int32_t[8]){x,y,size,black},name))return true;
  }
#endif
  /* Preserve the existing call contract; genuine glyph raster, no handmade FA IDs. */
  if (black) {
    /* np_icon normally draws white; invert its bounded output on a white tile. */
    if (!name || size < 1 || size > 96) return false;
    const rpi_glyph *g = NULL;
    for (unsigned i = 0; i < sizeof(rpi_icons)/sizeof(rpi_icons[0]); ++i)
      if (!strcmp(rpi_icons[i].name, name)) { g = &rpi_icons[i]; break; }
#if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || defined(PORTABLE_RESIDENT_LOADING)
    if (!g && !strcmp(name,rpi_springboard_gamepad.name))g=&rpi_springboard_gamepad;
#endif
    if (!g) return false;
    int max = g->width > g->height ? g->width : g->height;
    int w = g->width*size/max, h = g->height*size/max;
    int row_first=0,row_end=h;
#ifdef PORTABLE_RASTER_SNAPSHOT
    raster_rows((int64_t)y+(size-h)/2,&row_first,&row_end);
#endif
    for (int j = row_first; j < row_end; ++j) for (int i = 0; i < w; ++i) {
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
bool portable_paper_frame_ready(void) {
  if(failed)return false;
#ifdef PORTABLE_RASTER_SNAPSHOT
  /* Latest-image mailbox: a completed software image which has not acquired
   * a provider lease may be replaced while the panel is BUSY. Never replace
   * an in-progress capture or an explicit clean/quality submission. */
  if(raster_sealed&&raster_ready_to_submit&&paper_token&&
     raster_saved_intent==RISC_DISPLAY_PRESENT_LOW_LATENCY) {
   raster_free_commands();raster_sealed=raster_ready_to_submit=false;raster_started=false;
  }
  if(raster_sealed||raster_recording||surface.frame)return false;
  raster_begin_allowed=true;
#endif
  paper_async_frames=(info.flags&RISC_DISPLAY_INFO_ASYNC_PRESENT)!=0;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  paper_async_frames=paper_async_frames && desk_phase==DESK_FOREGROUND;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
  return true;
#else
  return !paper_token;
#endif
}
bool portable_paper_frame_idle(void) {
 return !failed&&!paper_token&&!surface.frame
#ifdef PORTABLE_RASTER_SNAPSHOT
   &&!raster_sealed&&!raster_recording
#endif
 ;
}
/* Advance once per foreground poll. Status never gives the app a writable
 * lease; only completion promotes the submitted image into damage history. */
static bool paper_present_progress(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_sealed&&!raster_replaying)return raster_progress();
#endif
  return paper_token_progress();
}
static bool paper_token_progress(void) {
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_capture_checkpoint())return false;
#endif
  if(failed)return false;
  if(!paper_token)return true;
  uint32_t elapsed=(uint32_t)(millis_now()-paper_submitted_at);
  if(failed)return false;
  risc_display_present_status_v1 status={0};
  portable_perf_count(PORTABLE_PERF_STATUS_POLLS);
  if(!display->present_status(display->context,paper_token,&status)) {
    display_callback_failure("PORTABLE_APP error=display-status");return false;
  }
  if(failed)return false;
  if(status.state==RISC_DISPLAY_PRESENT_FAILED || status.state==RISC_DISPLAY_PRESENT_SUPERSEDED) {
    display_failure("PORTABLE_APP error=display-failed");return false;
  }
  if(status.state!=RISC_DISPLAY_PRESENT_COMPLETE) {
    /* A synchronous service call can delay this observer after the hardware
     * has completed. Only a still-pending token can time out on observation. */
    if(elapsed>=10000){display_failure("PORTABLE_APP error=display-timeout");return false;}
    return true;
  }
  stage_display_complete(paper_token);portable_stage_log(rt,"display-complete","result=complete");
  perf_metrics(paper_token);perf_complete(true);
  paper_token=0;paper_token_clean=false;
#ifdef PORTABLE_PAPER_PREFERENCES
  paper_orientation_dirty=false;
#endif
#ifdef PORTABLE_DESK_CLOCK
  desk_present_complete=true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=portable_paper_frame_idle();alarm_pixels_valid=true;
#endif
  previous_valid=previous_pixels!=NULL;paper_previous_valid=paper_previous!=NULL;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_active)previous_valid=false;
#endif
  return true;
}
/* Ownership transitions deliberately settle the outstanding image. Input
 * remains sampled during this short boundary, with the old cancellation rule. */
bool portable_paper_frame_drain(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(!raster_replaying && !raster_drain())return false;
#endif
  if(!paper_token)return !failed;
  for(unsigned n=0;n<10000 && !failed;++n) {
    if(!paper_present_progress())return false;
    if(!paper_token)return true;
#ifdef PORTABLE_RESIDENT_LOADING
    if(!resident_loading_present)
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
    if(!resident_sleep_present)
#endif
    if((uint32_t)(millis_now()-input_sampled_at)>=16)input_service();
    rt->yield_ms(1);
  }
  display_failure("PORTABLE_APP error=display-timeout");return false;
}
#ifdef PORTABLE_PAPER_CROSSFADE
#include "paper_transition.inc"
#endif
static uint32_t present_intent(bool full) {return
#ifdef PORTABLE_RESIDENT_LOADING
    resident_loading_present?RISC_DISPLAY_PRESENT_LOW_LATENCY:
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
    resident_sleep_present?RISC_DISPLAY_PRESENT_LOW_LATENCY:
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
    (quick_clean_present || resident_failure_present) && (info.flags&RISC_DISPLAY_INFO_CLEAN_PRESENT)?RISC_DISPLAY_PRESENT_CLEAN:
#endif
#ifdef PORTABLE_DESK_LOCK_HOME
    desk_quality_present?(full?RISC_DISPLAY_PRESENT_CLEAN:RISC_DISPLAY_PRESENT_QUALITY):
#endif
#if defined(PORTABLE_DESK_CLOCK) && !defined(PORTABLE_DESK_LOCK_HOME)
    /* Legacy desk-only owners explicitly select the normal waveform. */
    surface_format==RISC_DISPLAY_FORMAT_MONO1?(full?RISC_DISPLAY_PRESENT_CLEAN:RISC_DISPLAY_PRESENT_QUALITY):
#endif
    /* Pixel coverage is independent of waveform. Interactive frames, including
     * full-buffer page changes and resident clients without transition code,
     * use the fast provider path. Only explicit desk/clean actions opt out. */
    surface_format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_PRESENT_LOW_LATENCY:
    full && (info.flags&RISC_DISPLAY_INFO_CLEAN_PRESENT)?RISC_DISPLAY_PRESENT_CLEAN:RISC_DISPLAY_PRESENT_DEFAULT;
}
static void present(bool full) {
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  if(text_input_suspended || text_input_retained)return;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_capture_checkpoint())return;
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
  if(paper_orientation_dirty)full=true;
#endif
#ifdef PORTABLE_DESK_CLOCK
  desk_present_complete=false;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_recording&&!failed){
    raster_recording=false;raster_sealed=true;raster_full=full;raster_saved_intent=present_intent(full);
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_RESIDENT_SHELL_HOST)
    raster_saved_clean=quick_clean_present;
#endif
#ifdef PORTABLE_ALARM_CLIENT
    raster_saved_app_frame=!alarm_modal;display_settled=false;
#else
    raster_saved_app_frame=true;
#endif
    return;
  }
#endif
  if (failed || !surface.frame)
    return;
#ifdef PORTABLE_ALARM_CLIENT
  bool app_owned_frame=!alarm_modal;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_replaying)app_owned_frame=raster_saved_app_frame;
#endif
#endif
  portable_stage_log(rt,"draw-end","");
  perf_draw_end();
  portable_perf_span(PORTABLE_PERF_SPAN_DAMAGE,true);
  risc_display_present_token_v1 token = 0;
  risc_display_rect_v1 damage={0};size_t damage_count=0;
#ifdef PORTABLE_PAPER_CROSSFADE
  if(paper_handoff_blend())full=false;
#endif
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
      if(!x && !(y&7u) && !raster_checkpoint())return;
      uint8_t value=((uint8_t*)surface.pixels)[(size_t)y*surface.stride_bytes+x];
      if(value!=paper_previous[(size_t)y*row_bytes+x]) {
        if(x<left)left=x;
        if(x+1>right)right=x+1;
        if(y<top)top=y;
        if(y+1>bottom)bottom=y+1;
      }
    }
    if(paper_previous_valid && !full && top==info.height){display->release(display->context,surface.frame);surface.frame=0;
#ifdef PORTABLE_ALARM_CLIENT
      display_settled=true;
#endif
#ifdef PORTABLE_DESK_CLOCK
      desk_present_complete=true;
#endif
      portable_perf_span(PORTABLE_PERF_SPAN_DAMAGE,false);perf_unchanged();
      portable_stage_log(rt,"display-skip","reason=pixels-unchanged");
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
      if(!(y&7u) && !raster_checkpoint())return;
      if(memcmp(previous_pixels+(size_t)y*info.width,(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,info.width*2)) {
        if(first==info.height)first=y;
        last=y+1;
      }
    }
    if(previous_valid && first==info.height){display->release(display->context,surface.frame);surface.frame=0;
#ifdef PORTABLE_ALARM_CLIENT
      display_settled=true;
#endif
      portable_perf_span(PORTABLE_PERF_SPAN_DAMAGE,false);perf_unchanged();return;}
    if(previous_valid){damage=(risc_display_rect_v1){0,(int32_t)first,info.width,last-first};damage_count=1;}
    for(unsigned y=0;y<info.height;y++)memcpy(previous_pixels+(size_t)y*info.width,(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,info.width*2);
    previous_valid=false;
  }
  portable_perf_span(PORTABLE_PERF_SPAN_DAMAGE,false);
  const risc_display_present_options_v1 options = {
#ifdef PORTABLE_RASTER_SNAPSHOT
    raster_replaying?raster_saved_intent:
#endif
    present_intent(full),
    RISC_DISPLAY_QUEUE_FIFO, 0};
#ifdef PORTABLE_STAGE_LOGS
  char display_line[112];
  snprintf(display_line,sizeof(display_line),"intent=%s damage=%ld,%ld,%lu,%lu",
    options.intent==RISC_DISPLAY_PRESENT_LOW_LATENCY?"low-latency":
    options.intent==RISC_DISPLAY_PRESENT_CLEAN?"clean":"normal",
    (long)damage.x,(long)damage.y,(unsigned long)(damage_count?damage.width:info.width),
    (unsigned long)(damage_count?damage.height:info.height));
  portable_stage_log(rt,"display-submit-begin",display_line);
#endif
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=false;
  if(app_owned_frame) {
    alarm_pixels_valid=false;
    unsigned bytes=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
    for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)alarm_pixels+(size_t)y*bytes,
      (uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,bytes);
  }
#endif
  perf_submit();portable_perf_span(PORTABLE_PERF_SPAN_SUBMIT,true);
  if (!display->submit(display->context, surface.frame, damage_count?&damage:NULL, damage_count, &options,
                       &token)) {
    display_callback_failure("PORTABLE_APP error=display-submit");
    return;
  }
  portable_perf_span(PORTABLE_PERF_SPAN_SUBMIT,false);
  portable_stage_log(rt,"display-submit-end","result=accepted");
#if defined(PORTABLE_DISPLAY_SETTLED) && defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
  if(resident_sleep_present)resident_sleep_token=token;
#endif
  surface.frame = 0;
  if(paper_async_frames) {
    paper_token=token;paper_submitted_at=millis_now();
#if defined(PORTABLE_QUICK_ACTIONS) && defined(PORTABLE_RESIDENT_SHELL_HOST)
    paper_token_clean=quick_clean_present;
#endif
    return;
  }
  uint32_t start = millis_now();
  for (unsigned n = 0; n < 10000 && !failed; ++n) {
#ifdef PORTABLE_USB_TRANSFER_APP
    portable_usb_transfer_service();
#endif
    risc_display_present_status_v1 s = {0};
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!contexts_capture_checkpoint())return;
#endif
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
    portable_perf_count(PORTABLE_PERF_STATUS_POLLS);
    bool ok=waited?display->wait_present(display->context,token,10000-elapsed,&s):
      display->present_status(display->context,token,&s);
    if(!ok){display_callback_failure(waited?"PORTABLE_APP error=display-wait":"PORTABLE_APP error=display-status");break;}
    if(s.state==RISC_DISPLAY_PRESENT_FAILED || s.state==RISC_DISPLAY_PRESENT_SUPERSEDED) {
      display_failure("PORTABLE_APP error=display-failed");break;
    }
    if (s.state == RISC_DISPLAY_PRESENT_COMPLETE){
      stage_display_complete(token);portable_stage_log(rt,"display-complete","result=complete");
      perf_metrics(token);perf_complete(true);
#ifdef PORTABLE_PAPER_PREFERENCES
      paper_orientation_dirty=false;
#endif
#ifdef PORTABLE_DESK_CLOCK
      desk_present_complete=true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
      display_settled=true;if(app_owned_frame)alarm_pixels_valid=true;
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
#ifdef PORTABLE_RESIDENT_LOADING
    if(!resident_loading_present)
#endif
#if defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
    if(!resident_sleep_present)
#endif
    if((uint32_t)(millis_now()-input_sampled_at)>=16)input_service();
    rt->yield_ms(1);
  }
  display_failure("PORTABLE_APP error=display-timeout");
}
#ifdef PORTABLE_APP_SLEEP_LOCAL
static bool idle_sleep(void) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
  if(portable_adapter_retained())return false;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
 int status=resident_checkpoint(RISC_RESIDENT_CHECKPOINT_SLEEP);
 return status==RISC_RESIDENT_OK || status==RISC_RESIDENT_BUSY || status==RISC_RESIDENT_EXIT;
#else
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || (desk_phase!=DESK_TIMER && desk_phase!=DESK_FOREGROUND))return false;
  if(desk_phase==DESK_TIMER) {
    if(surface.frame || !display_settled)return false;
    /* The alarm-service dependency closure remains live. Do not acquire touch,
     * navigation, battery, preferences or radios just to prepare them for sleep. */
    int status=portable_app_alarm_sleep(rt,display,NULL,alarm_sleep_api());
    if(status==-2){
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
      portable_adapter_retain_silent();
#endif
      portable_desk_adapter_retain();return false;}
    if(status<0)failed=true;
    return !failed;
  }
#endif
#ifdef PORTABLE_DESK_CLOCK
  int desk_mode=
#if defined(PORTABLE_X4_IDLE_POLICY) && !defined(PORTABLE_DESK_LOCK_HOME)
    automatic_idle?0:
#endif
    portable_desk_clock_mode();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(desk_mode<0){portable_desk_adapter_retain();return false;}
#else
  if(desk_mode<0){native_sleep_retained=true;failed=true;return false;}
#endif
#endif
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
  if(!portable_background_stop())return false;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_suspend())return false;
#endif
  /* Existing app stack, editor draft and private storage grants stay live.
   * No handoff, unload or settings grant is introduced by idle sleeping. */
  if(surface.frame)return true;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(paper_token)return true;
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close()){last_activity=millis_now();return !failed;}
#endif
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  if(handoff_active || handoff_pending)return true;
#endif
#ifdef PORTABLE_PAPER_CROSSFADE
  /* Sleep has its own scene and ownership transition. Do not blend that scene
   * against an interrupted app handoff, or delay an explicit sleep request. */
  portable_paper_transition_cancel();
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
#ifdef PORTABLE_X4_IDLE_POLICY
     !automatic_idle &&
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
#ifdef PORTABLE_X4_IDLE_POLICY
  int status=
#ifdef PORTABLE_DESK_CLOCK
#ifndef PORTABLE_DESK_LOCK_HOME
    automatic_idle?portable_app_idle_sleep(rt,display,gauge,alarm_sleep_api()):
#endif
    portable_app_alarm_sleep(rt,display,gauge,alarm_sleep_api());
#else
    portable_app_idle_sleep(rt,display,gauge,alarm_sleep_api());
#endif
#else
  int status=portable_app_alarm_sleep(rt,display,gauge,alarm_sleep_api());
#endif
#else
  int status=portable_app_sleep(rt,display,gauge);
#endif
#ifdef PORTABLE_ALARM_CLIENT
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(status==-2){
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
      portable_adapter_retain_silent();
#endif
      portable_desk_adapter_retain();return false;}
#else
  if(status==-2){
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    portable_adapter_retain_silent();
#else
    portable_adapter_retain();
#endif
#else
    native_sleep_retained=true;failed=true;
#endif
    return false;
  }
#endif
#endif
  if(status<0){failed=true;return false;}
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(quick.ui.paper && !pqa_session_sync_tone(&quick,display)){
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
   /* A refused provider operation may already own terminal custody. */
   portable_adapter_retain_silent();
#else
   portable_adapter_retain();
#endif
   return false;}
#endif
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
#ifdef PORTABLE_X4_IDLE_POLICY
  if(automatic_idle) {
    paper_previous_valid=false; /* Panel resume invalidates physical history. */
  }
#endif
  last_activity=last_poll_at=input_sampled_at=millis_now();previous_valid=false;
#ifdef PORTABLE_DESK_CLOCK
  if(failed && desk_mode==1)native_sleep_retained=true;
#endif
  rt->diagnostic(status?"PORTABLE_APP sleep=resumed":"PORTABLE_APP sleep=refused");
  return !failed;
#endif /* resident foreground never sleeps with a native child stack */
}
#endif
#ifdef PORTABLE_PAPER_PREFERENCES
/* Only a settled foreground can change orientation. Invalidate both physical
 * history and modal restoration images before the next complete repaint.
 * Do not touch retained custody; do not apply while a modal owns resources. */
static inline bool paper_apply_flip(unsigned value) {
  if(failed || value>1u || !pp_enabled() || surface_format!=RISC_DISPLAY_FORMAT_MONO1)return false;
  if(paper_flip_ui==(value!=0))return true;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(!portable_paper_frame_drain())return false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  if(native_sleep_retained || alarm_modal || !display_settled)return false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick_modal || pqa_visible(&quick.ui) || (pqa_capture(&quick.ui)
#ifdef PORTABLE_DESK_LOCK_HOME
      /* A post-cancel neutral gate owns no scene or provider. Timer startup
       * and portrait restoration must preserve, rather than wait on, it. */
      && !quick.ui.neutral_gate
#endif
      ) || quick.ui.pending)return false;
  pqa_cancel(&quick.ui);quick_replay_pending=quick_replay_delivery=false;
#endif
  if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
  paper_previous_valid=previous_valid=false;
#ifdef PORTABLE_ALARM_CLIENT
  alarm_pixels_valid=false;
#endif
  input_pending=false;input_sample=(portable_touch_sample){0};
  input_reset();touch.home_neutral=touch.home_down=false;
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
#ifdef PORTABLE_DESK_LOCK_HOME
bool portable_desk_adapter_landscape(bool landscape,unsigned direction) {
 if(failed || direction>1u || !pp_enabled() || surface_format!=RISC_DISPLAY_FORMAT_MONO1)return false;
 const bool rotated=!landscape && PORTABLE_DISPLAY_ROTATION==90 && info.width>info.height;
 if(rotated==paper_rotated)return paper_apply_flip(direction);
 if(!portable_paper_frame_drain() || !display_settled || native_sleep_retained || alarm_modal)return false;
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_modal || pqa_visible(&quick.ui) || (pqa_capture(&quick.ui)&&!quick.ui.neutral_gate) || quick.ui.pending)return false;
#endif
 if(!paper_apply_flip(direction))return false;
 if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
 paper_rotated=rotated;paper_orientation_dirty=true;
 paper_previous_valid=previous_valid=alarm_pixels_valid=false;
 return true;
}
#endif
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
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
 /* Native desk entry/restore may already own the terminal boundary. Fence
  * first; even a log timestamp would be provider I/O after that return. */
 portable_adapter_retain_silent();
#else
 portable_adapter_retain();
#endif
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
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(!raster_immediate())return;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(!portable_desk_adapter_ready())return;
#endif
  if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
  paper_previous_valid=false;previous_valid=false;
}
void portable_desk_adapter_begin(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(!raster_immediate())return;
#endif
  list_mode=false;
  /* History is attached to this lease. Repaint it in place: releasing and
   * reacquiring here would invalidate the provider's one-shot seeded image. */
  if(!failed && surface.frame)fill(0,0,width(),height(),0xffff);
  else pp_begin();
}
int portable_desk_adapter_seed(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(!raster_immediate())return -2;
#endif
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
#ifdef PORTABLE_DESK_LOCK_HOME
  desk_quality_present=true;
#endif
  present(full);
#ifdef PORTABLE_DESK_LOCK_HOME
  desk_quality_present=false;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(!portable_paper_frame_drain())return false;
#endif
  return !failed && desk_present_complete;
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
#ifdef PORTABLE_CONTEXTS_CLIENT
#include "contexts_policy.inc"
#endif
#ifdef PORTABLE_APP_TOUCH_SCROLL
bool portable_paper_scroll_settled(void) {return portable_paper_frame_idle();}
bool portable_paper_scroll_available(void) {
 if(failed)return false;
#ifdef PORTABLE_ALARM_CLIENT
 if(alarm_modal)return false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_modal||quick.ui.gesture==PQA_TOP_PENDING||pqa_visible(&quick.ui))return false;
#endif
 return !home_pending&&!crown_pending;
}
#endif
static void clear_contact_snapshots(void) {
#ifdef PORTABLE_APP_TOUCH_SCROLL
 paper_scroll_sample=(portable_touch_sample){.cancelled=true};paper_scroll_sampled_at=input_sampled_at;
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
  memset(&file_scroll_sample,0,sizeof(file_scroll_sample));file_scroll_sampled_at=input_sampled_at;
#endif
  nova_contact=(springboard_contact){0};
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
}
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
static bool wifi_scroll_poll(t5_app_input_t *out);
#endif
#include "resident_adapter.inc"
#include "resident_shell.inc"
static bool poll_input(t5_app_input_t *out, uint32_t wait) {
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_message_foreground())return false;
  if(context_launch_pending){memset(out,0,sizeof(*out));out->exit_requested=true;return true;}
#endif
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
  if(consumed){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    crown_pending=false;return true;}
#endif
#ifdef PORTABLE_BLE_BROADCAST
  /* Background model/service progression shares the owner task, not the
   * display completion clock. Mutable raster custody is still excluded. */
  if(!raster_lease_mutable() && !broadcast_tick())return false;
#endif
  /* A rejected paper top-edge gesture replays its original down followed
   * by the saved current sample before sampling another contact. */
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick_replay_delivery){quick_replay_delivery=false;goto replay_input;}
#endif
  uint32_t now=millis_now(),spent=now-last_poll_at;
  if(failed)return false;
  input_service();input_dispatch();
  if(failed)return false;
  const uint32_t idle_budget=spent<wait?wait-spent:0;
  uint32_t delay=idle_budget;
  while(delay && !input_progressed && !navigation_pending && !failed) {
    uint32_t slice=delay>4u?4u:delay;
#ifdef PORTABLE_USB_TRANSFER_APP
    if(slice>2u)slice=2u;
    portable_usb_transfer_service();
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
    if(raster_sealed&&!raster_ready_to_submit)slice=1u;
#endif
    rt->yield_ms(paper_token?1u:slice);
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!contexts_capture_checkpoint())return false;
#endif
    if(!paper_present_progress())return false;
    input_pending=false;input_service();input_dispatch();
    uint32_t elapsed=(uint32_t)(millis_now()-now);
    delay=elapsed>=idle_budget?0:idle_budget-elapsed;
  }
  if(input_progressed || navigation_pending || !wait)rt->yield_ms(1);
  last_poll_at=millis_now();
  if(!paper_present_progress() || failed)return false;
#ifdef PORTABLE_ALARM_CLIENT
  if(!alarm_foreground(&consumed))return false;
  if(consumed){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    crown_pending=false;return true;}
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
  /* A recognized pull-down is model state. Keep it while an existing image
   * still owns the display, and continue child input/services in the meantime.
   * Once custody is clear, the ordinary checkpoint attempts it exactly once;
   * a user-declined edit/launch guard must not turn into repeated prompts. */
  bool resident_display_ready=!surface.frame && !paper_token && display_settled && !alarm_modal;
  if(resident_display_ready) {
    uint32_t reason=resident_controls_requested?RISC_RESIDENT_CHECKPOINT_CONTROLS:RISC_RESIDENT_CHECKPOINT_POLL;
    int status=resident_checkpoint(reason);
    if(status==RISC_RESIDENT_RETAINED)return false;
    if(status==RISC_RESIDENT_EXIT){out->exit_requested=true;return true;}
    if(status<0){failed=true;return false;}
#ifdef PORTABLE_RESIDENT_POLICY
    if(reason==RISC_RESIDENT_CHECKPOINT_POLL && status==RISC_RESIDENT_OK && resident_policy_pending &&
       resident_policy_ready()) {
      status=resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY);
      if(status==RISC_RESIDENT_RETAINED)return false;
      if(status==RISC_RESIDENT_EXIT){out->exit_requested=true;return true;}
      if(status<0){failed=true;return false;}
      return true;
    }
#endif
    if(reason==RISC_RESIDENT_CHECKPOINT_CONTROLS) {
      resident_controls_requested=false;return true;
    }
  }
#endif
#ifdef PORTABLE_LOW_BATTERY
  if(!low_battery_poll()){failed=true;return false;}
#endif
#ifdef PORTABLE_CRASH_REPORT_SD
  if(!failure_archive_checkpoint())return false;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!raster_lease_mutable()&&!contexts_tick())return false;
#if defined(PORTABLE_CONTEXTS_CLOCK_RF_ONLY) && !defined(PORTABLE_RESIDENT_SHELL_HOST)
  if(contexts_clock_handoff){out->exit_requested=true;return true;}
#endif
#endif
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
  portable_audio_capture_resume();
#endif
#if defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_SLEEP_MANUAL_ONLY)
  if(!failed && (uint32_t)(millis_now()-last_activity)>=portable_idle_ms() &&
     !idle_capture_active() &&
#ifdef PORTABLE_X4_IDLE_POLICY
     automatic_idle_ready() &&
#endif
     !navigation_pending && !(input_pending && (input_sample.down || input_sample.released))) {
#ifdef PORTABLE_DESK_LOCK_HOME
    /* Home's timeout enters the same desk owner as its explicit lock. Other
     * foreground apps keep the reversible Light helper and suspended stack. */
    if(!portable_desk_clock_request_lock(true))return !failed;
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
    automatic_idle=true;
#endif
    if(!idle_sleep())return false;
#ifdef PORTABLE_DESK_LOCK_HOME
    if(!portable_desk_clock_request_lock(false))return false;
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
    automatic_idle=false;
#endif
    return !failed; /* Waking crown/contact never becomes an app action. */
  }
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  bool quick_consumed=false;
  if(!quick_foreground(&quick_consumed))return false;
  if(quick_consumed){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    #if !defined(PORTABLE_RESIDENT_SHELL_HOST) && (!defined(PORTABLE_UPDATE_APP) || !defined(PORTABLE_HOME_APP))
    crown_pending=false;
#endif
    out->exit_requested=quick_launch_pending;
    /* A reserved gesture is not app contact. Keep the presentation neutral
     * without an invented release, so deliberate replay can begin normally. */
    if(quick.ui.gesture==PQA_TOP_PENDING && !pqa_visible(&quick.ui))nova_contact.valid=true;
    input_pending=false;
    return true;
  }
replay_input:
#endif
#ifdef PORTABLE_HOME_APP
  /* Physical root navigation must not first trigger a nested app Back/redraw. */
  if(home_pending||crown_pending){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    navigation_pending=0;input_pending=false;return !failed;}
#endif
  if (nova_mode) { np_poll(out); input_navigation_take(out); return !failed; }
#ifdef PORTABLE_SETTINGS_APP
  if (settings_view_poll(out)) return !failed;
#endif
  input_navigation_take(out);
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
  if(wifi_scroll_poll(out))return !failed;
#endif
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
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  if(text_input_suspended || text_input_retained){memset(out,0,sizeof(*out));return false;}
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed){memset(out,0,sizeof(*out));return false;}
#endif
  if(handoff_requested){clear_contact_snapshots();memset(out,0,sizeof(*out));out->exit_requested=true;return true;}
  bool ok=poll_input(out,wait);
#ifdef PORTABLE_PERFORMANCE_TRACE
  if(ok && (out->tapped || out->buttons))portable_perf_event(RISC_PERF_RECOGNIZED,PORTABLE_PERF_INPUT_DELIVERED);
#endif
#ifdef PORTABLE_ALARM_CLIENT
  if(!ok)return alarm_failure();
#endif
#ifdef PORTABLE_CROWN_SLEEP_LOCAL
#ifdef PORTABLE_DESK_LOCK_HOME
  if(ok && crown_pending) {
    desk_lock_armed=true;crown_pending=home_pending=false;navigation_pending=0;input_pending=false;
    portable_stage_log(rt,"action","name=desk-lock-armed");
  }
  if(ok && desk_lock_armed) {
    memset(out,0,sizeof(*out));
    if(navigation_home_down)return true;
    if(!portable_paper_frame_drain())return false;
    /* A mutable, unsubmitted foreground image cannot postpone and silently
     * consume an explicit lock. The desk owner paints a complete new scene. */
    portable_desk_adapter_invalidate();
    desk_lock_armed=false;
    portable_stage_log(rt,"action","name=desk-lock-enter");
    if(!portable_desk_clock_request_lock(true)){rt->diagnostic("DESK_CLOCK lock=refused configuration");return !failed;}
    if(!idle_sleep())return false;
    return portable_desk_clock_request_lock(false);
  }
#else
  if(ok && crown_pending) {
    crown_pending=home_pending=false;navigation_pending=0;input_pending=false;
    memset(out,0,sizeof(*out));
    return idle_sleep();
  }
#endif
#endif
#if defined(PORTABLE_RETURN_APP) || defined(PORTABLE_HOME_APP)
  const char *destination=NULL;
  bool direct_home=false;
#ifdef PORTABLE_HOME_APP
  if(home_pending||crown_pending){destination=PORTABLE_HOME_APP;direct_home=true;}
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
#ifdef PORTABLE_WIFI_SETTINGS_APP
  if(!destination)destination=wifi_deferred_return;
#endif
  home_pending=false;
  if(ok && destination
#ifdef PORTABLE_QUICK_ACTIONS
     && !quick_launch_pending
#endif
  ) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(!portable_paper_frame_drain())return false;
#endif
    if(!(direct_home?app_allows_home(destination):app_allows_launch(destination))) {
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
      /* A direct Home request survives ordinary asynchronous stop. The next
       * owner poll retries this exact handoff after checked quiescence. */
      bool pending=portable_wifi_stop_pending();
      home_pending=direct_home && pending;
      wifi_deferred_return=pending?destination:NULL;
      crown_pending=false;navigation_pending=0;input_pending=false;
      clear_contact_snapshots();memset(out,0,sizeof(*out));return !failed;
    }
#endif
#ifdef PORTABLE_WIFI_SETTINGS_APP
    wifi_deferred_return=NULL;
#endif
    portable_perf_action(PORTABLE_PERF_LAUNCH,true);
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
    /* Home ends this child cleanly. Runtime restores the one live host. */
    bool requested=!strcmp(destination,"default.elf") || rt->request_launch(destination);
#elif defined(PORTABLE_RESIDENT_SHELL_HOST)
    bool requested=!strcmp(destination,"default.elf") || resident_queue_launch(destination);
#else
    bool requested=rt->request_launch(destination);
#endif
    if(!requested) {
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
#ifdef PORTABLE_RESIDENT_SHELL_HOST
    resident_neutral();memset(out,0,sizeof(*out));
#else
    handoff_requested=true;out->exit_requested=true;
#endif
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
#ifdef PORTABLE_TEXT_INPUT_CLIENT
#include "text_input_handoff.inc"
#endif
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
#define PORTABLE_SELECTED_CATALOG_BOUND PORTABLE_SPRINGBOARD_CATALOG_BOUND
#else
#define PORTABLE_SELECTED_CATALOG_BOUND 18
#endif
static bool refresh(void) { return portable_catalog_count <= PORTABLE_SELECTED_CATALOG_BOUND; }
static uint32_t count(void) {
  return portable_catalog_count <= PORTABLE_SELECTED_CATALOG_BOUND ? portable_catalog_count : 0;
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
  if(i<count())portable_stage_log(rt,"app-launch-request",portable_catalog[i].file_name);
#if defined(PORTABLE_NATIVE_CUSTODY_FENCE) || defined(PORTABLE_RASTER_SNAPSHOT)
  /* The final ownership boundary settles any sealed software frame before
   * deciding whether its retained transition has finished. Ordinary model
   * polling stays independent of raster and display completion. */
  if(!portable_paper_frame_drain())return false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  bool consumed=false;
  if(!alarm_foreground(&consumed) || consumed)return false;
#endif
  portable_perf_action(PORTABLE_PERF_LAUNCH,true);
  return !failed &&
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
      !handoff_active &&
#endif
      !(input_pending && (input_sample.down || input_sample.cancelled)) && !(navigation_pending&T5_APP_BUTTON_BACK) && i < count() && app_allows_launch(portable_catalog[i].file_name) &&
#ifdef PORTABLE_RESIDENT_SHELL_HOST
      resident_queue_launch(portable_catalog[i].file_name);
#else
      rt->request_launch(portable_catalog[i].file_name);
#endif
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
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(failed)return false;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(failed || desk_phase!=DESK_FOREGROUND)return false;
#endif
  portable_perf_count(PORTABLE_PERF_BATTERY_READS);
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
#ifdef PORTABLE_RASTER_SNAPSHOT
#if defined(PORTABLE_RASTER_SNAPSHOT)&&(defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL)||defined(RASTER_SBH_TEST))
#include "springboard_header_replay.inc"
#endif
#include "raster_snapshot_replay.inc"
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
                                  .frame_ready = portable_paper_frame_ready,
                                  .frame_drain = portable_paper_frame_drain,
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
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  if(v!=1 || !settings_initialized || native_custody_retained)return NULL;
  if(!settings_started) {
    settings_started=true;
    if(initialize_providers()) {portable_adapter_retain();return NULL;}
  }
#endif
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
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  if(text_input_retained)return -1;
  text_input_suspended=false;
#endif
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
#if defined(PORTABLE_RESIDENT_SHELL_HOST) || defined(PORTABLE_RESIDENT_SHELL_CLIENT)
  /* Check the appended getter before any guarded Runtime table copy. Role
   * binding itself is deferred until app_main, as required by the Runtime. */
  if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE || !rt->resident_shell)return -1;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained || rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE || !rt->retain_invocation)return -1;
  native_custody_runtime=rt;
  PORTABLE_TOUCH_ISSUE(NULL,0,0);
#endif
  dg.struct_size = sizeof(dg);
#ifdef PORTABLE_PAPER_CROSSFADE
  paper_handoff_old=NULL;paper_handoff_clock=false;
#endif
  bg.struct_size = sizeof(bg);
  failed = false;perf_initialize();
  paper_async_frames=false;paper_token=0;paper_token_clean=false;paper_submitted_at=0;
#ifdef PORTABLE_PAPER_PREFERENCES
  paper_flip_ui=paper_orientation_dirty=false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  display_settled=true;alarm_pixels_valid=alarm_modal=native_sleep_retained=false;alarm_pixels=NULL;
  alarm_error_seen=alarm_failed_cleaned=false;memset(&alarms,0,sizeof(alarms));
#ifdef PORTABLE_WIFI_SERVICE_LEASE
  alarm_refresh_pending=alarm_ack_pending=alarm_service_deferred=false;
#endif
#endif
#ifdef PORTABLE_QUICK_ACTIONS
#ifdef PORTABLE_LOW_BATTERY
  low_battery=(portable_low_battery){0};low_battery_sampled=false;low_battery_sampled_at=0;
#ifdef PORTABLE_WIFI_SETTINGS_APP
  low_battery_transition_pending=false;
#endif
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
  automatic_idle=false;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  desk_kv_reset();
#endif
#endif
#ifdef PORTABLE_WIFI_SETTINGS_APP
  quick_deferred_actions=0;quick_deferred_destination=NULL;
#endif
  pqa_session_init(&quick);quick_background=NULL;quick_modal=quick_launch_pending=quick_replay_pending=quick_replay_delivery=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
  quick_open_requested=false;
#endif
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
#if defined(PORTABLE_WIFI_SETTINGS_APP) && (defined(PORTABLE_RETURN_APP) || defined(PORTABLE_HOME_APP))
  wifi_deferred_return=NULL;
#endif
  list_mode = false;input_pending=home_pending=crown_pending=handoff_requested=false;navigation_pending=0;previous_valid=false;
#ifdef PORTABLE_DESK_LOCK_HOME
  desk_lock_armed=navigation_home_down=desk_quality_present=false;
#endif
  input_progressed=false;raster_pixels=0;input_delivered_at=input_sampled_at=last_poll_at=0;previous_pixels=NULL;paper_previous=NULL;paper_previous_valid=false;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_old=handoff_scratch=NULL;handoff_pending=false;handoff_active=false;handoff_first=false;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  desk_runtime=rt;
  /* Runtime extensions are optional: never read beyond an older table. */
  memset(&desk_guarded_runtime,0,sizeof(desk_guarded_runtime));
  memcpy(&desk_guarded_runtime,rt,rt->struct_size<sizeof(desk_guarded_runtime)?rt->struct_size:sizeof(desk_guarded_runtime));
  desk_guarded_runtime.acquire=desk_acquire;desk_guarded_runtime.release=desk_release;
  desk_guarded_runtime.health=desk_guard_health;desk_guarded_runtime.yield_ms=desk_guard_yield;
  desk_guarded_runtime.diagnostic=desk_guard_diagnostic;desk_guarded_runtime.request_launch=desk_guard_launch;
#ifdef PORTABLE_QUICK_RADIOS
  desk_quick_runtime=desk_guarded_runtime;desk_quick_runtime.acquire=desk_quick_acquire;
#endif
  rt=&desk_guarded_runtime;desk_phase=DESK_INITIALIZED;
  return 0;
#else
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  /* Runtime extensions are optional: never read beyond an older table. */
  memset(&settings_runtime,0,sizeof(settings_runtime));
  memcpy(&settings_runtime,rt,rt->struct_size<sizeof(settings_runtime)?rt->struct_size:sizeof(settings_runtime));
  settings_runtime.acquire=custody_acquire;settings_runtime.release=custody_release;
  settings_runtime.health=custody_health;settings_runtime.yield_ms=custody_yield;
  settings_runtime.diagnostic=custody_diagnostic;settings_runtime.request_launch=custody_launch;
  rt=&settings_runtime;settings_initialized=true;return 0;
#else
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
  /* Runtime extensions are optional: never read beyond an older table. */
  memset(&settings_runtime,0,sizeof(settings_runtime));
  memcpy(&settings_runtime,rt,rt->struct_size<sizeof(settings_runtime)?rt->struct_size:sizeof(settings_runtime));
  settings_runtime.acquire=custody_acquire;settings_runtime.release=custody_release;
  settings_runtime.health=custody_health;settings_runtime.yield_ms=custody_yield;
  settings_runtime.diagnostic=custody_diagnostic;settings_runtime.request_launch=custody_launch;
  rt=&settings_runtime;
#endif
#include "foreground_adapter_open.inc"
#endif
#endif
}
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
static int initialize_providers(void) {
#include "foreground_adapter_open.inc"
}
#endif

#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
#include "sparse_clock_adapter.inc"
#endif
#if defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_NATIVE_TIME_TOOLBAR) || defined(PORTABLE_RESIDENT_SHELL_CLIENT)
static void settings_native_finalize(void) {
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  if(native_custody_retained || !rt || !settings_started)return;
#else
  if(native_custody_retained || !rt)return;
#endif
  if(paper_token && !portable_paper_frame_drain())return;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
  if(!portable_background_stop())return;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_suspend())return;
#endif
  /* Foreground app-owned grants precede adapter teardown. Unconfirmed app
   * cleanup pins the entire invocation before any provider release or free. */
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close()){portable_adapter_retain();return;}
  if(native_custody_retained)return;
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  if(!portable_wifi_close()){portable_adapter_retain();return;}
  if(native_custody_retained)return;
#endif
#ifdef PORTABLE_RADIO_SESSION
  if(!portable_radio_suspend()){portable_adapter_retain();return;}
  if(native_custody_retained)return;
#endif
#ifdef PORTABLE_AUDIO_SESSION
  if(!portable_audio_suspend()){portable_adapter_retain();return;}
  if(native_custody_retained)return;
#endif
#ifdef PORTABLE_ALARM_CLIENT
  if(alarms.api && !alarm_failed_cleaned) {
    if(!portable_alarm_status(&alarms) || alarms.status.output_uncertain) {portable_adapter_retain();return;}
    if((failed || !display_settled || portable_alarm_owned(&alarms)) && !portable_alarm_failure_stop(&alarms)) {portable_adapter_retain();return;}
  }
  if(!portable_alarm_close(&alarms,rt))return;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick.ui.torch && !pqa_session_restore(&quick,display)){portable_adapter_retain();return;}
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_discard();
#endif
  if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
#ifdef PORTABLE_INPUT_NAVIGATION
  if(navigation_ready) {
    if(!navigation->foreground(navigation->context,NULL,0) || !navigation->reset(navigation->context)) {portable_adapter_retain();return;}
    navigation_ready=false;
  }
  if(navigation_grant.api && !rt->release(&navigation_grant))return;
  navigation=NULL;
#endif
  if(!portable_touch_close(&touch,rt)){portable_adapter_retain();return;}
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  if(settings_grant.api && !rt->release(&settings_grant))return;
  settings_store=NULL;sv_active=false;
#endif
  if(bg.api && !rt->release(&bg))return;
  if(dg.api && !rt->release(&dg))return;
#ifdef PORTABLE_ALARM_CLIENT
  free(alarm_pixels);alarm_pixels=NULL;alarm_pixels_valid=false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  free(quick_background);quick_background=NULL;
#endif
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) && !defined(PORTABLE_SETTINGS_NATIVE_TIME) && !defined(PORTABLE_NATIVE_TIME_TOOLBAR)
  np_close();if(native_custody_retained)return;
#endif
  free(previous_pixels);previous_pixels=NULL;previous_valid=false;
  free(paper_previous);paper_previous=NULL;paper_previous_valid=false;
#ifdef PORTABLE_PAPER_CROSSFADE
  paper_handoff_finish();
#endif
  gauge=NULL;display=NULL;
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  settings_started=false;settings_initialized=false;
#endif
}
#endif
__attribute__((visibility("default"))) void app_module_fini(void) {
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  if(text_input_retained)return;
  if(text_input_suspended){portable_text_adapter_retain();return;}
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained)return;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
  /* Failed legacy invocations still reach their original confirmed cleanup.
   * Retained fences above and provider-owned tokens below remain authoritative. */
  if(raster_sealed&&!failed&&!raster_drain())return;
#endif
  if(paper_token && !portable_paper_frame_drain())return;
#ifdef PORTABLE_USB_TRANSFER_APP
  if(rt && !portable_usb_transfer_close()) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
    portable_adapter_retain();return;
#endif
    rt->diagnostic("USB transfer still owns media; invocation retained");
    for(;;){portable_usb_transfer_service();rt->yield_ms(2);}
  }
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(native_custody_retained)return;
#endif
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  if(!settings_started)return;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  desk_finalize();
#elif defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_NATIVE_TIME_TOOLBAR) || defined(PORTABLE_RESIDENT_SHELL_CLIENT)
  settings_native_finalize();
#else
#ifdef PORTABLE_PAPER_PREFERENCES
  if(paper_preferences_retained)return;
#endif
  if (!rt)
    return;
#ifdef PORTABLE_ALARM_CLIENT
  if(native_sleep_retained)return; /* Runtime normally blocks fini first. */
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_suspend())return;
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
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_discard();
#endif
  if (surface.frame)
    display->release(display->context, surface.frame);
  surface.frame = 0;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_finish();
#endif
#ifdef PORTABLE_PAPER_CROSSFADE
  paper_handoff_finish();
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
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
  if(status) {
    if(native_custody_runtime)portable_adapter_retain();
    return status;
  }
#endif
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
