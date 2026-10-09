/* Shared modal: retains the actual foreground stack and renders over a copied
 * completed image. No display lease survives storage/service calls. */
#ifndef PORTABLE_ALARM_CLIENT
#error Quick controls require foreground service arbitration
#endif
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "quick_paper.inc"
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
static bool quick_apply(uint32_t actions) {
 if(!actions)return true;
 if(!quick_storage_safe())return false;
#ifdef PORTABLE_QUICK_RADIOS
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
 if(((actions&PQA_AIRPLANE)&&!quick.ui.airplane)||((actions&PQA_WIFI)&&quick.ui.wifi_enabled)) {
  if(!portable_wifi_suspend())return false;
 }
#endif
 if(!pqa_radios_apply(&quick_radios,&quick.ui,rt,actions))return false;
#endif
 bool changed=false;
 if(!pqa_session_apply(&quick,rt,display,actions,&changed))return false;
 const alarm_service_v1 *service=alarm_sleep_api();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(changed && service) {
  int32_t status=service->refresh(service->context);
  if(status!=ALARM_OK && status!=ALARM_PENDING){portable_desk_adapter_retain();return false;}
 }
#else
 if(changed && service)(void)service->refresh(service->context);
#endif
 return true;
}
static bool quick_copy_background(void) {
 clear();if(failed)return false;
 if(!quick_background)return false;
 size_t bytes=quick_row_bytes();
 for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,(uint8_t*)quick_background+y*bytes,bytes);
 return true;
}
static bool quick_interrupt(void) {
 if(!quick_modal)return true;
 pqa_cancel(&quick.ui);quick.ui.pending&=~(uint32_t)PQA_WIFI;
 if(!quick_apply(pqa_take_action(&quick.ui)))return false;
 /* Alarm's saved restoration frame is the app, never the torch/controls. */
 if(quick_background && alarm_pixels){memcpy(alarm_pixels,quick_background,quick_frame_bytes());alarm_pixels_valid=true;}
 return !quick.ui.torch_valid || pqa_session_restore(&quick,display);
}
static void quick_time(char out[6]) {
 memcpy(out,"--:--",6);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 uint8_t hour,minute;
 if(!failed && portable_desk_clock_time(&hour,&minute)) {
  unsigned h=quick.hour_24?hour:hour%12;if(!quick.hour_24&&!h)h=12;
  out[0]=(char)('0'+h/10);out[1]=(char)('0'+h%10);out[3]=(char)('0'+minute/10);out[4]=(char)('0'+minute%10);
 }
#else
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!rt->acquire("rtc.clock",2,0,&g))return;
 const twatch_rtc_api_v1 *clock=g.api;twatch_rtc_time_v1 raw,local;
 if(clock && clock->api_version==2 && clock->struct_size>=sizeof(*clock) && clock->read &&
    clock->read(clock->context,&raw) && portable_time_forward(&raw,&local)) {
  unsigned h=quick.hour_24?local.hour:local.hour%12;if(!quick.hour_24&&!h)h=12;
  out[0]=(char)('0'+h/10);out[1]=(char)('0'+h%10);out[3]=(char)('0'+local.minute/10);out[4]=(char)('0'+local.minute%10);
 }
 if(!rt->release(&g))failed=true;
