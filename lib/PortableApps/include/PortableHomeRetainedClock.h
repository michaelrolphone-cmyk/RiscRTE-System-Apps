#pragma once
/* Shared application-owned retained-clock cycle. Hardware custody and raster
 * operations are borrowed hooks, never device identities or a second app.
 * The existing DCLK schemas/bytes are unchanged. Hooks must be owner-serialized;
 * ready is a pure custody observation; cancelled may make only bounded
 * owned wake-input observations allowed at the preparation boundary. After a retained result no
 * hook except retain may run. The caller classifies TIMER before starting UI. */
#include "PortableDeskClockApp.h"
#include "PortableRealtimeClient.h"
typedef struct {
 void *context;
 bool (*ready)(void *);
 int (*cancelled)(void *);
 int (*sample)(void *,risc_realtime_snapshot_v1 *);
 bool (*project)(void *,const risc_realtime_snapshot_v1 *,portable_realtime_estimate *,uint32_t *);
 bool (*scene)(void *,const portable_desk_config *,int64_t);
 int (*seed)(void *);
 bool (*present)(void *,bool);
 /* Optional copied-catalog refresh: negative is retained, zero unchanged,
  * positive replaced. This is application data policy, not provider I/O. */
 int (*refresh)(void *,portable_desk_config *,int64_t,bool);
 void (*held)(void *,bool);
 void (*invalidate)(void *);
 void (*retain)(void *);
} portable_home_retained_hooks;
static inline int portable_home_retained_fence(const portable_home_retained_hooks *h) {
 h->retain(h->context);return -2;
}
static inline int portable_home_retained_cancelled(const portable_home_retained_hooks *h) {
 if(!h->ready(h->context))return portable_home_retained_fence(h);
 int rc=h->cancelled(h->context);
 if((rc!=0&&rc!=1)||!h->ready(h->context))return portable_home_retained_fence(h);
 return rc;
}
static inline int portable_home_retained_restore(const portable_home_retained_hooks *h,
                                                const portable_desk_sleep_ops *ops) {
 if(!h->ready(h->context))return portable_home_retained_fence(h);
 if(ops->resume(ops->context)!=1)return portable_home_retained_fence(h);
 h->held(h->context,false);h->invalidate(h->context);return 0;
}
/* Eligibility/configuration capture are settled foreground decisions made by
 * the caller. A classified TIMER uses the copied record, never live preferences.
 * Success enters terminal sleep and cannot return. 0 is a clean return to Home;
 * -2 retains the invocation and forbids cleanup, retries, or another provider. */
static inline int portable_home_retained_run(const portable_desk_config *configuration,
 const portable_desk_record *record,bool timer,const portable_home_retained_hooks *h,
 const portable_desk_sleep_ops *ops) {
 if(!configuration||!record||!h||!ops||!h->ready||!h->cancelled||!h->sample||
    !h->project||!h->scene||!h->seed||!h->present||!h->held||!h->invalidate||
    !h->retain||!ops->cancelled||!ops->stage||!ops->prepare||!ops->resume||!ops->stage_enter)return 0;
 portable_desk_config config=timer?record->config:*configuration;
 portable_desk_cycle cycle;
 if(!portable_desk_begin(&cycle,&config,record,timer))return 0;
 for(unsigned attempt=0;attempt<PORTABLE_DESK_CLOCK_MAX_PRESENTS;attempt++) {
  risc_realtime_snapshot_v1 sample;portable_desk_frame frame;
  int stopped=portable_home_retained_cancelled(h);if(stopped<0)return -2;
  if(stopped||h->sample(h->context,&sample)!=PORTABLE_REALTIME_OK)break;
#ifdef PORTABLE_DESK_POINTS_FACE
  if(timer&&h->refresh) {
   int refreshed=h->refresh(h->context,&config,sample.epoch_seconds,
       cycle.current.has_image&&sample.epoch_seconds<cycle.current.displayed_minute);
   if(refreshed<0)return portable_home_retained_fence(h);
   if(refreshed) {
    stopped=portable_home_retained_cancelled(h);if(stopped<0)return -2;
    if(stopped||h->sample(h->context,&sample)!=PORTABLE_REALTIME_OK)break;
    if(config.points.status==PORTABLE_DESK_POINTS_READY&&
       (sample.epoch_seconds<INT64_C(946684800)+config.points.view.seconds||
        sample.epoch_seconds>=INT64_C(946684800)+config.points.view.valid_until))
     config.points=(portable_desk_points_snapshot){.status=PORTABLE_DESK_POINTS_ERROR};
    cycle.current.config=config;cycle.full=true;
   }
  }
#endif
  if(!portable_desk_plan_frame(&cycle,sample.epoch_seconds,&frame))break;
  if(frame.has_previous) {
   if(!h->scene(h->context,&cycle.current.config,frame.previous_minute))break;
   int seeded=h->seed(h->context);
   if(seeded!=0&&seeded!=1)return portable_home_retained_fence(h);
   if(!seeded)frame.full=true;
  }
  if(!h->scene(h->context,&config,frame.minute))break;
  if(!portable_desk_presented(&cycle,h->present(h->context,frame.full)))
   return portable_home_retained_fence(h);
  stopped=portable_home_retained_cancelled(h);if(stopped<0)return -2;
  if(stopped||h->sample(h->context,&sample)!=PORTABLE_REALTIME_OK)break;
  portable_desk_record proposal;uint32_t duration=0,sampled_at=0;
  portable_realtime_estimate now;
  if(!h->project(h->context,&sample,&now,&sampled_at))break;
  int plan=portable_desk_prepare_record(&cycle,now.epoch_seconds,(int32_t)(now.nanoseconds/1000u),&proposal,&duration);
  if(plan==PORTABLE_DESK_REPAINT)continue;
  if(plan!=PORTABLE_DESK_READY)break;
  int staged=ops->stage(ops->context,&proposal);
  if(staged!=0&&staged!=1)return portable_home_retained_fence(h);
  if(staged!=1)break;
  /* No time/provider acquisition, KV, or yield once physical holds begin. */
  h->held(h->context,true);
  int prepared=ops->prepare(ops->context);
  if(prepared!=0&&prepared!=1)return portable_home_retained_fence(h);
  if(prepared!=1){h->held(h->context,false);break;}
  stopped=portable_home_retained_cancelled(h);if(stopped<0)return -2;
  if(stopped||!h->project(h->context,&sample,&now,&sampled_at))
   return portable_home_retained_restore(h,ops);
  plan=portable_desk_prepare_record(&cycle,now.epoch_seconds,(int32_t)(now.nanoseconds/1000u),&proposal,&duration);
  stopped=plan==PORTABLE_DESK_READY?portable_home_retained_cancelled(h):0;
  if(stopped<0)return -2;
  if(plan==PORTABLE_DESK_READY&&!stopped) {
   if(ops->stage_enter(ops->context,&proposal,duration,sampled_at)!=0)
    return portable_home_retained_fence(h);
   return portable_home_retained_restore(h,ops);
  }
  if(portable_home_retained_restore(h,ops)==-2)return -2;
  if(plan!=PORTABLE_DESK_REPAINT)break;
  cycle.full=true;
 }
 if(!h->ready(h->context))return portable_home_retained_fence(h);
 h->invalidate(h->context);return 0;
}
