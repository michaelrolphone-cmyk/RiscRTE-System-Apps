/* Production API2 client against the exact Utilities descriptor header. */
#include <assert.h>
#include <stdio.h>
#include "PortableAlarmClient.h"
static unsigned calls,acquires,releases,retains;
static bool stopped;
#ifdef TEST_ALARM_FACADE
static bool null_success,operation_lost,status_lost,acquire_true_lost,release_true_lost;
#define EXPECTED_ALARM_CAPABILITY "alarm.service.facade"
#else
#define EXPECTED_ALARM_CAPABILITY ALARM_SERVICE_CAPABILITY
#endif
#if defined(PORTABLE_ALARM_TERMINAL_RETENTION) || defined(PORTABLE_ALARM_ACQUISITION_CUSTODY)
static bool missing_runtime,activation_lost,release_lost;
#endif
static int32_t result,reported_error;
static const alarm_service_v1 *offered;
void portable_adapter_retain(void){assert(!stopped);stopped=true;++retains;}
void portable_adapter_retain_silent(void){portable_adapter_retain();}
bool portable_adapter_retained(void){return stopped;}
static void io(void){assert(!stopped);++calls;}
static int32_t status(void *p,alarm_status_v1 *out){(void)p;io();
#ifdef TEST_ALARM_FACADE
 if(status_lost)missing_runtime=true;
#endif
*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=reported_error?ALARM_STATE_BLOCKED:ALARM_STATE_READY,.error=reported_error};return result;}
static int32_t operation(void *p){(void)p;io();
#ifdef TEST_ALARM_FACADE
 if(operation_lost)missing_runtime=true;
#endif
 return result;}
static int32_t ack(void *p,const alarm_token_v1 *t){(void)t;return operation(p);}
static int32_t prepare(void *p,alarm_sleep_v1 *t){(void)t;return operation(p);}
static int32_t resume(void *p,const alarm_sleep_v1 *t){(void)t;return operation(p);}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){io();assert(!strcmp(name,EXPECTED_ALARM_CAPABILITY)&&version==2&&!instance);++acquires;
#ifdef TEST_ALARM_FACADE
 if(acquire_true_lost)missing_runtime=true;
#endif
#if defined(PORTABLE_ALARM_TERMINAL_RETENTION) || defined(PORTABLE_ALARM_ACQUISITION_CUSTODY)
 if(activation_lost){missing_runtime=true;return false;}
#endif
 grant->api=offered;
#ifdef TEST_ALARM_FACADE
 return offered!=NULL||null_success;
#else
 return offered!=NULL;
#endif
}
static bool release(risc_runtime_capability_v1 *grant){io();++releases;
#if defined(PORTABLE_ALARM_TERMINAL_RETENTION) || defined(PORTABLE_ALARM_ACQUISITION_CUSTODY)
 if(release_lost){missing_runtime=true;return false;}
#endif
 grant->api=NULL;
#ifdef TEST_ALARM_FACADE
 if(release_true_lost)missing_runtime=true;
#endif
 return true;}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
#if defined(PORTABLE_ALARM_TERMINAL_RETENTION) || defined(PORTABLE_ALARM_ACQUISITION_CUSTODY)
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&!stopped&&!missing_runtime?&runtime:NULL;}
#endif
static alarm_service_descriptor_v2 descriptor(void){return (alarm_service_descriptor_v2){.base={2,sizeof(alarm_service_descriptor_v2),NULL,status,operation,operation,ack,prepare,operation},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=ALARM_MODE_VISUAL,.features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=resume};}
int main(void){
 portable_alarm_client c;alarm_service_descriptor_v2 d=descriptor();offered=&d.base;
 assert(portable_alarm_open(&c,&runtime)&&acquires==1);assert(portable_alarm_output_modes(&c)==0);
 for(unsigned mask=0;mask<=ALARM_MODE_BOTH;mask++){d.output_modes=mask;assert(portable_alarm_output_modes(&c)==mask);}
 assert(portable_alarm_pump(&c)&&c.status.state==ALARM_STATE_READY);assert(portable_alarm_close(&c,&runtime)&&releases==1);
#ifdef TEST_ALARM_FACADE
 offered=NULL;null_success=true;unsigned old_releases=releases;
 assert(!portable_alarm_open(&c,&runtime)&&c.malformed&&!c.retained&&releases==old_releases+1&&!c.grant.api&&!c.grant.slot&&!c.grant.generation);
 null_success=false;assert(!portable_alarm_open(&c,&runtime)&&!c.malformed&&releases==old_releases+1);
#endif
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
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
 stopped=false;result=ALARM_OK;reported_error=0;assert(portable_alarm_open(&c,&runtime));
 stopped=true;before=calls; /* Another native owner retained first. */
 assert(!c.retained&&!portable_alarm_pump(&c)&&!portable_alarm_status(&c)&&!portable_alarm_refresh(&c)&&!portable_alarm_acknowledge(&c,&token)&&!portable_alarm_failure_stop(&c)&&!portable_alarm_close(&c,&runtime)&&!portable_alarm_open(&c,&runtime));assert(calls==before);
 stopped=false;activation_lost=true;before=calls;
 assert(!portable_alarm_open(&c,&runtime)&&stopped&&c.retained&&calls==before+1);
 activation_lost=false;missing_runtime=false;stopped=false;assert(portable_alarm_open(&c,&runtime));
 release_lost=true;before=calls;assert(!portable_alarm_close(&c,&runtime)&&stopped&&c.retained&&calls==before+1);
#endif
#ifdef TEST_ALARM_FACADE
 /* Guarded runtime wrappers are legitimate; native identity is captured separately. */
 missing_runtime=false;stopped=false;release_lost=false;activation_lost=false;
 risc_runtime_api_v1 wrapper=runtime;
 result=ALARM_OK;reported_error=0;offered=&d.base;
 assert(portable_alarm_open(&c,&wrapper));assert(portable_alarm_close(&c,&wrapper));
 for(unsigned fault=0;fault<7;fault++){
  missing_runtime=false;stopped=false;operation_lost=false;status_lost=false;acquire_true_lost=false;release_true_lost=false;
  result=fault==1?ALARM_PENDING:ALARM_OK;assert(portable_alarm_open(&c,&wrapper));
  before=calls;operation_lost=true;
  if(fault<2)assert(!portable_alarm_pump(&c));
  if(fault==2)assert(!portable_alarm_failure_stop(&c));
  if(fault==3)assert(!portable_alarm_refresh(&c));
  if(fault==4)assert(!portable_alarm_acknowledge(&c,&token));
  if(fault==5){operation_lost=false;release_true_lost=true;assert(!portable_alarm_close(&c,&wrapper));}
  if(fault==6){operation_lost=false;status_lost=true;assert(!portable_alarm_status(&c));}
  assert(stopped&&c.retained&&calls==before+1);
  before=calls;
  assert(!portable_alarm_pump(&c)&&!portable_alarm_status(&c)&&!portable_alarm_failure_stop(&c)&&!portable_alarm_close(&c,&wrapper));
  assert(calls==before);
 }
 missing_runtime=false;stopped=false;release_true_lost=false;operation_lost=false;acquire_true_lost=true;before=calls;
 assert(!portable_alarm_open(&c,&wrapper)&&stopped&&c.retained&&calls==before+1);
#endif
 puts("API2 production client: exact negotiation, complete descriptor, safe errors and retained no-I/O PASS");return 0;
}
