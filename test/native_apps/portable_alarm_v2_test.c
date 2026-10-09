/* Production API2 client against the exact Utilities descriptor header. */
#include <assert.h>
#include <stdio.h>
#include "PortableAlarmClient.h"
static unsigned calls,acquires,releases,retains;
static bool stopped;
static int32_t result,reported_error;
static const alarm_service_v1 *offered;
void portable_adapter_retain(void){assert(!stopped);stopped=true;++retains;}
static void io(void){assert(!stopped);++calls;}
static int32_t status(void *p,alarm_status_v1 *out){(void)p;io();*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=reported_error?ALARM_STATE_BLOCKED:ALARM_STATE_READY,.error=reported_error};return result;}
static int32_t operation(void *p){(void)p;io();return result;}
static int32_t ack(void *p,const alarm_token_v1 *t){(void)t;return operation(p);}
static int32_t prepare(void *p,alarm_sleep_v1 *t){(void)t;return operation(p);}
static int32_t resume(void *p,const alarm_sleep_v1 *t){(void)t;return operation(p);}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){io();assert(!strcmp(name,ALARM_SERVICE_CAPABILITY)&&version==2&&!instance);++acquires;grant->api=offered;return offered!=NULL;}
static bool release(risc_runtime_capability_v1 *grant){io();++releases;grant->api=NULL;return true;}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
static alarm_service_descriptor_v2 descriptor(void){return (alarm_service_descriptor_v2){.base={2,sizeof(alarm_service_descriptor_v2),NULL,status,operation,operation,ack,prepare,operation},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=ALARM_MODE_VISUAL,.features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=resume};}
int main(void){
 portable_alarm_client c;alarm_service_descriptor_v2 d=descriptor();offered=&d.base;
 assert(portable_alarm_open(&c,&runtime)&&acquires==1);assert(portable_alarm_output_modes(&c)==0);
 for(unsigned mask=0;mask<=ALARM_MODE_BOTH;mask++){d.output_modes=mask;assert(portable_alarm_output_modes(&c)==mask);}
 assert(portable_alarm_pump(&c)&&c.status.state==ALARM_STATE_READY);assert(portable_alarm_close(&c,&runtime)&&releases==1);
 /* Both exact old allocation sizes are rejected before any suffix read. */
 alarm_service_v1 old=d.base;old.api_version=1;old.struct_size=sizeof(old);offered=&old;
 assert(!portable_alarm_open(&c,&runtime));old.struct_size=sizeof(old)+sizeof(void*);assert(!portable_alarm_open(&c,&runtime));
 for(unsigned fault=0;fault<15;fault++){
  d=descriptor();switch(fault){
   case 0:d.base.api_version=1;break;case 1:d.base.struct_size=sizeof(d)-1;break;
   case 2:d.tag^=1;break;case 3:d.descriptor_version++;break;
   case 4:d.output_modes=4;break;case 5:d.features=2;break;
   case 6:d.features=0;break;case 7:d.resume_sleep=NULL;break;
   case 8:d.base.status=NULL;break;case 9:d.base.step=NULL;break;
   case 10:d.base.refresh=NULL;break;case 11:d.base.acknowledge=NULL;break;
   case 12:d.base.prepare_sleep=NULL;break;case 13:d.base.stop_only=NULL;break;
   case 14:d.base.api_version=3;break;
  }
  offered=&d.base;assert(!portable_alarm_open(&c,&runtime));
 }
 d=descriptor();d.features=0;d.resume_sleep=NULL;offered=&d.base;assert(portable_alarm_open(&c,&runtime));
 d.tag=0;unsigned before=calls;alarm_token_v1 token={0};
 assert(!portable_alarm_pump(&c)&&!portable_alarm_status(&c)&&!portable_alarm_refresh(&c)&&!portable_alarm_acknowledge(&c,&token)&&!portable_alarm_failure_stop(&c));assert(calls==before);
 d=descriptor();
 const int32_t safe[]={ALARM_STORAGE,ALARM_RTC,ALARM_STALE,ALARM_BUSY,ALARM_INVALID,ALARM_EXHAUSTED,ALARM_FOREGROUND};
 for(unsigned i=0;i<sizeof(safe)/sizeof(safe[0]);i++){
  result=safe[i];assert(portable_alarm_refresh(&c)&&portable_alarm_acknowledge(&c,&token));
  result=ALARM_OK;reported_error=safe[i];assert(portable_alarm_status(&c)&&c.status.error==safe[i]&&!stopped);
 }
 reported_error=0;
 for(unsigned fault=0;fault<7;fault++){
  stopped=false;result=ALARM_OK;reported_error=0;assert(portable_alarm_open(&c,&runtime));unsigned previous_retains=retains;
  if(fault==0){result=ALARM_RETAINED;assert(!portable_alarm_pump(&c));}
  if(fault==1){result=ALARM_RETAINED;assert(!portable_alarm_refresh(&c));}
  if(fault==2){result=ALARM_RETAINED;assert(!portable_alarm_acknowledge(&c,&token));}
  if(fault==3){result=ALARM_RETAINED;assert(!portable_alarm_failure_stop(&c));}
  if(fault==4){result=ALARM_RETAINED;assert(!portable_alarm_status(&c));}
  if(fault==5){reported_error=ALARM_RETAINED;assert(!portable_alarm_status(&c));}
  if(fault==6){result=ALARM_OUTPUT;assert(!portable_alarm_pump(&c));}
  assert(stopped&&c.retained&&retains==previous_retains+1);before=calls;
  assert(!portable_alarm_pump(&c)&&!portable_alarm_status(&c)&&!portable_alarm_refresh(&c)&&!portable_alarm_acknowledge(&c,&token)&&!portable_alarm_failure_stop(&c)&&!portable_alarm_close(&c,&runtime));assert(calls==before);
 }
 puts("API2 production client: exact negotiation, complete descriptor, safe errors and retained no-I/O PASS");return 0;
}