#endif
}
static bool quick_foreground(bool *consumed) {
 *consumed=false;
 if(!pqa_visible(&quick.ui) && !pqa_capture(&quick.ui))return true;
 /* Reserve a possible top-edge gesture without owning a modal yet.
  * Keep ordinary tabs/gear taps replayable without pausing the foreground
  * radio, touching storage, copying pixels or acquiring controls resources. */
 if(quick.ui.gesture==PQA_TOP_PENDING && !pqa_visible(&quick.ui)) {
  *consumed=true;return true;
 }
 /* A gate after close needs only a neutral sample, never a blocking modal. */
 if(quick.ui.neutral_gate && !pqa_visible(&quick.ui))return true;
 if(!display_settled || surface.frame || !alarm_pixels_valid)return false;
 if(!quick_storage_safe()){pqa_cancel(&quick.ui);return true;}
#ifdef PORTABLE_RADIO_SESSION
 /* Drain before radio controls load/apply; a dismiss never resumes RF. */
 if(!portable_radio_suspend())return false;
#endif
 if(!pqa_session_load(&quick,rt))return false;
 quick_paper_capabilities();
#ifdef PORTABLE_QUICK_RADIOS
 if(!pqa_radios_load(&quick_radios,&quick.ui,rt))return false;
#endif
 quick_background=malloc(quick_frame_bytes());
 if(!quick_background){pqa_cancel(&quick.ui);rt->diagnostic("QUICK unavailable=memory");return true;}
 memcpy(quick_background,alarm_pixels,quick_frame_bytes());quick_modal=true;
 bool painted=false,wifi=false;uint32_t began=millis_now(),telemetry_at=began-1000u;
 char time[6]="--:--",painted_time[6]={0};risc_battery_sample_v1 battery={0};bool battery_valid=false,painted_battery_valid=false;
 pqa_state painted_state={0};uint8_t painted_battery=0;
 while(!failed && (pqa_visible(&quick.ui)||pqa_capture(&quick.ui))) {
  uint32_t now=millis_now();bool alarm_consumed=false;
  if(!alarm_foreground(&alarm_consumed)){failed=true;break;}
  if(alarm_consumed){*consumed=true;break;}
  if(navigation_pending&T5_APP_BUTTON_BACK){navigation_pending=0;pqa_close(&quick.ui);}
#ifdef PORTABLE_APP_SLEEP_LOCAL
  if((uint32_t)(now-last_activity)>=60000u)pqa_cancel(&quick.ui);
#endif
  uint32_t actions=pqa_take_action(&quick.ui);
  if(!quick.ui.radio_controls && (actions&PQA_WIFI)){wifi=true;pqa_cancel(&quick.ui);actions|=pqa_take_action(&quick.ui);}
  if(!quick_apply(actions)){failed=true;break;}
  pqa_animate(&quick.ui,now);
  if(pqa_visible(&quick.ui) || (!quick.ui.paper && painted)) {
   if((uint32_t)(now-telemetry_at)>=1000u) {
    quick_time(time);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
    if(native_sleep_retained)return false;
#endif
    battery_valid=gauge && gauge->read && gauge->read(gauge->context,&battery) && battery.percent<=100 && !(battery.flags&RISC_BATTERY_PROFILE_MISSING);telemetry_at=now;
   }
   bool dirty=!quick.ui.paper || !painted || quick_paper_changed(&painted_state,&quick.ui) ||
       strcmp(time,painted_time) || battery_valid!=painted_battery_valid || (battery_valid&&battery.percent!=painted_battery);
   if(dirty) {
    if(quick.ui.paper)quick_paper_render(time,battery_valid,battery.percent);
    else if(!quick_copy_background() || !pqa_render(&surface,&quick.ui,time,battery_valid,(uint8_t)battery.percent)){failed=true;break;}
    present(!quick.ui.paper);painted=true;*consumed=true;painted_state=quick.ui;
    memcpy(painted_time,time,sizeof(time));painted_battery_valid=battery_valid;painted_battery=battery.percent;
   }
  }
  if(!pqa_visible(&quick.ui) && (quick.ui.neutral_gate || quick.ui.gesture==PQA_IDLE || quick.ui.gesture==PQA_PASS_THROUGH))break;
  rt->yield_ms(8);input_service();
 }
 /* A release can make the overlay invisible immediately (torch-off). Its
  * final action still owns a hardware restoration even after the loop ends. */
 if(!failed && !quick_apply(pqa_take_action(&quick.ui)))failed=true;
 if(painted && !failed) {
  if(!quick_copy_background())failed=true;else present(!quick.ui.paper);
 }
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(native_sleep_retained)return false; /* Keep the Runtime-owned allocation. */
#endif
 quick_modal=false;free(quick_background);quick_background=NULL;
 if(*consumed){input_pending=false;input_sample=(portable_touch_sample){0};touch.neutral=touch.down=false;}
 if(wifi && !failed && app_allows_launch("wifi_settings.elf")) {
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close())return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
  if(!portable_audio_suspend())return false;
#endif
#ifdef PORTABLE_UPDATE_APP
  if(!portable_update_close())return false;
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
  if(!portable_wifi_close())return false;
#endif
  if(!rt->request_launch("wifi_settings.elf")){quick.ui.error_flags|=PQA_ERROR_WIFI;rt->diagnostic("QUICK Wi-Fi launch-failed");}
  else {
#ifdef PORTABLE_UPDATE_APP
   extern void portable_update_handoff(void);portable_update_handoff();
#endif
   quick_launch_pending=handoff_requested=true;
  } /* caller exits via ordinary cleanup */
 }
 return !failed;
}
