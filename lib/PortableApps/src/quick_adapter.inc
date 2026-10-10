/* Shared modal: retains the actual foreground stack and renders over a copied
 * completed image. No display lease survives storage/service calls. */
#ifndef PORTABLE_ALARM_CLIENT
#error Quick controls require foreground service arbitration
#endif
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "quick_paper.inc"
#ifdef PORTABLE_WIFI_SETTINGS_APP
/* Copied explicit intent survives checked shutdown and policy/UI reloads. */
static uint32_t quick_deferred_actions;
static struct {
 uint8_t brightness,volume;
 bool torch,action_torch,dnd;
#ifdef PORTABLE_FRONTLIGHT_TONE
 uint8_t tone;
#endif
} quick_deferred_values;
static const char *quick_deferred_destination;
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) && defined(PORTABLE_LOW_BATTERY)
static bool low_battery_transition_pending;
static risc_battery_sample_v1 low_battery_pending_sample;
#endif
static size_t quick_row_bytes(void){return surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;}
static size_t quick_frame_bytes(void){return quick_row_bytes()*info.height;}
static bool quick_storage_safe(void) {
#ifdef PORTABLE_FILE_BROWSER_APP
 if(!portable_file_browser_safe())return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
 if(!portable_radio_services_safe())return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
 if(!portable_audio_services_safe())return false;
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
 if(!portable_wifi_services_safe())return false;
#endif
 return true;
}
#ifdef PORTABLE_X4_IDLE_POLICY
static bool automatic_idle_interaction_ready(void) {
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
 if(!portable_wifi_idle_ready())return false;
#endif
 return !idle_capture_active() && !surface.frame && !alarm_modal && !touch.down &&
     !quick_modal && !pqa_visible(&quick.ui) && !pqa_capture(&quick.ui) &&
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
     !quick_open_requested &&
#endif
     !quick_launch_pending && !handoff_requested && quick_storage_safe();
}
static bool automatic_idle_ready(void) {
 return display_settled&&!paper_token&&automatic_idle_interaction_ready();
}

