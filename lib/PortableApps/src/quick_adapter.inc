/* Shared modal: retains the actual foreground stack and renders over a copied
 * completed image. No display lease survives storage/service calls. */
#ifndef PORTABLE_ALARM_CLIENT
#error Quick controls require foreground service arbitration
#endif
#include "PortableRtcClock.h"
#include "PortableTime.h"
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
#ifdef PORTABLE_LOW_BATTERY
static bool low_battery_poll(void) {
 if(!display_settled||surface.frame||!quick_storage_safe())return true;
 uint32_t now=millis_now();
 if(low_battery_sampled&&(uint32_t)(now-low_battery_sampled_at)<5000u)return true;
 low_battery_sampled=true;low_battery_sampled_at=now;
 risc_battery_sample_v1 sample={0,255,RISC_BATTERY_PROFILE_MISSING};
 if(!gauge||!gauge->read||!gauge->read(gauge->context,&sample))return true;
 unsigned result=portable_low_battery_update(&low_battery,rt,&sample);
 if(result&PORTABLE_LOW_BATTERY_RETAINED)return false;
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
 if(!pqa_session_load(&quick,rt)||!pqa_radios_load(&quick_radios,&quick.ui,rt)||
    !pqa_session_restore(&quick,display))return false;
 rt->diagnostic(result&PORTABLE_LOW_BATTERY_ERROR?"LOW_BATTERY crossing=partial-once":"LOW_BATTERY crossing=applied-once");return true;
}
#endif
static bool quick_apply(uint32_t actions) {
 if(!actions)return true;
 if(!quick_storage_safe())return false;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if((actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE)) && !portable_background_stop())return false;
