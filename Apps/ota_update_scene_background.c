/* Shared Firmware Update background integration. Canonical controller/header bytes are
 * supplied from the verified Utilities dependency, never the raster adapter. */
#include "ota_update_scene_background.h"
#include "files_background.h"
#include "PortableSleepPolicy.h"
#include "RiscCivilClockV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscDisplayOutputV1.h"
#include "AlarmServiceV2.h"
extern bool ota_update_scene_background_message(const char text[33]);
extern bool ota_update_scene_background_launch(const char text[33]);
extern bool ota_update_scene_background_workflow_ready(void);
extern void ota_update_scene_background_idle_update(uint32_t);
/* safe is pure: no Wi-Fi/OTA custody, uncertain bank, pending cleanup or restart.
 * The root must complete pause BEFORE entering any such state. Unsafe gates
 * inspect copied state only; even a repeated generic pause can touch storage. */
extern bool ota_update_scene_background_safe(void);
static bool permitted(void){return fbg_check()&&ota_update_scene_background_safe();}
static bool paused_only(void){return fbg_check()&&files_background_status_copy().paused;}
static files_background_observation observation;
static uint32_t observed_at;static bool observed;
/* Both retained Context deployments use the same stable 34-face preference
 * namespace. This is saved-data policy, not a device or physical dispatch. */
static bool face_save(const risc_key_value_v1*kv,unsigned id){
 if(id>=34||!pqa_preferences_valid(kv))return false;
 const uint8_t wanted[]={0x46,1,(uint8_t)id,(uint8_t)(id^0xa5u)};
 int32_t rc=kv->put(kv->context,"watch_face",wanted,sizeof(wanted));
 if(!fbg_check())return false;
 uint8_t actual[4];uint32_t n=0;
 return (rc==RISC_KEY_VALUE_OK||rc==RISC_KEY_VALUE_IO)&&kv->get(kv->context,"watch_face",actual,sizeof(actual),&n)==RISC_KEY_VALUE_OK&&fbg_check()&&n==sizeof(actual)&&!memcmp(actual,wanted,n);
}
static bool firmware_observe(void*c,files_background_observation*out){(void)c;*out=observation;return true;}
static bool firmware_message(void*c,const char text[33]){(void)c;return ota_update_scene_background_message(text);}
static bool launch(void*c,const char text[33]){(void)c;return ota_update_scene_background_launch(text);}
bool ota_update_scene_background_init(const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*c){
 /* Reinitialization is legal only after all previous custody has closed. */
 if(files_bg.initialized||files_bg.retained)return fbg_fail();
 if(!ota_update_scene_background_safe())return false;
 observed=false;observation=(files_background_observation){0};
 files_background_hooks hooks={.alive=alive,.retain=retain,.context=c,.observe=firmware_observe,.message=firmware_message,.launch=launch,
 .face_save=face_save,.action_mask=PORTABLE_CONTEXT_ACTIONS};
 return files_background_init(rt,&hooks);
}
bool ota_update_scene_background_capture(void){return permitted()?files_background_capture():paused_only();}
bool ota_update_scene_background_pause(void){return permitted()?files_background_pause():paused_only();}
bool ota_update_scene_background_close(void){return permitted()&&files_background_close();}
bool ota_update_scene_background_capture_active(void){return files_background_capture_active();}
uint32_t ota_update_scene_background_effects(void){return files_background_status_copy().pending_effects;}
bool ota_update_scene_background_effect_done(unsigned e,unsigned result){return files_background_effect_done(e,result);}
static bool sample_applicability(void){
 uint16_t mask=PORTABLE_CONTEXT_IDLE|PORTABLE_CONTEXT_SLEEP|PORTABLE_CONTEXT_DEEP|PORTABLE_CONTEXT_FACE;
 unsigned modes=0;risc_runtime_capability_v1 g={0};
 int ac=fbg_acquire_version(ALARM_SERVICE_CAPABILITY,2,0,&g);if(ac<0)return false;
 if(!ac){const alarm_service_descriptor_v2*d=alarm_service_descriptor(g.api);if(!d)return fbg_fail();modes=d->output_modes;if(!fbg_release(&g))return false;}
 else {ac=fbg_acquire(ALARM_SERVICE_CAPABILITY,0,&g);if(ac<0)return false;
  if(!ac){const alarm_service_v1*p=g.api;if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->status||!p->refresh)return fbg_fail();
   /* Published v1 compatibility profile is both-output; no suffix is read. */
   modes=ALARM_MODE_BOTH;if(!fbg_release(&g))return false;}}
 bool brightness=false;ac=fbg_acquire("display.output",0,&g);if(ac<0)return false;
 if(!ac){const risc_display_output_api_v1*p=g.api;
  if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->get_info)return fbg_fail();
  risc_display_info_v1 info={.struct_size=sizeof(info)},before=info;bool ok=p->get_info(p->context,&info);if(!fbg_check())return false;
  if((!ok&&memcmp(&info,&before,sizeof(info)))||(ok&&info.struct_size!=sizeof(info)))return fbg_fail();
  brightness=ok&&(info.flags&RISC_DISPLAY_INFO_BRIGHTNESS)&&p->set_brightness;
  if(!fbg_release(&g))return false;}
 ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&g);if(ac<0)return false;
 if(!ac){const risc_key_value_v1*p=g.api;if(!pqa_preferences_valid(p))return fbg_fail();
  risc_key_value_v1 view={1,sizeof(view),(void*)p,fbg_kv_get,fbg_deny_put};bool dnd=false;unsigned level=0,restore=0,volume=0;
  if(pqa_dnd_load(&view,&dnd))mask|=PORTABLE_CONTEXT_DND;
  if(!fbg_check())return false;
  bool valid=pqa_preference_load(&view,PQA_BRIGHTNESS_KEY,PQA_BRIGHTNESS_DEFAULT,0,&level);
  if(!fbg_check())return false;
  if(level&&level<10)valid=false;
  if(!level)valid&=pqa_preference_load(&view,PQA_RESTORE_BRIGHTNESS_KEY,PQA_BRIGHTNESS_DEFAULT,10,&restore);
  if(!fbg_check())return false;
  if(valid&&brightness)mask|=PORTABLE_CONTEXT_BRIGHTNESS;
  valid=pqa_preference_load(&view,PQA_VOLUME_KEY,PQA_VOLUME_DEFAULT,0,&volume);if(!fbg_check())return false;
  if(valid&&(modes&ALARM_MODE_SOUND))mask|=PORTABLE_CONTEXT_VOLUME;
  uint32_t idle=0;int idle_result=portable_sleep_timer_load(&view,false,&idle);
  if(!fbg_check())return false;
  if(idle_result!=PORTABLE_SLEEP_LOADED&&idle_result!=PORTABLE_SLEEP_MISSING)idle=0;
  if(!fbg_release(&g))return false;
  ota_update_scene_background_idle_update(idle);}
 if(modes)mask|=PORTABLE_CONTEXT_ALERT;
 files_bg.hooks.action_mask=mask;files_bg.hooks.alert_modes=(uint8_t)modes;return true;
}
/* Local time comes only from the installed provider's current policy. Clean
 * absence/invalid time disables hour-based rules; no app zone or RTC basis. */
