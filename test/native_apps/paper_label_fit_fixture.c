/* Provider-only additions to the maintained coordinate/ownership fixture.
 * Link with --wrap=risc_runtime_get_api. Clock, adapter, input, presentation,
 * restoration and lifecycle are the unmodified paper_coordinate_test.c path. */
#include "RiscRuntimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTimeFormat.h"
#include "AlarmServiceV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const risc_runtime_api_v1 *__real_risc_runtime_get_api(uint32_t);
static const risc_runtime_api_v1 *base_runtime;
static risc_runtime_api_v1 label_runtime;
static risc_key_value_v1 label_storage;
static const risc_key_value_v1 *base_storage;
static twatch_rtc_api_v1 label_rtc;
static const twatch_rtc_api_v1 *base_rtc;
static alarm_service_outputs_v1 label_alarm;

static unsigned setting(const char *name,unsigned fallback) {
 const char *value=getenv(name);return value?(unsigned)strtoul(value,NULL,10):fallback;
}
static int32_t label_get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *length) {
 if(strcmp(key,PORTABLE_TIME_FORMAT_KEY))return base_storage->get(context,key,out,capacity,length);
 unsigned mode=setting("PAPER_LABEL_FORMAT",PORTABLE_TIME_FORMAT_24);
 assert(mode<=PORTABLE_TIME_FORMAT_24&&capacity>=4);
 const uint8_t value[]={0x54,1,(uint8_t)mode,(uint8_t)(mode^0xa5u)};
 memcpy(out,value,sizeof(value));*length=sizeof(value);return RISC_KEY_VALUE_OK;
}
static bool label_read(void *context,twatch_rtc_time_v1 *out) {
 if(!base_rtc->read(context,out))return false;
 out->hour=(uint8_t)setting("PAPER_LABEL_HOUR",20);
 out->minute=(uint8_t)setting("PAPER_LABEL_MINUTE",59);
 assert(out->hour<24&&out->minute<60);
 return setting("PAPER_LABEL_KNOWN",1)!=0;
}
static bool label_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 if(!base_runtime->acquire(name,version,instance,grant))return false;
 if(!strcmp(name,"storage.key-value")) {
  base_storage=grant->api;label_storage=*base_storage;label_storage.get=label_get;grant->api=&label_storage;
 } else if(!strcmp(name,"rtc.clock")) {
  base_rtc=grant->api;label_rtc=*base_rtc;label_rtc.read=label_read;grant->api=&label_rtc;
 } else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)&&!setting("PAPER_LABEL_SOUND",1)) {
  label_alarm=(alarm_service_outputs_v1){.service=*(const alarm_service_v1 *)grant->api,.output_modes=ALARM_MODE_VISUAL};
  label_alarm.service.struct_size=sizeof(label_alarm);grant->api=&label_alarm;
 }
 return true;
}
const risc_runtime_api_v1 *__wrap_risc_runtime_get_api(uint32_t version) {
 const risc_runtime_api_v1 *runtime=__real_risc_runtime_get_api(version);
 if(!runtime)return NULL;
 base_runtime=runtime;label_runtime=*runtime;label_runtime.acquire=label_acquire;return &label_runtime;
}
