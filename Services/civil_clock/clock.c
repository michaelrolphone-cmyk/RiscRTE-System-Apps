#include "RiscCivilClockV1.h"
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include "PortableTimeZone.h"
#include <string.h>
#include <stdio.h>
#ifdef CIVIL_NATIVE_UTC
#include "RiscPlatformRealtimeV1.h"
#include "RiscBoundKeyValueV1.h"
#include "PortableTimeZonePreference.h"
#include "PortableRtcBasis.h"
static const risc_platform_realtime_api_v1 *realtime;
static const risc_bound_key_value_v1 *preferences;
#else
#include "PortableRtcClock.h"
#include "PortableTime.h"
static const twatch_rtc_api_v1 *rtc;
#endif
static bool started,retained;
static bool alive(void){if(!retained&&!risc_runtime_get_api(1))retained=true;return !retained;}
#ifdef CIVIL_NATIVE_UTC
static int32_t preference_get(void *c,const char *key,void *buffer,uint32_t capacity,uint32_t *size){
    (void)c;if(!alive())return RISC_BOUND_KEY_VALUE_CONTEXT;
    int32_t rc=preferences->get(preferences->context,key,buffer,capacity,size);
    if(rc==RISC_BOUND_KEY_VALUE_CONTEXT||!alive())retained=true;
    return rc;
}
#endif
static int32_t read_time(void *ctx,risc_civil_time_v1 *out){
    (void)ctx;if(!alive())return -9;if(!started||!out||out->struct_size!=sizeof(*out))return -1;
    risc_civil_time_v1 result={.struct_size=sizeof(result)};portable_timezone_civil local;
#ifdef CIVIL_NATIVE_UTC
    risc_realtime_snapshot_v1 sample={.struct_size=sizeof(sample)};int32_t rc=realtime->read(realtime->context,&sample);
    if(rc==RISC_REALTIME_CONTEXT||!alive()){retained=true;return -9;}
    if(rc||sample.validity!=RISC_REALTIME_VALID||sample.nanoseconds>=1000000000u)return -1;
    const risc_key_value_v1 kv={1,sizeof(kv),NULL,preference_get,NULL};
    int zr=portable_timezone_preference_load(&kv,result.zone);if(!alive())return -9;
    portable_timezone_rule rule;
    if((zr!=PORTABLE_TIMEZONE_LOADED&&zr!=PORTABLE_TIMEZONE_MISSING)||portable_timezone_resolve(result.zone,sizeof(result.zone),&rule)!=PORTABLE_TIMEZONE_OK||portable_timezone_utc_to_local(&rule,sample.epoch_seconds,&local,NULL)!=PORTABLE_TIMEZONE_OK)return -1;
#else
    twatch_rtc_time_v1 raw,civil;bool ok=rtc->read(rtc->context,&raw);if(!alive())return -9;
    if(!ok||!portable_time_forward(&raw,&civil))return -1;
    local=(portable_timezone_civil){civil.year,civil.month,civil.day,civil.hour,civil.minute,civil.second,civil.weekday};
    snprintf(result.zone,sizeof(result.zone),"%s",portable_time_zone());
#endif
    if(local.year<2000||local.year>2099)return -1;
    result.minute=(uint32_t)local.hour*60+local.minute;local.hour=local.minute=local.second=0;int64_t epoch;
    if(portable_timezone_civil_to_epoch(&local,&epoch)!=PORTABLE_TIMEZONE_OK)return -1;
    result.day=(int32_t)((uint32_t)(epoch-INT64_C(946684800))/86400u);*out=result;return 0;
}
/* Full-clock policy suffix: application owns acquisition/seed cadence; provider
 * owns installed clock-domain interpretation. Never mutates persistent state. */