#endif
#ifdef PORTABLE_LOW_BATTERY
static bool low_battery_poll_inner(void) {
 /* Battery policy is elapsed-time state. A submitted immutable image owns
  * no app raster lease and must not delay a threshold crossing. */
 if(surface.frame||!quick_storage_safe())return true;
 uint32_t now=millis_now();
 risc_battery_sample_v1 sample={0,255,RISC_BATTERY_PROFILE_MISSING};
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(low_battery_transition_pending)sample=low_battery_pending_sample;
 else
#endif
 {
  if(low_battery_sampled&&(uint32_t)(now-low_battery_sampled_at)<5000u)return true;
  low_battery_sampled=true;low_battery_sampled_at=now;
  if(!gauge||!gauge->read||!gauge->read(gauge->context,&sample))return true;
 }
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(!portable_low_battery_sample_valid(&sample))return true;
 if(portable_wifi_async_owned()) {
  bool low=sample.percent<PORTABLE_LOW_BATTERY_THRESHOLD;
  if(low_battery.observed&&low_battery.low==low)return true;
  /* Entering Low changes radio policy, so preserve the triggering sample
   * through checked shutdown before committing either the edge or preferences.
   * A high/rearm sample only needs the short lease below, never RF shutdown. */
  if(low) {
   low_battery_pending_sample=sample;low_battery_transition_pending=true;
   if(!portable_wifi_suspend())return portable_wifi_stop_pending();
  }
 }
 if(!portable_wifi_services_begin()) {
  low_battery_pending_sample=sample;low_battery_transition_pending=true;
  return !failed && !native_sleep_retained;
 }
#endif
 unsigned result=portable_low_battery_update(&low_battery,rt,&sample);
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(native_sleep_retained || !portable_wifi_services_end())return false;
 low_battery_transition_pending=false;
#endif
 if((result&PORTABLE_LOW_BATTERY_RETAINED)
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    || native_custody_retained
#endif
 )return false;
 if(result&PORTABLE_LOW_BATTERY_ERROR)rt->diagnostic("LOW_BATTERY settings=unconfirmed");
 if(!(result&PORTABLE_LOW_BATTERY_ENTERED))return true;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if(!portable_background_stop())return false;
#endif
 /* Cancel stale preview/pending actions before reloading confirmed settings.
  * A later new gesture is a manual override and remains authoritative. */
 pqa_cancel(&quick.ui);(void)pqa_take_action(&quick.ui);
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
 if(!portable_wifi_suspend())return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
 if(!portable_radio_suspend())return false;
#endif
 if(!pqa_session_load(&quick,rt)||!pqa_radios_load(&quick_radios,&quick.ui,rt) ||
    !pqa_session_restore(&quick,display))return false;
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(quick.ui.paper && !pqa_session_sync_brightness(&quick,display))return false;
#endif
 rt->diagnostic(result&PORTABLE_LOW_BATTERY_ERROR?"LOW_BATTERY crossing=partial-once":"LOW_BATTERY crossing=applied-once");return true;
}
static bool low_battery_poll(void) {
 bool ok=low_battery_poll_inner();
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(!ok){
#if defined(PORTABLE_FRONTLIGHT_TONE) && defined(PORTABLE_ALARM_TERMINAL_RETENTION)
  if(quick.retained)portable_adapter_retain_silent();else
#endif
  {portable_adapter_retain();}
 }
#endif
 return ok;
}
#endif
static bool quick_apply(uint32_t actions) {
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 if(failed || native_custody_retained)return false;
#endif
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(actions && (quick_deferred_actions || portable_wifi_async_owned())) {
  if(actions&(PQA_BRIGHTNESS_PREVIEW|PQA_BRIGHTNESS_COMMIT))quick_deferred_values.brightness=quick.ui.action_brightness;
  if(actions&(PQA_VOLUME_COMMIT|PQA_SILENT))quick_deferred_values.volume=quick.ui.action_volume;
  if(actions&PQA_TORCH){quick_deferred_values.torch=quick.ui.torch;quick_deferred_values.action_torch=quick.ui.action_torch;}
  if(actions&PQA_DND)quick_deferred_values.dnd=quick.ui.action_dnd;
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(actions&(PQA_TONE_PREVIEW|PQA_TONE_COMMIT))quick_deferred_values.tone=quick.ui.action_tone;
#endif
 }
 actions|=quick_deferred_actions;
 if(actions && portable_wifi_async_owned()) {
  quick_deferred_actions=actions;
  if(!portable_wifi_services_safe())return !failed;
  if(!portable_wifi_suspend())return portable_wifi_stop_pending();
 }
 if(quick_deferred_actions) {
  if(actions&(PQA_BRIGHTNESS_PREVIEW|PQA_BRIGHTNESS_COMMIT))quick.ui.action_brightness=quick_deferred_values.brightness;
  if(actions&(PQA_VOLUME_COMMIT|PQA_SILENT))quick.ui.action_volume=quick_deferred_values.volume;
  if(actions&PQA_TORCH){quick.ui.torch=quick_deferred_values.torch;quick.ui.action_torch=quick_deferred_values.action_torch;}
  if(actions&PQA_DND)quick.ui.action_dnd=quick_deferred_values.dnd;
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(actions&(PQA_TONE_PREVIEW|PQA_TONE_COMMIT))quick.ui.action_tone=quick_deferred_values.tone;
#endif
 }
 quick_deferred_actions=0;
#endif
 if(!actions)return true;
 portable_stage_log(rt,"quick-action","state=apply");
 if(!quick_storage_safe())return false;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if(!portable_background_stop())return false;
#endif
#ifdef PORTABLE_QUICK_RADIOS
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
 if(((actions&PQA_AIRPLANE)&&!quick.ui.airplane)||((actions&PQA_WIFI)&&quick.ui.wifi_enabled)) {
  if(!portable_wifi_suspend())return false;
 }
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(!pqa_radios_apply(&quick_radios,&quick.ui,rt,actions)){portable_adapter_retain();return false;}
#else
 if(!pqa_radios_apply(&quick_radios,&quick.ui,rt,actions))return false;
#endif
#endif
 bool changed=false;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(!pqa_session_apply(&quick,rt,display,actions,&changed)){
#if defined(PORTABLE_FRONTLIGHT_TONE) && defined(PORTABLE_ALARM_TERMINAL_RETENTION)
  if(quick.retained)portable_adapter_retain_silent();else
#endif
  {portable_adapter_retain();}
  return false;
 }
#else
 if(!pqa_session_apply(&quick,rt,display,actions,&changed))return false;
#endif
#if defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION)
 if(changed && !alarm_refresh())return false;
#else
 const alarm_service_v1 *service=alarm_sleep_api();
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(changed && service) {
  int32_t status=service->refresh(service->context);
  if(status!=ALARM_OK && status!=ALARM_PENDING){portable_adapter_retain();return false;}
 }
#else
 if(changed && service)(void)service->refresh(service->context);
#endif
#endif
 return true;
}
static bool quick_copy_background(void) {
 /* Every visible byte is overwritten below; clearing it first only repeats
  * a full-frame paint. Keep ordinary surface acquisition and custody checks. */
 if(!acquire_surface() || failed)return false;
 if(!quick_background)return false;
 size_t bytes=quick_row_bytes();
 for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,(uint8_t*)quick_background+y*bytes,bytes);
 return true;
}
static bool quick_interrupt(void) {
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 if(failed || native_custody_retained)return false;
#endif
 if(!quick_modal)return true;
 pqa_cancel(&quick.ui);quick.ui.pending&=~(uint32_t)PQA_WIFI;
 if(!quick_apply(pqa_take_action(&quick.ui)))return false;
 /* Alarm's saved restoration frame is the app, never the torch/controls. */
 if(quick_background && alarm_pixels){memcpy(alarm_pixels,quick_background,quick_frame_bytes());alarm_pixels_valid=true;}
 return !quick.ui.torch_valid || pqa_session_restore(&quick,display);
}
static void quick_time(char out[6]) {
 memcpy(out,"--:--",6);
#if defined(PORTABLE_DESK_CLOCK_SPARSE_START) || defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_NATIVE_TIME_TOOLBAR)
 uint8_t hour,minute;
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 twatch_rtc_time_v1 local;
 if(!native_toolbar_local(&local))return;
 hour=local.hour;minute=local.minute;
 if(true)
#elif defined(PORTABLE_SETTINGS_NATIVE_TIME)
 if(!failed && settings_native_clock(&hour,&minute))
#else
 if(!failed && portable_desk_clock_time(&hour,&minute))
#endif
 {
  unsigned h=quick.hour_24?hour:hour%12;if(!quick.hour_24&&!h)h=12;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  snprintf(out,6,"%u:%02u",h%24u,minute%60u);
#else
  out[0]=(char)('0'+h/10);out[1]=(char)('0'+h%10);out[3]=(char)('0'+minute/10);out[4]=(char)('0'+minute%10);
#endif
 }
#else
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!rt->acquire("rtc.clock",2,0,&g))return;
 const twatch_rtc_api_v1 *clock=g.api;twatch_rtc_time_v1 raw,local;
 if(clock && clock->api_version==2 && clock->struct_size>=sizeof(*clock) && clock->read &&
    clock->read(clock->context,&raw) && portable_time_forward(&raw,&local)) {
  unsigned h=quick.hour_24?local.hour:local.hour%12;if(!quick.hour_24&&!h)h=12;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  snprintf(out,6,"%u:%02u",h%24u,local.minute%60u);
#else
  out[0]=(char)('0'+h/10);out[1]=(char)('0'+h%10);out[3]=(char)('0'+local.minute/10);out[4]=(char)('0'+local.minute%10);
#endif
 }
 if(!rt->release(&g))failed=true;