static bool sample_local_time(void){
 risc_runtime_capability_v1 g={0};int ac=fbg_acquire(RISC_CIVIL_CLOCK_CAPABILITY,0,&g);
 if(ac<0)return false;
 if(ac)return true;
 const risc_civil_clock_api_v1*p=g.api;
 if(p->api_version!=1||p->struct_size<RISC_CIVIL_CLOCK_BASE_SIZE||!p->read)return fbg_fail();
 risc_civil_time_v1 q={.struct_size=sizeof(q)},before=q;
 int rc=p->read(p->context,&q);if(!fbg_check())return false;
 if((rc!=0&&rc!=-1)||(rc&&memcmp(&q,&before,sizeof(q)))||
    (!rc&&(q.struct_size!=sizeof(q)||q.minute>=1440||!memchr(q.zone,0,sizeof(q.zone)))))return fbg_fail();
 if(!rc){observation.local_time_valid=true;observation.local_hour=(uint8_t)(q.minute/60);}
 return fbg_release(&g);
}
static bool sample_observation(void){
 if(!files_bg.contexts.api)return true;
 uint32_t now=0;if(!files_bg.rt->monotonic_ms(&now)||!fbg_check())return fbg_fail();
 if(observed&&(uint32_t)(now-observed_at)<500u)return true;
 if(!files_background_before_storage(false)||!sample_applicability())return false;
 observation=(files_background_observation){0};
 risc_runtime_capability_v1 g={0};int ac=fbg_acquire(RISC_BATTERY_GAUGE_CAPABILITY,0,&g);
 if(ac<0)return false;
 if(!ac){
  const risc_battery_gauge_api_v1*p=g.api;
  if(p->api_version!=1||p->struct_size<sizeof(*p)||!p->read)return fbg_fail();
  risc_battery_sample_v1 q={0},before=q;bool ok=p->read(p->context,&q);
  if(!fbg_check())return false;
  if((!ok&&memcmp(&q,&before,sizeof(q)))||(ok&&(q.percent>100||(q.flags&~3u))))return fbg_fail();
  if(ok&&!(q.flags&RISC_BATTERY_PROFILE_MISSING)){observation.battery_valid=true;observation.battery_percent=q.percent;observation.charging=!!(q.flags&RISC_BATTERY_CHARGING);}
  if(!fbg_release(&g))return false;
 }
 if(!sample_local_time())return false;
 observed=true;observed_at=now;return true;
}
bool ota_update_scene_background_step(bool settled){
 if(!permitted())return paused_only();
 if(settled&&!sample_observation())return false;
 files_background_boundary b={.settled=settled,.audio_allowed=true,.radio_allowed=true,.broadcast_allowed=true,.workflow_ready=ota_update_scene_background_workflow_ready()};
 return files_background_step(&b);
}
bool ota_update_scene_background_idle_timeout(uint32_t*out){
 if(!out||!permitted()||!files_background_before_storage(false))return false;
 risc_runtime_capability_v1 g={0};int ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&g);if(ac)return false;
 const risc_key_value_v1*p=g.api;if(!pqa_preferences_valid(p))return fbg_fail();
 risc_key_value_v1 view={1,sizeof(view),(void*)p,fbg_kv_get,fbg_deny_put};uint32_t ms=0;
 int rc=portable_sleep_timer_load(&view,false,&ms);
 if(!fbg_check()||!fbg_release(&g))return false;
 if(rc!=PORTABLE_SLEEP_LOADED&&rc!=PORTABLE_SLEEP_MISSING)return false;
 *out=ms;return true;
}
bool portable_background_stop(void){return ota_update_scene_background_pause();}
bool portable_broadcast_stop(void){return ota_update_scene_background_pause();}
bool portable_contexts_stop(void){return ota_update_scene_background_pause();}
