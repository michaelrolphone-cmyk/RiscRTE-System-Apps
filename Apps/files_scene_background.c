/* Shared Files background integration. Canonical controller/header bytes are
 * supplied from the verified Utilities dependency, never the raster adapter. */
#include "files_scene_background.h"
#include "files_background.h"
#include "PortableSleepPolicy.h"
#include "PortableTimeZonePreference.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscDisplayOutputV1.h"
#include "AlarmServiceV2.h"
extern bool files_scene_background_message(const char text[33]);
extern bool files_scene_background_launch(const char text[33]);
extern bool files_scene_background_workflow_ready(void);
extern void files_scene_background_idle_update(uint32_t);
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
static bool observe(void*c,files_background_observation*out){(void)c;*out=observation;return true;}
static bool message(void*c,const char text[33]){(void)c;return files_scene_background_message(text);}
static bool launch(void*c,const char text[33]){(void)c;return files_scene_background_launch(text);}
bool files_scene_background_init(const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*c){
 /* Reinitialization is legal only after all previous custody has closed. */
 if(files_bg.initialized||files_bg.retained)return fbg_fail();
 observed=false;observation=(files_background_observation){0};
 files_background_hooks hooks={.alive=alive,.retain=retain,.context=c,.observe=observe,.message=message,.launch=launch,
 .face_save=face_save,.action_mask=PORTABLE_CONTEXT_ACTIONS};
 return files_background_init(rt,&hooks);
}
bool files_scene_background_capture(void){return files_background_capture();}
bool files_scene_background_pause(void){return files_background_pause();}
bool files_scene_background_close(void){return files_background_close();}
bool files_scene_background_capture_active(void){return files_background_capture_active();}
uint32_t files_scene_background_effects(void){return files_background_status_copy().pending_effects;}
bool files_scene_background_effect_done(unsigned e,unsigned result){return files_background_effect_done(e,result);}
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
  files_scene_background_idle_update(idle);}
 if(modes)mask|=PORTABLE_CONTEXT_ALERT;
 files_bg.hooks.action_mask=mask;files_bg.hooks.alert_modes=(uint8_t)modes;return true;
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
 uint64_t utc=0;if(!files_background_clock(files_bg.rt,&utc))return false;
 if(utc){
  /* Exact retained Watch/Wave recipes select UTC+8-to-America/Denver and
   * do not read time_zone for rule observations. The provider UTC suffix
   * removes the RTC basis; the same pure civil rule preserves local hour. */
  portable_timezone_rule rule;portable_timezone_civil local;
  if(portable_timezone_resolve("America/Denver",sizeof("America/Denver"),&rule)==PORTABLE_TIMEZONE_OK&&portable_timezone_utc_to_local(&rule,(int64_t)utc,&local,NULL)==PORTABLE_TIMEZONE_OK){observation.local_time_valid=true;observation.local_hour=local.hour;}
 }
 observed=true;observed_at=now;return true;
}
bool files_scene_background_step(bool settled){
 if(settled&&!sample_observation())return false;
 files_background_boundary b={.settled=settled,.audio_allowed=true,.radio_allowed=true,.broadcast_allowed=true,.workflow_ready=files_scene_background_workflow_ready()};
 return files_background_step(&b);
}
bool files_scene_background_idle_timeout(uint32_t*out){
 if(!out||!files_background_before_storage(false))return false;
 risc_runtime_capability_v1 g={0};int ac=fbg_acquire(RISC_KEY_VALUE_CAPABILITY,1,&g);if(ac)return false;
 const risc_key_value_v1*p=g.api;if(!pqa_preferences_valid(p))return fbg_fail();
 risc_key_value_v1 view={1,sizeof(view),(void*)p,fbg_kv_get,fbg_deny_put};uint32_t ms=0;
 int rc=portable_sleep_timer_load(&view,false,&ms);
 if(!fbg_check()||!fbg_release(&g))return false;
 if(rc!=PORTABLE_SLEEP_LOADED&&rc!=PORTABLE_SLEEP_MISSING)return false;
 *out=ms;return true;
}
bool portable_background_stop(void){return files_scene_background_pause();}
bool portable_broadcast_stop(void){return files_scene_background_pause();}
bool portable_contexts_stop(void){return files_scene_background_pause();}