#endif
}
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
/* Private Home tap intent. Ownership and storage work begin in the next poll,
 * after the ordinary pending frame has reached its completion barrier. */
bool portable_paper_quick_open(void) {
 if(!rt || failed || native_sleep_retained || !quick.ui.paper || quick_modal ||
    quick_launch_pending || handoff_requested || alarm_modal || pqa_visible(&quick.ui))return false;
 quick_open_requested=true;return true;
}
#endif
static bool quick_dispatch_destination(const char *quick_destination) {
#ifdef PORTABLE_WIFI_SETTINGS_APP
 quick_deferred_destination=NULL;
#endif
 bool wifi=quick_destination && !strcmp(quick_destination,"wifi_settings.elf");
 if(quick_destination && !failed && app_allows_launch(quick_destination)) {
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close())return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
  if(!portable_audio_suspend())return false;
#endif
#ifdef PORTABLE_UPDATE_APP
  if(!portable_update_close())return false;
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
  if(!portable_wifi_services_safe())return !failed;
  if(!portable_wifi_close()) {
   if(!portable_wifi_stop_pending())return false;
   quick_deferred_destination=quick_destination;return true;
  }
  quick_deferred_destination=NULL;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  if(!resident_queue_launch(quick_destination)){
#else
  if(!rt->request_launch(quick_destination)){
#endif
   quick.ui.error_flags|=wifi?PQA_ERROR_WIFI:PQA_ERROR_USB;rt->diagnostic("QUICK app launch-failed");}
  else {
#ifdef PORTABLE_UPDATE_APP
   extern void portable_update_handoff(void);portable_update_handoff();
#endif
#ifndef PORTABLE_RESIDENT_SHELL_HOST
   quick_launch_pending=handoff_requested=true;
#endif
  } /* caller exits via ordinary cleanup */
 }
 return !failed;
}
static bool quick_foreground(bool *consumed) {
 *consumed=false;
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 if(failed || native_custody_retained)return false;
#endif
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(quick_deferred_destination) {
  *consumed=true;return quick_dispatch_destination(quick_deferred_destination);
 }
 if(quick_deferred_actions) {
  if(!quick_apply(0))return false;
  if(quick_deferred_actions){*consumed=true;return true;}
 }
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(quick_open_requested) {
  quick_open_requested=false;
  if(!quick.ui.paper || quick_modal || alarm_modal)return false;
  pqa_cancel_input(&quick.ui);
  quick.ui.position_q8=0;quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.velocity_q8=0;
  quick.ui.clock_valid=false;quick.ui.animation_remainder_ms=0;
  last_activity=millis_now();
 }
#endif
 if(!pqa_visible(&quick.ui) && !pqa_capture(&quick.ui))return true;
 /* Reserve a possible top-edge gesture without owning a modal yet.
  * Keep ordinary tabs/gear taps replayable without pausing the foreground
  * radio, touching storage, copying pixels or acquiring controls resources. */
 if(quick.ui.gesture==PQA_TOP_PENDING && !pqa_visible(&quick.ui)) {
  *consumed=true;return true;
 }
 /* A gate after close needs only a neutral sample, never a blocking modal. */
 if(quick.ui.neutral_gate && !pqa_visible(&quick.ui))return true;
 if(!failed && paper_token){*consumed=true;return true;}
 if(!display_settled || surface.frame || !alarm_pixels_valid)return false;
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(portable_wifi_async_owned()) {
  /* The explicit gesture stays captured; no radio-policy/KV/HCI call occurs
   * before its asynchronous Wi-Fi shutdown has actually finished. */
  if(!portable_wifi_services_safe())return !failed;
  if(!portable_wifi_suspend()) {
   if(!portable_wifi_stop_pending())return false;
   *consumed=true;return true;
  }
 }
#endif
 if(!quick_storage_safe()){pqa_cancel(&quick.ui);return true;}
#ifdef PORTABLE_RADIO_SESSION
 /* Drain before radio controls load/apply; a dismiss never resumes RF. */
 if(!portable_radio_suspend())return false;
#endif
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if(!portable_background_stop())return false;
#endif
 if(!pqa_session_load(&quick,rt))return false;
 quick_paper_capabilities();
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(quick.ui.paper && quick.ui.torch_valid && !pqa_session_sync_brightness(&quick,display)) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#if defined(PORTABLE_FRONTLIGHT_TONE) && defined(PORTABLE_ALARM_TERMINAL_RETENTION)
  if(quick.retained)portable_adapter_retain_silent();else
#endif
  {portable_adapter_retain();}
#endif
  return false;
 }
