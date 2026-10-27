#pragma once
#include "PortableRealtimeClient.h"
#include "PortableAlarmClient.h"
#include "AlarmClockEditV2.h"
/* Shared foreground UTC transaction used by manual Settings and explicit
 * network synchronization. It does not modify timezone or manual preferences.
 * Caller supplies current alarm client and pure custody guard; no background
 * work or independent service ownership is introduced. */
typedef struct {
 portable_alarm_client *alarms;
 const risc_runtime_api_v1 *runtime;
 portable_realtime_phase_guard guard;
 void (*retain)(void *);
 void *context;
 bool (*prepare_seed)(void *,int64_t *); /* Optional freshness check after alarm intent; false cancels before seed. */
 alarm_clock_edit_permit_v2 permit;
 bool permit_valid;
 const char *message;
 portable_realtime_client reader;
 portable_realtime_seed_result seeded;
} portable_clock_commit;
static inline bool portable_clock_commit_live(portable_clock_commit *c){return c->guard(c->context)==PORTABLE_REALTIME_GUARD_SAFE && !c->alarms->retained && portable_alarm_callable(c->alarms);}
static inline bool portable_clock_commit_result(portable_clock_commit *c,int32_t result){
 if(result==ALARM_RETAINED || result==ALARM_OUTPUT || result<ALARM_FOREGROUND || result>ALARM_PENDING){
  c->alarms->retained=true;c->retain(c->context);return false;
 }
 if(!portable_alarm_result(c->alarms,result) || !portable_clock_commit_live(c))return false;
 return true;
}
static inline bool portable_clock_commit_begin(portable_clock_commit *c,int64_t epoch){
 c->permit_valid=false;
 const alarm_clock_edit_descriptor_v2 *edit=alarm_clock_edit_descriptor(c->alarms->api);
 if(!portable_clock_commit_live(c) || !edit){c->message="Alarm clock editor unavailable";return false;}
 if(epoch<INT64_C(946684800) || epoch>INT32_MAX){c->message="Invalid alarm clock range";return false;}
 const uint32_t wanted=(uint32_t)(epoch-INT64_C(946684800));
 for(unsigned n=0;n<128;n++) {
  alarm_clock_edit_permit_v2 permit={.struct_size=sizeof(permit)};
  int32_t result=edit->begin(c->alarms->api->context,wanted,&permit);
  if(!portable_clock_commit_result(c,result))return false;
  if(result==ALARM_OK) {
   if(permit.struct_size!=sizeof(permit) || !permit.nonce || !permit.generation || permit.requested!=wanted || permit.reserved[0] || permit.reserved[1]) {
    c->retain(c->context);return false;
   }
   c->permit=permit;c->permit_valid=true;return true;
  }
  if(result!=ALARM_PENDING){c->message="Alarm clock edit not ready; dismiss/retry";return false;}
  result=c->alarms->api->step(c->alarms->api->context);
  if(!portable_clock_commit_result(c,result))return false;
  if(result!=ALARM_OK && result!=ALARM_PENDING && result!=ALARM_RTC){c->message="Alarm reconciliation failed; not saved";return false;}
 }
 c->message="Alarm reconciliation pending; not saved";return false;
}
static inline bool portable_clock_commit_cancel_before_seed(portable_clock_commit *c){
 if(!c->permit_valid || !portable_clock_commit_live(c))return false;
 const alarm_clock_edit_descriptor_v2 *edit=alarm_clock_edit_descriptor(c->alarms->api);
 if(!edit){c->retain(c->context);return false;}
 c->permit_valid=false;
 int32_t result=edit->cancel(c->alarms->api->context,&c->permit);
 if(!portable_clock_commit_result(c,result))return false;
 if(result!=ALARM_OK && result!=ALARM_PENDING){c->message="Clock edit recovery required";return false;}
 return true;
}
static inline bool portable_clock_commit_status(portable_clock_commit *c){
 if(!portable_clock_commit_live(c))return false;
 c->alarms->status=(alarm_status_v1){.struct_size=sizeof(c->alarms->status)};
 int32_t result=c->alarms->api->status(c->alarms->api->context,&c->alarms->status);
 if(!portable_clock_commit_result(c,result) || result!=ALARM_OK ||
    !portable_clock_commit_result(c,c->alarms->status.error))return false;
 return c->alarms->status.api_version==1 && c->alarms->status.struct_size>=sizeof(c->alarms->status) &&
  c->alarms->status.state<=ALARM_STATE_CUE;
}
static inline bool portable_clock_commit_finish(portable_clock_commit *c){
 if(!c->permit_valid || !portable_clock_commit_live(c))return false;
 const alarm_clock_edit_descriptor_v2 *edit=alarm_clock_edit_descriptor(c->alarms->api);
 if(!edit){c->retain(c->context);return false;}
 c->permit_valid=false; /* A consumed/refused permit is never retried. */
 int32_t result=edit->finish(c->alarms->api->context,&c->permit);
 if(!portable_clock_commit_result(c,result))return false;
 if(result!=ALARM_OK && result!=ALARM_PENDING){c->message="Time saved; alarm recovery required";return false;}
 alarm_clock_edit_status_v2 saved={.struct_size=sizeof(saved)};
 result=edit->edit_status(c->alarms->api->context,&saved);
 if(!portable_clock_commit_result(c,result))return false;
 if(result!=ALARM_OK || saved.struct_size!=sizeof(saved) || saved.phase!=ALARM_CLOCK_EDIT_VERIFIED || saved.generation!=c->permit.generation || saved.requested!=c->permit.requested){
  c->message="Time saved; alarm commit unconfirmed";c->retain(c->context);return false;
 }
 for(unsigned n=0;n<128;n++) {
  if(!portable_clock_commit_status(c) || !portable_clock_commit_live(c))return false;
  if(c->alarms->status.output_uncertain){c->retain(c->context);return false;}
  if(c->alarms->status.error || c->alarms->status.state==ALARM_STATE_BLOCKED){c->message="Time saved; review alarm status";return false;}
  if(c->alarms->status.state==ALARM_STATE_READY || portable_alarm_owned(c->alarms))return true;
  result=c->alarms->api->step(c->alarms->api->context);
  if(!portable_clock_commit_result(c,result))return false;
  if(result!=ALARM_OK && result!=ALARM_PENDING){c->message="Time saved; review alarm status";return false;}
 }
 c->message="Time saved; alarm reconciliation pending";return false;
}

