#include "PortablePerformance.h"
#include "PortableRealtimeClient.h"
#include "PortableRtcClock.h"
#include <string.h>

enum { CLOSED=0, OPENING=1, OPEN=2, HALTED=3 };
static int halt(portable_realtime_client *c,int reason) {
 c->state=HALTED;c->halt_reason=reason;return reason;
}
static int safe(portable_realtime_client *c) {
 if(!c || c->self!=c)return PORTABLE_REALTIME_CONTEXT;
 if(c->state==HALTED)return c->halt_reason;
 if(!c->guard)return halt(c,PORTABLE_REALTIME_CONTEXT);
 int state=c->guard(c->guard_context);
 if(state==PORTABLE_REALTIME_GUARD_SAFE)return PORTABLE_REALTIME_OK;
 return halt(c,state==PORTABLE_REALTIME_GUARD_RETAINED?
             PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT);
}
static int live(portable_realtime_client *c) {
 int rc=safe(c);if(rc)return rc;
 return c->state==OPEN?PORTABLE_REALTIME_OK:PORTABLE_REALTIME_CONTEXT;
}
static bool grant_empty(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g) && !g->slot && !g->generation && !g->api;
}
static bool grant_valid(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g) && g->slot && g->generation && g->api;
}
static int acquire(portable_realtime_client *c,const char *name,uint32_t version,
                   uint64_t instance,risc_runtime_capability_v1 *g) {
 int rc=safe(c);if(rc)return rc;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=c->acquire(name,version,instance,g);
 rc=safe(c);if(rc)return rc;
 if(!ok) return grant_empty(g)?PORTABLE_REALTIME_UNAVAILABLE:halt(c,PORTABLE_REALTIME_UNCERTAIN);
 return grant_valid(g)?PORTABLE_REALTIME_OK:halt(c,PORTABLE_REALTIME_UNCERTAIN);
}
static int release(portable_realtime_client *c,risc_runtime_capability_v1 *g) {
 int rc=safe(c);if(rc)return rc;
 if(!grant_valid(g))return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 /* Keep the original grant on uncertainty; it is evidence, never a retry. */
 risc_runtime_capability_v1 released=*g;
 bool ok=c->release(&released);
 rc=safe(c);if(rc)return rc;
 if(!ok || !grant_empty(&released))return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 *g=released;return PORTABLE_REALTIME_OK;
}
static int snapshot_status(const risc_realtime_snapshot_v1 *s) {
 if(!s || s->struct_size!=sizeof(*s) || s->reserved ||
    s->monotonic_before_us>s->monotonic_after_us || s->nanoseconds>999999999u ||
    s->nanoseconds%1000u)return PORTABLE_REALTIME_MALFORMED;
 if(s->validity==RISC_REALTIME_UNSET)
  return s->epoch_seconds || s->nanoseconds?PORTABLE_REALTIME_MALFORMED:PORTABLE_REALTIME_UNSET;
 if(s->validity!=RISC_REALTIME_VALID)return PORTABLE_REALTIME_MALFORMED;
 if(s->epoch_seconds<0 || s->epoch_seconds>PORTABLE_REALTIME_MAX_EPOCH)return PORTABLE_REALTIME_RANGE;
 return PORTABLE_REALTIME_OK;
}
static int native_status(portable_realtime_client *c,int32_t status) {
 int rc=safe(c);if(rc)return rc;
 switch(status) {
 case RISC_REALTIME_OK:return PORTABLE_REALTIME_OK;
 case RISC_REALTIME_CONTEXT:return halt(c,PORTABLE_REALTIME_CONTEXT);
 case RISC_REALTIME_INVALID:return PORTABLE_REALTIME_INVALID;
 case RISC_REALTIME_IO:return PORTABLE_REALTIME_IO;
 default:return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 }
}
int portable_realtime_open(portable_realtime_client *c,const risc_runtime_api_v1 *rt,
 int access,int startup,portable_realtime_phase_guard guard,void *context) {
 if(!c || !guard || (access!=PORTABLE_REALTIME_READER && access!=PORTABLE_REALTIME_CONTROL) ||
    (startup!=PORTABLE_REALTIME_TIMER_ONLY && startup!=PORTABLE_REALTIME_NORMAL_START))return PORTABLE_REALTIME_INVALID;
 if(c->self && c->self!=c)return PORTABLE_REALTIME_CONTEXT;
 if(c->state==HALTED)return c->halt_reason;
 if(c->state!=CLOSED)return PORTABLE_REALTIME_CONTEXT;
 *c=(portable_realtime_client){.self=c,.guard=guard,.guard_context=context,
     .state=OPENING,.access=access,.startup=startup};
 int rc=safe(c);if(rc)return rc;
 if(!rt || rt->api_version!=RISC_RUNTIME_API_V1 ||
    rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !rt->acquire || !rt->release) {
  c->state=CLOSED;return PORTABLE_REALTIME_UNAVAILABLE;
 }
 c->acquire=rt->acquire;c->release=rt->release;
 rc=acquire(c,access==PORTABLE_REALTIME_CONTROL?RISC_REALTIME_CONTROL_CAPABILITY:
            RISC_REALTIME_CAPABILITY,RISC_REALTIME_API_V1,0,&c->realtime_grant);
 if(rc){if(c->state!=HALTED)c->state=CLOSED;return rc;}
 /* Check just the common header before reading the rest of the typed table. */
 uint32_t header[2];memcpy(header,c->realtime_grant.api,sizeof(header));
 size_t size=access==PORTABLE_REALTIME_CONTROL?sizeof(risc_realtime_control_api_v1):sizeof(risc_realtime_api_v1);
 if(header[0]!=RISC_REALTIME_API_V1 || header[1]<size)rc=PORTABLE_REALTIME_MALFORMED;
 else if(access==PORTABLE_REALTIME_CONTROL) {
  const risc_realtime_control_api_v1 *control=c->realtime_grant.api;
  if(!control->context || !control->read || !control->seed)rc=PORTABLE_REALTIME_MALFORMED;
  else {c->native_context=control->context;c->native_read=control->read;c->native_seed=control->seed;}
 } else {
  const risc_realtime_api_v1 *api=c->realtime_grant.api;
  if(!api->context || !api->read)rc=PORTABLE_REALTIME_MALFORMED;
  else {c->native_context=api->context;c->native_read=api->read;}
 }
 if(rc) {
  int cleanup=release(c,&c->realtime_grant);
  if(cleanup)return cleanup;
  c->state=CLOSED;return rc;
 }
 c->state=OPEN;return PORTABLE_REALTIME_OK;
}
int portable_realtime_read(portable_realtime_client *c,risc_realtime_snapshot_v1 *out) {
 if(!out)return PORTABLE_REALTIME_INVALID;
 int rc=live(c);if(rc)return rc;
 risc_realtime_snapshot_v1 s={.struct_size=sizeof(s)};
 portable_perf_count(PORTABLE_PERF_NATIVE_READS);
 rc=native_status(c,c->native_read(c->native_context,&s));if(rc)return rc;
 rc=snapshot_status(&s);if(rc<0)return rc;
 *out=s;return rc;
}
int portable_realtime_seed_confirmed(portable_realtime_client *c,int64_t epoch,
 uint32_t budget_us,portable_realtime_seed_result *out) {
 if(!out)return PORTABLE_REALTIME_INVALID;
 *out=(portable_realtime_seed_result){0};
 int rc=live(c);if(rc)return rc;
 if(c->access!=PORTABLE_REALTIME_CONTROL || c->startup!=PORTABLE_REALTIME_NORMAL_START)
  return PORTABLE_REALTIME_DENIED;
 if(epoch<0 || epoch>PORTABLE_REALTIME_MAX_EPOCH || budget_us>5000000u)
  return PORTABLE_REALTIME_RANGE;
 if(c->rtc_grant.api)return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 out->attempted=true;
 rc=native_status(c,c->native_seed(c->native_context,epoch,0));if(rc)return rc;
 out->seeded=true;
 rc=portable_realtime_read(c,&out->snapshot);
 if(rc)return rc==PORTABLE_REALTIME_UNSET?PORTABLE_REALTIME_IO:rc;
 int64_t delta=out->snapshot.epoch_seconds-epoch;
 uint32_t seconds=budget_us/1000000u,ns=(budget_us%1000000u)*1000u;
 if(delta<0 || delta>seconds || (delta==seconds && out->snapshot.nanoseconds>ns))
  return PORTABLE_REALTIME_VERIFY;
 out->verified=true;return PORTABLE_REALTIME_OK;
}
static int interpretation(const portable_realtime_recovery_policy *p,
 const twatch_rtc_time_v1 *calendar,portable_realtime_recovery_result *r) {
 if(calendar->year<PORTABLE_TIMEZONE_MIN_YEAR || calendar->year>PORTABLE_TIMEZONE_MAX_YEAR) {
  r->timezone_status=PORTABLE_TIMEZONE_RANGE;return PORTABLE_REALTIME_RANGE;
 }
 portable_timezone_civil civil={calendar->year,calendar->month,calendar->day,
     calendar->hour,calendar->minute,calendar->second,calendar->weekday};
 r->timezone_status=portable_timezone_interpret_rtc(&p->rule,&civil,p->basis.stores_utc?1u:0u,
     p->basis.reference_epoch,p->fold_choice,&r->interpretation);
 if(r->timezone_status==PORTABLE_TIMEZONE_INVALID)return PORTABLE_REALTIME_MALFORMED;
 if(r->timezone_status==PORTABLE_TIMEZONE_RANGE)return PORTABLE_REALTIME_RANGE;
 if(r->timezone_status==PORTABLE_TIMEZONE_BAD_RULE)return PORTABLE_REALTIME_INVALID;
 r->interpreted=true;
 if(!p->basis.stores_utc) {
  if(r->interpretation.local_status==PORTABLE_TIMEZONE_GAP)return PORTABLE_REALTIME_GAP;
  if(r->interpretation.local_status==PORTABLE_TIMEZONE_FOLD && p->fold_choice<0)return PORTABLE_REALTIME_FOLD;
 }
 if(r->interpretation.epoch<0 || r->interpretation.epoch>PORTABLE_REALTIME_MAX_EPOCH)return PORTABLE_REALTIME_RANGE;
 if(r->timezone_status!=PORTABLE_TIMEZONE_OK)return PORTABLE_REALTIME_UNUSABLE;
 if(r->interpretation.mode_changed && !p->allow_basis_change)return PORTABLE_REALTIME_BASIS_CHOICE;
 return PORTABLE_REALTIME_OK;
}
int portable_realtime_recover_rtc(portable_realtime_client *c,
 const portable_realtime_recovery_policy *p,portable_realtime_recovery_result *out) {
 if(!out)return PORTABLE_REALTIME_INVALID;
 *out=(portable_realtime_recovery_result){0};
 int rc=portable_realtime_read(c,&out->snapshot);out->reason=rc;
 if(rc!=PORTABLE_REALTIME_UNSET)return rc;
 if(c->access!=PORTABLE_REALTIME_CONTROL || c->startup!=PORTABLE_REALTIME_NORMAL_START)
  return out->reason=PORTABLE_REALTIME_DENIED;
 portable_timezone_civil check;
 if(!p || !portable_rtc_basis_valid(&p->basis) || p->fold_choice< -1 || p->fold_choice>1 ||
    portable_timezone_utc_to_local(&p->rule,PORTABLE_TIMEZONE_VALID_EPOCH,&check,0)!=PORTABLE_TIMEZONE_OK)
  return out->reason=PORTABLE_REALTIME_INVALID;
 rc=acquire(c,TWATCH_RTC_CAPABILITY,TWATCH_RTC_API_V1,p->rtc_instance,&c->rtc_grant);
 if(rc) {
  out->reason=rc;
  /* The bool broker cannot distinguish denied RTC from failed activation /
   * rollback with internal retention, even when its output grant is empty.
   * A caller phase guard is not authoritative evidence of clean rollback. */
  if(rc==PORTABLE_REALTIME_UNAVAILABLE)rc=halt(c,PORTABLE_REALTIME_UNCERTAIN);
  out->cleanup=rc;return rc;
 }
 const twatch_rtc_api_v1 *rtc=c->rtc_grant.api;
 uint32_t header[2];memcpy(header,rtc,sizeof(header));
 if(header[0]!=TWATCH_RTC_API_V1 || header[1]<sizeof(*rtc) || !rtc->read)rc=PORTABLE_REALTIME_MALFORMED;
 else {
  rc=safe(c);
  if(!rc) {
   twatch_rtc_time_v1 calendar={0};
   bool ok=rtc->read(rtc->context,&calendar);
   rc=safe(c);
   if(!ok) {
    /* False cannot distinguish ordinary RTC IO from provider-local retained
     * custody. Do not release either grant, even when the local guard is SAFE. */
    out->reason=PORTABLE_REALTIME_IO;
    if(!rc)rc=halt(c,PORTABLE_REALTIME_UNCERTAIN);
    out->cleanup=rc;return rc;
   }
   if(!rc)rc=interpretation(p,&calendar,out);
  }
 }
 out->reason=rc;
 if(c->state==HALTED){out->cleanup=c->halt_reason;return c->halt_reason;}
 int cleanup=release(c,&c->rtc_grant);out->cleanup=cleanup;
 if(cleanup)return cleanup;
 if(rc)return rc;
 rc=live(c);if(rc)return out->reason=rc;
 rc=native_status(c,c->native_seed(c->native_context,out->interpretation.epoch,0));
 if(rc)return out->reason=rc;
 out->seeded=true;
 rc=portable_realtime_read(c,&out->snapshot);
 if(rc==PORTABLE_REALTIME_OK)rc=PORTABLE_REALTIME_RECOVERED;
 else if(rc==PORTABLE_REALTIME_UNSET)rc=PORTABLE_REALTIME_IO;
 return out->reason=rc;
}
int portable_realtime_close(portable_realtime_client *c) {
 if(!c)return PORTABLE_REALTIME_INVALID;
 if(c->self && c->self!=c)return PORTABLE_REALTIME_CONTEXT;
 if(c->state==CLOSED)return PORTABLE_REALTIME_OK;
 int rc=safe(c);if(rc)return rc;
 /* Recovery never leaves a normally owned RTC grant live on return. */
 if(c->rtc_grant.api)return halt(c,PORTABLE_REALTIME_UNCERTAIN);
 rc=release(c,&c->realtime_grant);if(rc)return rc;
 c->native_read=0;c->native_seed=0;c->native_context=0;c->state=CLOSED;
 return PORTABLE_REALTIME_OK;
}
void portable_realtime_stop(portable_realtime_client *c,bool retained) {
 if(!c || c->self!=c || c->state==HALTED)return;
 (void)halt(c,retained?PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT);
}
int portable_realtime_project(const risc_realtime_snapshot_v1 *s,uint32_t elapsed_us,
 uint32_t max_elapsed_us,portable_realtime_estimate *out) {
 if(!out)return PORTABLE_REALTIME_INVALID;
 int rc=snapshot_status(s);if(rc)return rc;
 if(elapsed_us>max_elapsed_us)return PORTABLE_REALTIME_RANGE;
 uint32_t seconds=elapsed_us/1000000u;
 uint32_t fraction=s->nanoseconds+(elapsed_us%1000000u)*1000u;
 /* Two bounded sub-second fractions fit uint32; avoid target libgcc division. */
 if(fraction>=1000000000u){fraction-=1000000000u;++seconds;}
 int64_t epoch=s->epoch_seconds+seconds;
 if(epoch>PORTABLE_REALTIME_MAX_EPOCH)return PORTABLE_REALTIME_RANGE;
 *out=(portable_realtime_estimate){epoch,fraction};
 return PORTABLE_REALTIME_OK;
}