#endif
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
 if(changed && service)(void)service->refresh(service->context);
 return true;
}
static bool quick_copy_background(void) {
 clear();if(failed)return false;
 if(!quick_background)return false;
 for(unsigned y=0;y<240;y++)memcpy((uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,quick_background+y*240,480);
 return true;
}
static bool quick_interrupt(void) {
 if(!quick_modal)return true;
 pqa_cancel(&quick.ui);quick.ui.pending&=~(uint32_t)PQA_WIFI;
 if(!quick_apply(pqa_take_action(&quick.ui)))return false;
 /* Alarm's saved restoration frame is the app, never the torch/controls. */
 if(quick_background && alarm_pixels){memcpy(alarm_pixels,quick_background,240u*240u*2u);alarm_pixels_valid=true;}
 return pqa_session_restore(&quick,display);
}
static void quick_time(char out[6]) {
 memcpy(out,"--:--",6);
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 if(!rt->acquire("rtc.clock",2,0,&g))return;
 const twatch_rtc_api_v1 *clock=g.api;twatch_rtc_time_v1 raw,local;
 if(clock && clock->api_version==2 && clock->struct_size>=sizeof(*clock) && clock->read &&
    clock->read(clock->context,&raw) && portable_time_forward(&raw,&local)) {
  unsigned h=quick.hour_24?local.hour:local.hour%12;if(!quick.hour_24&&!h)h=12;
  out[0]=(char)('0'+h/10);out[1]=(char)('0'+h%10);out[3]=(char)('0'+local.minute/10);out[4]=(char)('0'+local.minute%10);
 }
 if(!rt->release(&g))failed=true;
}
static bool quick_foreground(bool *consumed) {
 *consumed=false;
 if(!pqa_visible(&quick.ui) && !pqa_capture(&quick.ui))return true;
 /* A possible top-edge gesture reserves input, not foreground resources.
  * Ordinary tab/gear taps must replay without suspending RF or loading
  * controls storage before a pull gesture actually commits. */
 if(quick.ui.gesture==PQA_TOP_PENDING && !pqa_visible(&quick.ui)) {
  *consumed=true;return true;
 }
 /* A gate after close needs only a neutral sample, never a blocking modal. */
 if(quick.ui.neutral_gate && !pqa_visible(&quick.ui))return true;
#ifdef PORTABLE_TAP_SETTINGS
 /* Controls can request an app launch before returning to the Settings poll.
  * Restore its sensor enrollment first, while this app still owns authority. */
 if(settings_motion_active){
  settings_motion_interrupted=true;
  if(!settings_tap_close()){*consumed=true;return true;}
 }
#endif
 if(!display_settled || surface.frame || !alarm_pixels_valid)return false;
 if(!quick_storage_safe()){pqa_cancel(&quick.ui);return true;}
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if(!portable_background_stop())return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
 /* Drain before radio controls load/apply; a dismiss never resumes RF. */
 if(!portable_radio_suspend())return false;
#endif
 if(!pqa_session_load(&quick,rt))return false;
#ifdef PORTABLE_QUICK_RADIOS
 if(!pqa_radios_load(&quick_radios,&quick.ui,rt))return false;
#endif
 quick_background=malloc(240u*240u*2u);
 if(!quick_background){pqa_cancel(&quick.ui);rt->diagnostic("QUICK unavailable=memory");return true;}
 memcpy(quick_background,alarm_pixels,240u*240u*2u);quick_modal=true;
 bool painted=false,wifi=false;uint32_t began=millis_now(),telemetry_at=began-1000u;
 char time[6]="--:--";risc_battery_sample_v1 battery={0};bool battery_valid=false;
 while(!failed && (pqa_visible(&quick.ui)||pqa_capture(&quick.ui))) {
  uint32_t now=millis_now();bool alarm_consumed=false;
  if(!alarm_foreground(&alarm_consumed)){failed=true;break;}
  if(alarm_consumed){*consumed=true;break;}
#ifdef PORTABLE_LOW_BATTERY
  if(!low_battery_poll()){failed=true;break;}
#endif
  if(navigation_pending&T5_APP_BUTTON_BACK){navigation_pending=0;pqa_close(&quick.ui);}
#ifdef PORTABLE_APP_SLEEP_LOCAL
  if((uint32_t)(now-last_activity)>=portable_idle_ms())pqa_cancel(&quick.ui);
#endif
  uint32_t actions=pqa_take_action(&quick.ui);
  if(!quick.ui.radio_controls && (actions&PQA_WIFI)){wifi=true;pqa_cancel(&quick.ui);actions|=pqa_take_action(&quick.ui);}
  if(!quick_apply(actions)){failed=true;break;}
  pqa_animate(&quick.ui,now);
  if(pqa_visible(&quick.ui) || painted) {
   if((uint32_t)(now-telemetry_at)>=1000u) {
    quick_time(time);battery_valid=gauge && gauge->read && gauge->read(gauge->context,&battery) && battery.percent<=100 && !(battery.flags&RISC_BATTERY_PROFILE_MISSING);telemetry_at=now;
   }
   if(!quick_copy_background() || !pqa_render(&surface,&quick.ui,time,battery_valid,(uint8_t)battery.percent)){failed=true;break;}
   present(true);painted=true;*consumed=true;
  }
  if(!pqa_visible(&quick.ui) && (quick.ui.neutral_gate || quick.ui.gesture==PQA_IDLE || quick.ui.gesture==PQA_PASS_THROUGH))break;
  rt->yield_ms(8);input_service();
 }
 /* A release can make the overlay invisible immediately (torch-off). Its
  * final action still owns a hardware restoration even after the loop ends. */
 if(!failed && !quick_apply(pqa_take_action(&quick.ui)))failed=true;
 if(painted && !failed) {
  if(!quick_copy_background())failed=true;else present(true);
 }
 quick_modal=false;free(quick_background);quick_background=NULL;
 if(*consumed){input_pending=false;input_sample=(portable_touch_sample){0};touch.neutral=touch.down=false;}
 if(wifi && !failed && app_allows_launch("wifi_settings.elf")) {
#ifdef PORTABLE_FILE_BROWSER_APP
  if(!portable_file_browser_close())return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
  if(!portable_audio_suspend())return false;
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  if(!portable_wifi_suspend())return false;
#endif
  if(!rt->request_launch("wifi_settings.elf")){quick.ui.error_flags|=PQA_ERROR_WIFI;rt->diagnostic("QUICK Wi-Fi launch-failed");}
  else quick_launch_pending=true; /* caller exits via ordinary cleanup */
 }
 return !failed;
}
