#include "PortableTimeSync.h"
#include "RiscRuntimeClockV1.h"
#include <string.h>
static int retained(portable_time_sync *c){c->state=PORTABLE_TIME_SYNC_RETAINED;c->result=RISC_TIME_SAMPLE_RETAINED;return c->state;}
static bool safe(portable_time_sync *c){
 if(!c || c->self!=c)return false;
 if(c->state==PORTABLE_TIME_SYNC_RETAINED)return false;
 if(!c->guard || !c->guard(c->guard_context)){retained(c);return false;}
 return true;
}
static bool empty(const risc_runtime_capability_v1 *g){return g->struct_size==sizeof(*g)&&!g->slot&&!g->generation&&!g->api;}
static bool clock_now(portable_time_sync *c,uint32_t *now){
 if(!safe(c))return false;
 bool ok=risc_runtime_monotonic_ms_v1(c->runtime,now);
 if(!safe(c))return false;
 if(!ok){retained(c);return false;}
 return true;
}
static bool typed(portable_time_sync *c,int result){
 if(!safe(c))return false;
 if(result==RISC_TIME_SAMPLE_RETAINED || result==RISC_TIME_SAMPLE_CONTEXT ||
    result<RISC_TIME_SAMPLE_CLOSED || result>RISC_TIME_SAMPLE_PENDING){retained(c);return false;}
 return true;
}
static int finish(portable_time_sync *c,int state,int reason){
 if(!safe(c))return PORTABLE_TIME_SYNC_RETAINED;
 c->finish_state=state;c->finish_reason=reason;
 if(c->token){
  int rc=c->api->close(c->api->context,c->token);
  if(!typed(c,rc))return c->state;
  if(rc==RISC_TIME_SAMPLE_PENDING){c->state=PORTABLE_TIME_SYNC_CLOSING;return c->state;}
  if(rc!=RISC_TIME_SAMPLE_OK)return retained(c);
  c->token=0;
 }
 if(c->grant.api){
  risc_runtime_capability_v1 released=c->grant;
  bool ok=c->runtime->release(&released);
  if(!safe(c))return c->state;
  if(!ok || !empty(&released))return retained(c);
  c->grant=released;c->api=NULL;
 }
 c->state=state;c->result=reason;return state;
}
int portable_time_sync_cancel(portable_time_sync *c){
 if(!c || !c->self)return PORTABLE_TIME_SYNC_IDLE;
 if(!safe(c))return PORTABLE_TIME_SYNC_RETAINED;
 return finish(c,PORTABLE_TIME_SYNC_IDLE,RISC_TIME_SAMPLE_CLOSED);
}
int portable_time_sync_start(portable_time_sync *c,const risc_runtime_api_v1 *rt,
 portable_time_sync_guard guard,void *context,const char *capability,uint64_t instance,
 const char *server,uint32_t timeout){
 if(!c)return PORTABLE_TIME_SYNC_RETAINED;
 if(c->self && (c->self!=c || c->state==PORTABLE_TIME_SYNC_RETAINED || c->token || c->grant.api))return retained(c);
 *c=(portable_time_sync){.self=c,.runtime=rt,.guard=guard,.guard_context=context,
  .state=PORTABLE_TIME_SYNC_FAILED,.result=RISC_TIME_SAMPLE_INVALID,.timeout_ms=timeout};
 if(!safe(c))return c->state;
 if(!rt || rt->api_version!=RISC_RUNTIME_API_V1 || rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
    !rt->acquire || !rt->release || !capability || !server || !timeout || timeout>5000)return c->state;
 if(!clock_now(c,&c->started_ms))return c->state;
 c->grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->grant)};
 bool acquired=rt->acquire(capability,RISC_TIME_SAMPLE_API_V1,instance,&c->grant);
 if(!safe(c))return c->state;
 if(!acquired){if(!empty(&c->grant))return retained(c);c->result=RISC_TIME_SAMPLE_NETWORK;return c->state;}
 if(c->grant.struct_size!=sizeof(c->grant)||!c->grant.slot||!c->grant.generation||!c->grant.api)return retained(c);
 c->api=c->grant.api;
 if(c->api->api_version!=1 || c->api->struct_size<sizeof(*c->api) || !c->api->open || !c->api->step || !c->api->close)
  return finish(c,PORTABLE_TIME_SYNC_FAILED,RISC_TIME_SAMPLE_PROTOCOL);
 risc_time_sample_request_v1 request={sizeof(request),timeout,server};
 int rc=c->api->open(c->api->context,&request,&c->token);
 if(!typed(c,rc))return c->state;
 if(rc!=RISC_TIME_SAMPLE_OK){if(c->token)return retained(c);return finish(c,PORTABLE_TIME_SYNC_FAILED,rc);}
 if(!c->token)return retained(c);
 c->state=PORTABLE_TIME_SYNC_PENDING;return c->state;
}
bool portable_time_sync_epoch(portable_time_sync *c,int64_t *epoch){
 if(!epoch || !safe(c) || c->state!=PORTABLE_TIME_SYNC_SAMPLE)return false;
 uint32_t now;if(!clock_now(c,&now))return false;
 /* Both values share the boot-local clock; modular subtraction also handles
  * the public 32-bit clock wrapping during a bounded sampling operation. */
 uint32_t age=now-(uint32_t)c->sample.observed_monotonic_ms;
 if(age>1000u || c->sample.utc_seconds>INT32_MAX || c->sample.utc_seconds<UINT64_C(1704067200))return false;
 *epoch=(int64_t)c->sample.utc_seconds;return true;
}
int portable_time_sync_step(portable_time_sync *c){
 if(!safe(c))return PORTABLE_TIME_SYNC_RETAINED;
 if(c->state==PORTABLE_TIME_SYNC_CLOSING)return finish(c,c->finish_state,c->finish_reason);
 if(c->state!=PORTABLE_TIME_SYNC_PENDING)return c->state;
 uint32_t now;if(!clock_now(c,&now))return c->state;
 if((uint32_t)(now-c->started_ms)>c->timeout_ms)return finish(c,PORTABLE_TIME_SYNC_FAILED,RISC_TIME_SAMPLE_TIMEOUT);
 risc_time_sample_v1 value={.struct_size=sizeof(value)};
 int rc=c->api->step(c->api->context,c->token,&value);
 if(!typed(c,rc))return c->state;
 if(rc==RISC_TIME_SAMPLE_PENDING)return c->state;
 if(rc!=RISC_TIME_SAMPLE_OK)return finish(c,PORTABLE_TIME_SYNC_FAILED,rc);
 if(value.struct_size!=sizeof(value)||value.flags!=RISC_TIME_SAMPLE_UNAUTHENTICATED||value.reserved||
    (uint32_t)((uint32_t)value.observed_monotonic_ms-c->started_ms)>c->timeout_ms||
    value.resolution_ns!=1000000000u||value.utc_seconds<UINT64_C(1704067200)||value.utc_seconds>INT32_MAX)
  return finish(c,PORTABLE_TIME_SYNC_FAILED,RISC_TIME_SAMPLE_PROTOCOL);
 c->sample=value;
 if(finish(c,PORTABLE_TIME_SYNC_SAMPLE,RISC_TIME_SAMPLE_OK)!=PORTABLE_TIME_SYNC_SAMPLE)return c->state;
 int64_t epoch;
 if(!portable_time_sync_epoch(c,&epoch) && c->state!=PORTABLE_TIME_SYNC_RETAINED){c->state=PORTABLE_TIME_SYNC_FAILED;c->result=RISC_TIME_SAMPLE_TIMEOUT;}
 return c->state;
}