static int32_t policy_rule(char zone[40],portable_timezone_rule*rule){
#ifdef CIVIL_NATIVE_UTC
 const risc_key_value_v1 kv={1,sizeof(kv),NULL,preference_get,NULL};
 int rc=portable_timezone_preference_load(&kv,zone);if(!alive())return -9;
 if(rc!=PORTABLE_TIMEZONE_LOADED&&rc!=PORTABLE_TIMEZONE_MISSING)return -1;
#else
 snprintf(zone,40,"%s",portable_time_zone());
#endif
 return portable_timezone_resolve(zone,40,rule)==PORTABLE_TIMEZONE_OK?0:-1;
}
static int32_t project_time(void*ctx,int64_t epoch,uint32_t nanoseconds,risc_civil_calendar_v1*out){
 (void)ctx;if(!alive())return -9;
 if(!started||!out||out->struct_size<sizeof(*out)||nanoseconds>=1000000000u)return -1;
 risc_civil_calendar_v1 result={.struct_size=sizeof(result),.nanoseconds=nanoseconds};
 portable_timezone_rule rule;int rc=policy_rule(result.zone,&rule);if(rc)return rc;
 portable_timezone_civil local;
#ifdef CIVIL_NATIVE_UTC
 if(portable_timezone_utc_to_local(&rule,epoch,&local,NULL)!=PORTABLE_TIMEZONE_OK)return -1;
#else
 /* Preserve the delivered historical Denver rules inside the provider. */
 portable_timezone_civil raw;
 if(epoch<INT64_C(946656000)||epoch>INT64_C(4102415999)||
    portable_timezone_epoch_to_civil(epoch+INT64_C(28800),&raw)!=PORTABLE_TIMEZONE_OK)return -1;
 twatch_rtc_time_v1 rtc_value={(uint16_t)raw.year,raw.month,raw.day,raw.weekday,raw.hour,raw.minute,raw.second},display;
 if(!portable_time_forward(&rtc_value,&display))return -1;
 local=(portable_timezone_civil){display.year,display.month,display.day,display.hour,display.minute,display.second,display.weekday};
#endif
 if(local.year<2000||local.year>2099)return -1;
 result.year=local.year;result.month=local.month;result.day=local.day;result.hour=local.hour;
 result.minute=local.minute;result.second=local.second;result.weekday=local.weekday;
 *out=result;return 0;
}
#ifdef CIVIL_NATIVE_UTC
static int32_t readonly_put(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;return -1;}
#endif
static int32_t interpret_rtc(void*ctx,const risc_civil_calendar_v1*raw,int64_t*out){
 (void)ctx;if(!alive())return -9;
 if(!started||!raw||raw->struct_size<sizeof(*raw)||!out||raw->nanoseconds||raw->reserved||raw->year<2000||raw->year>2099)return -1;
 portable_timezone_civil civil={raw->year,raw->month,raw->day,raw->hour,raw->minute,raw->second,raw->weekday};
 int64_t epoch=0;
#ifdef CIVIL_NATIVE_UTC
 char zone[40];portable_timezone_rule rule;int rc=policy_rule(zone,&rule);if(rc)return rc;
 /* Existing codec requires a complete KV table, but this adapter never writes. */
 const risc_key_value_v1 kv={1,sizeof(kv),NULL,preference_get,readonly_put};
 portable_rtc_basis basis;rc=portable_rtc_basis_load(&kv,&basis);if(!alive())return -9;
 if(rc!=PORTABLE_RTC_BASIS_LOADED&&rc!=PORTABLE_RTC_BASIS_MISSING)return -1;
 portable_timezone_rtc_result interpreted;
 if(portable_timezone_interpret_rtc(&rule,&civil,basis.stores_utc,basis.reference_epoch,-1,&interpreted)!=PORTABLE_TIMEZONE_OK||
    interpreted.mode_changed)return -1; /* Delivered explicit no-basis-change policy. */
 epoch=interpreted.epoch;
#else
 if(portable_timezone_civil_to_epoch(&civil,&epoch)!=PORTABLE_TIMEZONE_OK)return -1;
 epoch-=INT64_C(28800); /* Installed Watch RTC domain, not application policy. */
#endif
#ifdef CIVIL_NATIVE_UTC
 if(epoch<INT64_C(946684800)||epoch>INT64_C(4102444799))return -1;
#else
 /* The first accepted 2000 calendar instant in UTC+8 is still1999 in UTC. */
 if(epoch<INT64_C(946656000)||epoch>INT64_C(4102444799))return -1;
#endif
 *out=epoch;return 0;
}
static int32_t inverse_time(void *ctx,const risc_civil_calendar_v1 *civil,risc_civil_inverse_v1 *out) {
 (void)ctx;if(!alive())return -9;
 if(!started||!civil||civil->struct_size<sizeof(*civil)||civil->reserved||civil->nanoseconds||
    civil->year<2000||civil->year>2099||!out||out->struct_size<sizeof(*out))return -1;
 portable_timezone_civil value={civil->year,civil->month,civil->day,civil->hour,civil->minute,civil->second,civil->weekday};
 int64_t checked;if(portable_timezone_civil_to_epoch(&value,&checked)!=PORTABLE_TIMEZONE_OK)return -1;
 risc_civil_inverse_v1 result={.struct_size=sizeof(result)};
#ifdef CIVIL_NATIVE_UTC
 char zone[40];portable_timezone_rule rule;int rc=policy_rule(zone,&rule);if(rc)return rc;
 portable_timezone_inverse candidates;
 rc=portable_timezone_local_to_utc(&rule,&value,&candidates);
 if(rc!=PORTABLE_TIMEZONE_OK&&rc!=PORTABLE_TIMEZONE_GAP&&rc!=PORTABLE_TIMEZONE_FOLD)return -1;
 result.count=candidates.count;
 for(unsigned i=0;i<result.count;i++)result.epoch[i]=candidates.candidate[i].epoch;
#else
 twatch_rtc_time_v1 local={(uint16_t)civil->year,civil->month,civil->day,civil->weekday,civil->hour,civil->minute,civil->second};
 portable_time_candidate candidates[2];result.count=portable_time_inverse(&local,candidates);
 for(unsigned i=0;i<result.count;i++) {
  const twatch_rtc_time_v1 *r=&candidates[i].rtc;
  portable_timezone_civil raw={r->year,r->month,r->day,r->hour,r->minute,r->second,r->weekday};
  if(portable_timezone_civil_to_epoch(&raw,&result.epoch[i])!=PORTABLE_TIMEZONE_OK)return -1;
  result.epoch[i]-=INT64_C(28800);
 }
#endif
 *out=result;return 0;
}
static int32_t cached_rule(void *ctx,char *out,uint32_t capacity) {
 (void)ctx;if(!alive())return -9;
 if(!started||!out||capacity<40)return -1;
 char token[40]={0};
#ifdef CIVIL_NATIVE_UTC
 portable_timezone_rule rule;int rc=policy_rule(token,&rule);if(rc)return rc;
#else
 /* Explicit installed historical policy. Generic application resolver reads
  * only the supplied era data; no application device/year special case. */
 memcpy(token,"@2007:7,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0",40);
#endif
 memcpy(out,token,40);return 0;
}
static bool start(const risc_provider_dependency_v1 *deps,size_t count){
    if(started||retained||!deps)return false;
#ifdef CIVIL_NATIVE_UTC
    if(count!=2)return false;
    realtime=NULL;preferences=NULL;
    for(size_t i=0;i<count;i++){
        if(!deps[i].capability_id||deps[i].api_version!=1)return false;
        if(!strcmp(deps[i].capability_id,"platform.realtime")&&!realtime)realtime=deps[i].api;
        else if(!strcmp(deps[i].capability_id,RISC_BOUND_KEY_VALUE_CAPABILITY)&&!preferences)preferences=deps[i].api;else return false;
    }
    if(!realtime||realtime->api_version!=1||realtime->struct_size<sizeof(*realtime)||!realtime->read||!preferences||preferences->api_version!=1||preferences->struct_size<sizeof(*preferences)||!preferences->get)return false;
#else
    if(count!=1||!deps[0].capability_id||strcmp(deps[0].capability_id,"rtc.clock")||deps[0].api_version!=2)return false;
    rtc=deps[0].api;if(!rtc||rtc->api_version!=2||rtc->struct_size<sizeof(*rtc)||!rtc->read)return false;
#endif
    started=true;return true;
}
static bool quiesce(void){return !retained;}
static void stop(void){if(!retained)started=false;}
static const risc_civil_clock_retained_v1 api={{{1,sizeof(api),NULL,read_time,1,RISC_CIVIL_CLOCK_POLICY_TAG,project_time,interpret_rtc},1,RISC_CIVIL_CLOCK_SCHEDULE_TAG,inverse_time},1,RISC_CIVIL_CLOCK_RETAINED_TAG,cached_rule};
static const risc_driver_v2 driver={2,sizeof(driver),CIVIL_CLOCK_ID,RISC_CIVIL_CLOCK_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t version){return version==2?&driver:NULL;}
