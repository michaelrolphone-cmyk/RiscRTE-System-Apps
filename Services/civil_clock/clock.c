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
static const risc_civil_clock_api_v1 api={1,sizeof(api),NULL,read_time};
static const risc_driver_v2 driver={2,sizeof(driver),CIVIL_CLOCK_ID,RISC_CIVIL_CLOCK_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t version){return version==2?&driver:NULL;}