#endif
#ifdef PORTABLE_QUICK_RADIOS
 if(!pqa_radios_load(&quick_radios,&quick.ui,rt))return false;
#endif
 quick_background=malloc(quick_frame_bytes());
 if(!quick_background){pqa_cancel(&quick.ui);rt->diagnostic("QUICK unavailable=memory");return true;}
 memcpy(quick_background,alarm_pixels,quick_frame_bytes());quick_modal=true;
 portable_stage_log(rt,"action","name=quick-controls-open");
 bool painted=false,wifi=false;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 bool clean_pending=false;
#endif
#ifdef PORTABLE_QUICK_USB_TRANSFER
 bool usb_transfer=false;
#endif
 uint32_t began=millis_now(),telemetry_at=began-1000u;
 char time[6]="--:--",painted_time[6]={0};risc_battery_sample_v1 battery={0};bool battery_valid=false,painted_battery_valid=false;
 pqa_state painted_state={0};uint8_t painted_battery=0;
 while(!failed && (pqa_visible(&quick.ui)||pqa_capture(&quick.ui))) {
  input_service();input_dispatch();
  if(failed)break;
  uint32_t now=millis_now();bool alarm_consumed=false;
  if(!alarm_foreground(&alarm_consumed)){failed=true;break;}
  if(alarm_consumed){*consumed=true;break;}
#ifdef PORTABLE_LOW_BATTERY
  if(!low_battery_poll()){failed=true;break;}
#endif
  if(navigation_pending&T5_APP_BUTTON_BACK){navigation_pending=0;pqa_close(&quick.ui);}
#ifdef PORTABLE_APP_SLEEP_LOCAL
  /* Retaining paper controls stay available for inspection until dismissed.
   * Keep the existing idle policy for the animated Watch sheet. */
  if(!quick.ui.paper && (uint32_t)(now-last_activity)>=portable_idle_ms())pqa_cancel(&quick.ui);
#endif
  uint32_t actions=pqa_take_action(&quick.ui);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  clean_pending|=quick.ui.clean_refresh_valid&&!!(actions&PQA_CLEAN_REFRESH);
  actions&=~PQA_CLEAN_REFRESH;
#endif
  if(!quick.ui.radio_controls && (actions&PQA_WIFI)){wifi=true;pqa_cancel(&quick.ui);actions|=pqa_take_action(&quick.ui);}
#ifdef PORTABLE_QUICK_USB_TRANSFER
  if(actions&PQA_USB_TRANSFER){usb_transfer=true;pqa_cancel(&quick.ui);actions|=pqa_take_action(&quick.ui);}
#endif
  if(!quick_apply(actions)){failed=true;break;}
  if(!paper_present_progress())break;
  pqa_animate(&quick.ui,now);
  if(pqa_visible(&quick.ui) || (!quick.ui.paper && painted)) {
   if((uint32_t)(now-telemetry_at)>=1000u) {
    quick_time(time);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(native_sleep_retained)return false;
#endif
    battery_valid=gauge && gauge->read && gauge->read(gauge->context,&battery) && battery.percent<=100 && !(battery.flags&RISC_BATTERY_PROFILE_MISSING);telemetry_at=now;
   }
   bool dirty=!quick.ui.paper || !painted || quick_paper_changed(&painted_state,&quick.ui) ||
#ifdef PORTABLE_RESIDENT_SHELL_HOST
       clean_pending ||
#endif
       strcmp(time,painted_time) || battery_valid!=painted_battery_valid || (battery_valid&&battery.percent!=painted_battery);
   if(dirty && portable_paper_frame_ready()) {
    if(quick.ui.paper)quick_paper_render(time,battery_valid,battery.percent);
    else if(!quick_copy_background() || !pqa_render(&surface,&quick.ui,time,battery_valid,(uint8_t)battery.percent)){failed=true;break;}
    /* Present can sample input while the panel is busy. Record only the state
     * rendered above; newer input must remain dirty for the next frame. */
    painted_state=quick.ui;
    memcpy(painted_time,time,sizeof(time));painted_battery_valid=battery_valid;painted_battery=battery.percent;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
    quick_clean_present=clean_pending;
    present(clean_pending||!quick.ui.paper);
    if(native_custody_retained)return false;
    quick_clean_present=false;clean_pending=false;
#else
    present(!quick.ui.paper);
#endif
    painted=true;*consumed=true;
   }
  }
  if(!pqa_visible(&quick.ui) && (quick.ui.neutral_gate || quick.ui.gesture==PQA_IDLE || quick.ui.gesture==PQA_PASS_THROUGH))break;
  input_pending=false;
  rt->yield_ms(input_progressed?1u:4u);
 }
 /* A release can make the overlay invisible immediately (torch-off). Its
  * final action still owns a hardware restoration even after the loop ends. */
 if(!failed && !quick_apply(pqa_take_action(&quick.ui)))failed=true;
 if(painted && !failed) {
  if(!portable_paper_frame_drain() || !quick_copy_background())failed=true;
  else {present(!quick.ui.paper);if(!portable_paper_frame_drain())failed=true;}
 }
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(native_sleep_retained)return false; /* Keep the Runtime-owned allocation. */
#endif
 quick_modal=false;free(quick_background);quick_background=NULL;
 portable_stage_log(rt,"action","name=quick-controls-close");
 if(*consumed){input_pending=false;input_sample=(portable_touch_sample){0};input_reset();}
 const char *quick_destination=wifi?"wifi_settings.elf":NULL;
#ifdef PORTABLE_QUICK_USB_TRANSFER
 if(usb_transfer)quick_destination="usb_sd_transfer.elf";
#endif
 if(!quick_dispatch_destination(quick_destination))return false;
 return !failed;
}
