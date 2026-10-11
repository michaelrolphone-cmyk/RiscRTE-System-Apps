#pragma once
#include "alarm_writer.h"
/* Apply only settings composed by the rule engine; consume before writes and
 * report partial failure. Manual changes are not overwritten on every poll. */
static inline bool portable_context_setting(const risc_key_value_v1*kv,unsigned field,int32_t value,bool low,bool(*face)(const risc_key_value_v1*,unsigned)){
 if(value==CR_KEEP)return true;
 if(low&&(field==CR_POWER||field==CR_SLEEP||field==CR_DEEP||field==CR_BRIGHTNESS))return false;
 switch(field){
 case CR_FACE:return face&&face(kv,(unsigned)value);
 case CR_POWER:return portable_sleep_save(kv,value==0?PORTABLE_SLEEP_HYBRID:value==1?PORTABLE_SLEEP_LIGHT:PORTABLE_SLEEP_DEEP);
 case CR_SLEEP:case CR_DEEP:return portable_sleep_timer_save(kv,field==CR_DEEP,(uint32_t)value*1000u);
 case CR_BRIGHTNESS:return value>0&&pqa_preference_save(kv,PQA_BRIGHTNESS_KEY,(unsigned)value*10u,10);
 case CR_VOLUME:return pqa_preference_save(kv,PQA_VOLUME_KEY,(unsigned)value*10u,0);
 case CR_DND:return pqa_dnd_save(kv,value!=0);
 case CR_NOTIFY:return !value?pqa_dnd_save(kv,true):pqa_dnd_save(kv,false)&&portable_alert_save(kv,(unsigned)value);
 case CR_WIFI:case CR_BLUETOOTH:{uint8_t flags=0,bit=field==CR_WIFI?PORTABLE_RADIO_WIFI:PORTABLE_RADIO_BLUETOOTH;if(!portable_radio_load(kv,&flags))return false;if(value&&(flags&PORTABLE_RADIO_AIRPLANE))return false;return portable_radio_save(kv,value?(flags|bit):(flags&~bit));}
 default:return false;
 }
}
static inline bool portable_context_timer(const risc_runtime_api_v1*rt,const alarm_service_v1*service,uint32_t duration,risc_runtime_capability_v1*grant){
 if(!duration||duration>ALARM_COUNTDOWN_MAX||!service)return false;
 alarm_status_v1 status={.struct_size=sizeof(status)};if(service->status(service->context,&status)!=ALARM_OK||status.state!=ALARM_STATE_READY||!status.rtc_seconds||status.rtc_seconds>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS-duration)return false;
 *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
 if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,3,grant))return false;
 const risc_key_value_v1*kv=grant->api;alarm_writer writer={0};bool ok=pqa_preferences_valid(kv)&&!alarm_writer_load(&writer,kv,2)&&writer.saved.revision<UINT32_MAX;
 /* Never replace a user's already-running countdown. */
 if(ok&&writer.saved.enabled&&writer.saved.deadline>status.rtc_seconds)ok=false;
 if(ok){alarm_config config={.revision=writer.saved.revision+1,.deadline=status.rtc_seconds+duration,.created=status.rtc_seconds,.duration=duration,.kind=2,.enabled=1};ok=!alarm_writer_commit(&writer,kv,&config);}
 if(!rt->release(grant))return false;
 memset(grant,0,sizeof(*grant));if(ok)(void)service->refresh(service->context);return ok;
}
