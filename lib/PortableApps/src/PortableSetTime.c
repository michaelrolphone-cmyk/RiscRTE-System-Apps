#include "PortableSetTime.h"
#include "PortableRtcClock.h"
#include <string.h>
enum { CLOSED=0,OPEN=1,HALTED=2 };
static int halt(portable_set_time_client *c,int reason) {
 c->state=HALTED;c->halt_reason=reason;
 portable_realtime_stop(&c->realtime,reason==PORTABLE_REALTIME_RETAINED);
 return reason;
}
static int safe(portable_set_time_client *c) {
 if(!c || c->self!=c)return PORTABLE_REALTIME_CONTEXT;
 if(c->state==HALTED)return c->halt_reason;
 if(!c->guard)return halt(c,PORTABLE_REALTIME_CONTEXT);
 int phase=c->guard(c->guard_context);
 if(phase==PORTABLE_REALTIME_GUARD_SAFE)return 0;
 return halt(c,phase==PORTABLE_REALTIME_GUARD_RETAINED?PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT);
}
static int native_result(portable_set_time_client *c,int rc) {
 if(rc==PORTABLE_REALTIME_CONTEXT || rc==PORTABLE_REALTIME_UNCERTAIN || rc==PORTABLE_REALTIME_RETAINED)
  return halt(c,rc);
 return rc;
}
static int mapped(int rc) {
 switch(rc) {
 case PORTABLE_TIMEZONE_OK:return 0;
 case PORTABLE_TIMEZONE_GAP:return PORTABLE_REALTIME_GAP;
 case PORTABLE_TIMEZONE_FOLD:return PORTABLE_REALTIME_FOLD;
 case PORTABLE_TIMEZONE_RANGE:return PORTABLE_REALTIME_RANGE;
 default:return PORTABLE_REALTIME_INVALID;
 }
}
int portable_set_time_prepare(const portable_set_time_request *p,portable_set_time_plan *out) {
 if(!p || !out || !portable_rtc_basis_valid(&p->basis) || p->fold_choice< -1 || p->fold_choice>1)
  return PORTABLE_REALTIME_INVALID;
 if(p->native_readback_budget_us>5000000u)return PORTABLE_REALTIME_RANGE;
 portable_timezone_inverse inverse;
 int rc=portable_timezone_local_to_utc(&p->rule,&p->local,&inverse);
 if(rc!=PORTABLE_TIMEZONE_OK && rc!=PORTABLE_TIMEZONE_FOLD)return mapped(rc);
 if(rc==PORTABLE_TIMEZONE_FOLD && p->fold_choice<0)return PORTABLE_REALTIME_FOLD;
 unsigned chosen=rc==PORTABLE_TIMEZONE_FOLD?(unsigned)p->fold_choice:0u;
 int64_t epoch=inverse.candidate[chosen].epoch;
 if(epoch<PORTABLE_RTC_BASIS_MIN_REFERENCE || epoch>PORTABLE_REALTIME_MAX_EPOCH)
  return PORTABLE_REALTIME_RANGE;
 portable_set_time_plan plan={.utc_epoch=epoch,.basis={p->basis.stores_utc,(uint32_t)epoch}};
 rc=p->basis.stores_utc?portable_timezone_epoch_to_civil(epoch,&plan.rtc_calendar):
     portable_timezone_utc_to_local(&p->rule,epoch,&plan.rtc_calendar,0);
 if(rc)return mapped(rc);
 if(plan.rtc_calendar.year<2000 || plan.rtc_calendar.year>2099)return PORTABLE_REALTIME_RANGE;
 *out=plan;return 0;
}
int portable_set_time_open(portable_set_time_client *c,const risc_runtime_api_v1 *rt,
 portable_realtime_phase_guard guard,void *context) {
 if(!c || !guard)return PORTABLE_REALTIME_INVALID;
 if(c->self && c->self!=c)return PORTABLE_REALTIME_CONTEXT;
 if(c->state==HALTED)return c->halt_reason;
 if(c->state!=CLOSED)return PORTABLE_REALTIME_CONTEXT;
 /* Never copy the nested realtime client: it has its own identity check. */
 c->self=c;c->guard=guard;c->guard_context=context;
 int rc=safe(c);if(rc)return rc;
 rc=portable_realtime_open(&c->realtime,rt,PORTABLE_REALTIME_CONTROL,
                          PORTABLE_REALTIME_NORMAL_START,guard,context);
 if(rc)return native_result(c,rc);
 c->acquire=rt->acquire;c->release=rt->release;c->state=OPEN;return 0;
}
static bool grant_valid(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g) && g->slot && g->generation && g->api;
}
static int acquire(portable_set_time_client *c,const char *name,uint32_t version,
 uint64_t instance,risc_runtime_capability_v1 *g) {
 int rc=safe(c);if(rc)return rc;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=c->acquire(name,version,instance,g);
 rc=safe(c);if(rc)return rc;
 /* Even false+empty cannot prove provider activation/rollback was clean. */
 if(!ok || !grant_valid(g))return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 return 0;
}
static int release(portable_set_time_client *c,risc_runtime_capability_v1 *g) {
 int rc=safe(c);if(rc)return rc;
 if(!grant_valid(g))return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 risc_runtime_capability_v1 copy=*g;
 bool ok=c->release(&copy);
 rc=safe(c);if(rc)return rc;
 if(!ok || copy.struct_size!=sizeof(copy) || copy.slot || copy.generation || copy.api)
  return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 *g=copy;return 0;
}
static int failed(portable_set_time_client *c,portable_set_time_result *r,int reason) {
 r->reason=reason;
 if(c && c->self==c && c->state==HALTED)r->cleanup=c->halt_reason;
 return r->cleanup?r->cleanup:reason;
}
static int bool_result(portable_set_time_client *c,bool ok) {
 int rc=safe(c);if(rc)return rc;
 return ok?0:halt(c,PORTABLE_REALTIME_UNCERTAIN);
}
static int kv_result(portable_set_time_client *c,int32_t status) {
 int rc=safe(c);if(rc)return rc;
 if(status==RISC_KEY_VALUE_OK)return 0;
 /* Canonical Runtime KV maps backend failures to typed persistence errors.
  * Its NVS backend closes every handle even for IO after commit. These values
  * do not claim retained ownership; phase and checked release remain mandatory. */
 if(status==RISC_KEY_VALUE_IO)return PORTABLE_REALTIME_IO;
 if(status==RISC_KEY_VALUE_INVALID)return PORTABLE_REALTIME_INVALID;
 if(status==RISC_KEY_VALUE_NOT_FOUND || status==RISC_KEY_VALUE_BUFFER_SMALL)return PORTABLE_REALTIME_VERIFY;
 return halt(c,status==RISC_KEY_VALUE_CONTEXT?PORTABLE_REALTIME_CONTEXT:PORTABLE_REALTIME_UNCERTAIN);
}
static int finish_kv(portable_set_time_client *c,portable_set_time_result *r,int reason) {
 r->reason=reason;
 if(c->state==HALTED)return failed(c,r,reason);
 r->stage=PORTABLE_SET_TIME_KV_RELEASE;r->cleanup=release(c,&c->kv_grant);
 return r->cleanup?r->cleanup:reason;
}
static bool same_calendar(const twatch_rtc_time_v1 *a,const twatch_rtc_time_v1 *b) {
 return a->year==b->year && a->month==b->month && a->day==b->day && a->weekday==b->weekday &&
        a->hour==b->hour && a->minute==b->minute && a->second==b->second;
}
static int finish_rtc(portable_set_time_client *c,portable_set_time_result *r,int reason) {
 r->reason=reason;
 if(c->state==HALTED)return failed(c,r,reason);
 r->stage=PORTABLE_SET_TIME_RTC_RELEASE;r->cleanup=release(c,&c->rtc_grant);
 return r->cleanup?r->cleanup:reason;
}
static int rtc_observed(const portable_set_time_request *p,const portable_set_time_plan *plan,
 const twatch_rtc_time_v1 *intended,const twatch_rtc_time_v1 *observed,portable_set_time_result *r) {
 r->verified_utc_epoch=plan->utc_epoch;
 if(same_calendar(intended,observed))return 0;
 if(plan->utc_epoch==PORTABLE_REALTIME_MAX_EPOCH)return PORTABLE_REALTIME_RANGE;
 /* A hardware calendar tick is plain Gregorian +1, never a DST jump. Require
  * both that calendar tick AND the chosen timezone's UTC+1 forward mapping. */
 int64_t wall_epoch;portable_timezone_civil tick,forward;
 if(portable_timezone_civil_to_epoch(&plan->rtc_calendar,&wall_epoch) ||
    portable_timezone_epoch_to_civil(wall_epoch+1,&tick))return PORTABLE_REALTIME_VERIFY;
 int rc=p->basis.stores_utc?portable_timezone_epoch_to_civil(plan->utc_epoch+1,&forward):
     portable_timezone_utc_to_local(&p->rule,plan->utc_epoch+1,&forward,0);
 if(rc || tick.year<2000 || tick.year>2099)return PORTABLE_REALTIME_VERIFY;
 twatch_rtc_time_v1 one={(uint16_t)tick.year,tick.month,tick.day,tick.weekday,tick.hour,tick.minute,tick.second};
 twatch_rtc_time_v1 mapped={(uint16_t)forward.year,forward.month,forward.day,forward.weekday,forward.hour,forward.minute,forward.second};
 if(!same_calendar(&one,observed) || !same_calendar(&mapped,observed))return PORTABLE_REALTIME_VERIFY;
 r->verified_utc_epoch=plan->utc_epoch+1;r->rtc_tick_accepted=true;return 0;
}
int portable_set_time_apply_confirmed(portable_set_time_client *c,
 const portable_set_time_request *p,portable_set_time_result *r) {
 if(!r)return PORTABLE_REALTIME_INVALID;
 *r=(portable_set_time_result){0};
 int rc=safe(c);if(rc)return failed(c,r,rc);
 if(c->state!=OPEN)return failed(c,r,PORTABLE_REALTIME_CONTEXT);
 rc=portable_set_time_prepare(p,&r->plan);if(rc)return failed(c,r,rc);
 r->planned=true;r->stage=PORTABLE_SET_TIME_RTC_ACQUIRE;
 rc=acquire(c,TWATCH_RTC_CAPABILITY,TWATCH_RTC_API_V1,p->rtc_instance,&c->rtc_grant);
 if(rc)return failed(c,r,rc);
 const twatch_rtc_api_v1 *rtc=c->rtc_grant.api;
 uint32_t header[2];memcpy(header,rtc,sizeof(header));
 if(header[0]!=TWATCH_RTC_API_V1 || header[1]<sizeof(*rtc) || !rtc->read || !rtc->write) {
  (void)halt(c,PORTABLE_REALTIME_UNCERTAIN);return failed(c,r,PORTABLE_REALTIME_MALFORMED);
 }
 const portable_timezone_civil *v=&r->plan.rtc_calendar;
 twatch_rtc_time_v1 intended={(uint16_t)v->year,v->month,v->day,v->weekday,v->hour,v->minute,v->second};
 r->stage=PORTABLE_SET_TIME_RTC_BEFORE;
 rc=safe(c);if(rc)return failed(c,r,rc);
 risc_realtime_snapshot_v1 before,after;
 rc=portable_realtime_read(&c->realtime,&before);
 if(rc<0)return finish_rtc(c,r,native_result(c,rc));
 r->stage=PORTABLE_SET_TIME_RTC_WRITE;
 rc=safe(c);if(rc)return failed(c,r,rc);
 r->rtc_outcome=PORTABLE_SET_TIME_UNCONFIRMED;
 bool ok=rtc->write(rtc->context,&intended);
 rc=bool_result(c,ok);if(rc)return failed(c,r,ok?rc:PORTABLE_REALTIME_IO);
 r->rtc_write_accepted=true;r->stage=PORTABLE_SET_TIME_RTC_VERIFY;
 rc=safe(c);if(rc)return failed(c,r,rc);
 twatch_rtc_time_v1 observed={0};ok=rtc->read(rtc->context,&observed);
 rc=bool_result(c,ok);if(rc)return failed(c,r,ok?rc:PORTABLE_REALTIME_IO);
 r->stage=PORTABLE_SET_TIME_RTC_AFTER;
 rc=safe(c);if(rc)return failed(c,r,rc);
 rc=portable_realtime_read(&c->realtime,&after);
 if(rc<0)return finish_rtc(c,r,native_result(c,rc));
 if(after.monotonic_before_us<before.monotonic_after_us)rc=PORTABLE_REALTIME_VERIFY;
 else {
  r->rtc_window_us=after.monotonic_after_us-before.monotonic_before_us;
  r->rtc_window_confirmed=r->rtc_window_us<=250000u;
  rc=r->rtc_window_confirmed?rtc_observed(p,&r->plan,&intended,&observed,r):PORTABLE_REALTIME_VERIFY;
 }
 if(!rc)r->rtc_outcome=PORTABLE_SET_TIME_CONFIRMED;
 rc=finish_rtc(c,r,rc);if(rc)return rc;
 r->stage=PORTABLE_SET_TIME_NATIVE;
 rc=safe(c);if(rc)return failed(c,r,rc);
 rc=portable_realtime_seed_confirmed(&c->realtime,r->verified_utc_epoch,p->native_readback_budget_us,&r->native);
 if(rc)return failed(c,r,native_result(c,rc));
 r->stage=PORTABLE_SET_TIME_KV_ACQUIRE;
 rc=acquire(c,RISC_KEY_VALUE_CAPABILITY,RISC_KEY_VALUE_API_V1,PORTABLE_RTC_BASIS_INSTANCE,&c->kv_grant);
 if(rc)return failed(c,r,rc);
 const risc_key_value_v1 *kv=c->kv_grant.api;
 memcpy(header,kv,sizeof(header));
 if(header[0]!=RISC_KEY_VALUE_API_V1 || header[1]<sizeof(*kv) || !kv->get || !kv->put) {
  (void)halt(c,PORTABLE_REALTIME_UNCERTAIN);return failed(c,r,PORTABLE_REALTIME_MALFORMED);
 }
 uint8_t bytes[PORTABLE_RTC_BASIS_BYTES]={0x52,0x54,1,(uint8_t)(r->plan.basis.stores_utc?1:0)};
 portable_rtc_basis_put_word(bytes+4,(uint32_t)r->verified_utc_epoch);
 portable_rtc_basis_put_word(bytes+8,portable_rtc_basis_check(bytes));
 r->stage=PORTABLE_SET_TIME_KV_WRITE;
 rc=safe(c);if(rc)return failed(c,r,rc);
 r->metadata_outcome=PORTABLE_SET_TIME_UNCONFIRMED;
 int32_t status=kv->put(kv->context,PORTABLE_RTC_BASIS_KEY,bytes,sizeof(bytes));
 rc=kv_result(c,status);if(rc)return finish_kv(c,r,status==RISC_KEY_VALUE_IO?PORTABLE_REALTIME_IO:rc);
 r->metadata_write_accepted=true;r->stage=PORTABLE_SET_TIME_KV_VERIFY;
 rc=safe(c);if(rc)return failed(c,r,rc);
 uint8_t readback[PORTABLE_RTC_BASIS_BYTES]={0};uint32_t size=0;
 status=kv->get(kv->context,PORTABLE_RTC_BASIS_KEY,readback,sizeof(readback),&size);
 rc=kv_result(c,status);if(rc)return finish_kv(c,r,status==RISC_KEY_VALUE_IO?PORTABLE_REALTIME_IO:rc);
 bool matches=size==sizeof(bytes);
 for(unsigned i=0;i<sizeof(bytes);++i)if(bytes[i]!=readback[i])matches=false;
 rc=matches?0:PORTABLE_REALTIME_VERIFY;
 if(matches)r->metadata_outcome=PORTABLE_SET_TIME_CONFIRMED;
 rc=finish_kv(c,r,rc);
 if(!rc)r->stage=PORTABLE_SET_TIME_DONE;
 return rc;
}
int portable_set_time_close(portable_set_time_client *c) {
 if(!c)return PORTABLE_REALTIME_INVALID;
 if(c->self && c->self!=c)return PORTABLE_REALTIME_CONTEXT;
 if(c->state==CLOSED)return 0;
 int rc=safe(c);if(rc)return rc;
 if(c->rtc_grant.api || c->kv_grant.api)return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 rc=portable_realtime_close(&c->realtime);if(rc)return native_result(c,rc);
 c->state=CLOSED;return 0;
}
void portable_set_time_stop(portable_set_time_client *c,bool retained) {
 if(!c || c->self!=c || c->state==HALTED)return;
 (void)halt(c,retained?PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT);
}