static inline bool portable_clock_commit_halted(portable_clock_commit *c,int status){
 if(status==PORTABLE_REALTIME_UNCERTAIN || status==PORTABLE_REALTIME_RETAINED || status==PORTABLE_REALTIME_CONTEXT){c->retain(c->context);return true;}
 return c->guard(c->context)!=PORTABLE_REALTIME_GUARD_SAFE;
}
static inline bool portable_clock_commit_apply(portable_clock_commit *c,int64_t epoch){
 if(!c || !c->alarms || !c->runtime || !c->guard || !c->retain)return false;
 c->message="Clock update unavailable";
 c->seeded=(portable_realtime_seed_result){0};
 if(!portable_clock_commit_live(c) || !portable_clock_commit_begin(c,epoch))return false;
 if(c->prepare_seed && !c->prepare_seed(c->context,&epoch)){
  if(!portable_clock_commit_live(c))return false;
  if(!portable_clock_commit_cancel_before_seed(c))return false;
  c->message="Time sample expired; retry";return false;
 }
 /* prepare_seed may validate, but never change the already permitted epoch. */
 if(epoch!=INT64_C(946684800)+(int64_t)c->permit.requested){
  (void)portable_clock_commit_cancel_before_seed(c);c->message="Time sample changed; retry";return false;
 }
 int status=portable_realtime_open(&c->reader,c->runtime,PORTABLE_REALTIME_CONTROL,
  PORTABLE_REALTIME_NORMAL_START,c->guard,c->context);
 if(portable_clock_commit_halted(c,status))return false;
 if(status!=PORTABLE_REALTIME_OK){
  if(!portable_clock_commit_cancel_before_seed(c))return false;
  c->message="Native time unavailable - not saved";return false;
 }
 if(c->prepare_seed && (!c->prepare_seed(c->context,&epoch) || epoch!=INT64_C(946684800)+(int64_t)c->permit.requested)){
  if(!portable_clock_commit_live(c))return false;
  int cleanup=portable_realtime_close(&c->reader);
  if(portable_clock_commit_halted(c,cleanup)||cleanup!=PORTABLE_REALTIME_OK)return false;
  if(!portable_clock_commit_cancel_before_seed(c))return false;
  c->message="Time sample expired; retry";return false;
 }
 status=portable_realtime_seed_confirmed(&c->reader,epoch,1000000u,&c->seeded);
 if(portable_clock_commit_halted(c,status))return false;
 int closed=portable_realtime_close(&c->reader);
 if(portable_clock_commit_halted(c,closed)||closed!=PORTABLE_REALTIME_OK)return false;
 if(status!=PORTABLE_REALTIME_OK || !c->seeded.verified){
  if(c->seeded.attempted){c->permit_valid=false;c->message="Time unconfirmed; restart to recover edit";c->retain(c->context);return false;}
  if(!portable_clock_commit_cancel_before_seed(c))return false;
  c->message="Native time unconfirmed";return false;
 }
 if(!portable_clock_commit_finish(c))return false;
 c->message="Native time verified";return true;
}
