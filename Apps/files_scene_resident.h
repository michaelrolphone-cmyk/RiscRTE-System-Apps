#pragma once
#include "RiscRuntimeV1.h"
#include "RiscResidentAttentionV1.h"
#include "RiscResidentIdleV1.h"
#include "RiscResidentAlarmV1.h"
#include <string.h>
typedef struct {
 const risc_runtime_api_v1 *runtime;
 bool (*alive)(void*);void (*retain)(void*);int (*settle)(void*);void *owner;
 risc_resident_attention_client_v1 client;
 bool enabled,attention_available,terminal,busy,exit_requested,alarm_supported,alarm_negotiated;
 uint32_t phase,manual_reason,reply_flags,last_observed,pending_attention;
 bool observed,alarm_cancelled_input,idle_completed,unsupported;
} files_resident;
enum {FR_NONE,FR_QUERY_ALARM,FR_APPLY_ALARM,FR_QUERY_IDLE,FR_APPLY_IDLE,FR_IDLE_OUTCOME,FR_MANUAL,FR_IDLE_ALARM_QUERY};
static bool files_resident_live(files_resident*r){
 if(r->terminal)return false;
 if(!r->alive||!r->alive(r->owner)){r->terminal=true;return false;}return true;
}
static bool files_resident_fail(files_resident*r){if(!r->terminal){r->terminal=true;if(r->retain)r->retain(r->owner);}return false;}
static int files_resident_call(files_resident*r,uint32_t reason,uint32_t *flags){
 if(!files_resident_live(r)||!r->enabled)return RISC_RESIDENT_RETAINED;
 if(r->busy){files_resident_fail(r);return RISC_RESIDENT_RETAINED;}r->busy=true;
 risc_resident_request_v1 q={sizeof(q),reason,0,0};risc_resident_reply_v1 reply={sizeof(reply),0};
 int rc=r->client.base.checkpoint(r->client.base.invocation,&q,&reply);
 r->busy=false;if(!files_resident_live(r))return RISC_RESIDENT_RETAINED;
 if(reply.struct_size!=sizeof(reply)||(reply.flags&~(RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED))||
 (rc!=RISC_RESIDENT_OK&&reply.flags)||
 (rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF)){
  files_resident_fail(r);return RISC_RESIDENT_RETAINED;
 }
 *flags=reply.flags;return rc;
}
static bool files_resident_init(files_resident*r,const risc_runtime_api_v1*rt,
 bool(*alive)(void*),void(*retain)(void*),int(*settle)(void*),void*owner){
 memset(r,0,sizeof(*r));r->runtime=rt;r->alive=alive;r->retain=retain;r->settle=settle;r->owner=owner;
 if(!files_resident_live(r))return false;
 if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!rt->resident_shell)return true;
 r->client.base.struct_size=sizeof(r->client);r->busy=true;
 bool ok=rt->resident_shell(&r->client.base);r->busy=false;
 if(!files_resident_live(r))return false;
 if(!ok){risc_resident_attention_client_v1 empty={.base={.struct_size=sizeof(empty)}};
  /* Runtime 0052 clears the entire advertised capacity on clean absence;
   * older optional implementations may leave the caller's size untouched. */
  if(r->client.base.struct_size==0)empty.base.struct_size=0;
  if(memcmp(&empty,&r->client,sizeof(empty)))return files_resident_fail(r);
  return true;}
 risc_resident_client_v1*c=&r->client.base;
 if(c->api_version!=1||c->struct_size<sizeof(*c)||!c->invocation||c->reserved||c->role!=RISC_RESIDENT_ROLE_FOREGROUND||!c->checkpoint)return files_resident_fail(r);
 r->enabled=true;r->attention_available=risc_resident_attention_available_v1(&r->client);return true;
}
/* Capture-safe observation only. No POLL, policy, settings, or focus work. */
static bool files_resident_capture(files_resident*r,uint32_t now){
 if(!files_resident_live(r))return false;
 if(!r->enabled||!r->attention_available)return true;
 if(r->observed&&(uint32_t)(now-r->last_observed)<16)return true;
 r->observed=true;r->last_observed=now;
 if(r->busy)return files_resident_fail(r);
 risc_resident_attention_v1 out={.struct_size=sizeof(out)},empty=out;r->busy=true;
 int rc=r->client.attention(r->client.base.invocation,&out);r->busy=false;
 if(!files_resident_live(r))return false;
 if(rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_INCOMPATIBLE){if(memcmp(&out,&empty,sizeof(out)))return files_resident_fail(r);return true;}
 if(rc!=RISC_RESIDENT_OK||out.struct_size!=sizeof(out)||(out.supported_flags&~RISC_RESIDENT_ATTENTION_ALL)||(out.pending_flags&~out.supported_flags))return files_resident_fail(r);
 r->pending_attention|=out.pending_flags;return true;
}
static bool files_resident_manual(files_resident*r,uint32_t reason){
 if(!files_resident_live(r)||!r->enabled)return false;
 if(reason!=RISC_RESIDENT_CHECKPOINT_CONTROLS&&reason!=RISC_RESIDENT_CHECKPOINT_EXIT)return files_resident_fail(r);
 if(r->phase)return false;
 r->manual_reason=reason;r->phase=FR_MANUAL;return true;
}
/* 0 pending/no work, 1 completed boundary, -1 retained. Inhibition keeps the
 * exact pending reason, including when sharing owns the network. */
static int files_resident_step(files_resident*r,bool settled,bool inhibited,bool idle_due){
 if(!files_resident_live(r))return -1;
 if(!r->enabled)return 0;
 if(r->busy){files_resident_fail(r);return -1;}
 if(!r->phase){
  r->reply_flags=0;r->alarm_cancelled_input=r->idle_completed=r->unsupported=false;
  if(r->pending_attention&RISC_RESIDENT_ATTENTION_EXIT){r->manual_reason=RISC_RESIDENT_CHECKPOINT_EXIT;r->phase=FR_MANUAL;}
  else if(r->pending_attention&RISC_RESIDENT_ATTENTION_CONTROLS){r->manual_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;r->phase=FR_MANUAL;}
  else if(r->pending_attention&RISC_RESIDENT_ATTENTION_ALARM)r->phase=FR_QUERY_ALARM;
  else if(idle_due)r->phase=r->alarm_negotiated?FR_QUERY_IDLE:FR_IDLE_ALARM_QUERY;
 }
 if(!r->phase||inhibited||!settled)return 0;
 r->busy=true;int clean=r->settle(r->owner);r->busy=false;
 if(!files_resident_live(r))return -1;
 if(!clean)return 0;
 if(clean!=1){files_resident_fail(r);return -1;}
 uint32_t reason=(r->phase==FR_QUERY_ALARM||r->phase==FR_IDLE_ALARM_QUERY)?RISC_RESIDENT_QUERY_ALARM_ONLY:
 r->phase==FR_APPLY_ALARM?RISC_RESIDENT_CHECKPOINT_ALARM:
 r->phase==FR_QUERY_IDLE?RISC_RESIDENT_QUERY_IDLE_ONLY:
 r->phase==FR_APPLY_IDLE?RISC_RESIDENT_APPLY_IDLE_ONLY:
 r->phase==FR_IDLE_OUTCOME?RISC_RESIDENT_QUERY_LAST_IDLE_ALARM:r->manual_reason;
 uint32_t flags=0;int rc=files_resident_call(r,reason,&flags);
 if(rc==RISC_RESIDENT_RETAINED)return -1;
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){
  if(r->phase==FR_QUERY_ALARM||r->phase==FR_IDLE_ALARM_QUERY||r->phase==FR_QUERY_IDLE||r->phase==FR_IDLE_OUTCOME){files_resident_fail(r);return -1;}
  r->exit_requested=true;r->phase=0;return 1;
 }
 if(rc==RISC_RESIDENT_BUSY){
  if(r->phase==FR_IDLE_ALARM_QUERY){r->alarm_negotiated=true;r->phase=FR_QUERY_IDLE;return 0;}
  if(r->phase==FR_QUERY_ALARM||r->phase==FR_QUERY_IDLE){
   if(r->phase==FR_QUERY_ALARM)r->pending_attention&=~RISC_RESIDENT_ATTENTION_ALARM;
   r->unsupported=true;r->phase=0;return 1;
  }
  return 0;
 }
 if((r->phase==FR_QUERY_ALARM||r->phase==FR_IDLE_ALARM_QUERY||r->phase==FR_QUERY_IDLE)&&flags){files_resident_fail(r);return -1;}
 r->reply_flags|=flags;
 switch(r->phase){
 case FR_IDLE_ALARM_QUERY:r->alarm_negotiated=r->alarm_supported=true;r->phase=FR_QUERY_IDLE;return 0;
 case FR_QUERY_ALARM:r->alarm_negotiated=r->alarm_supported=true;r->phase=FR_APPLY_ALARM;return 0;
 case FR_APPLY_ALARM:r->pending_attention&=~RISC_RESIDENT_ATTENTION_ALARM;break;
 case FR_QUERY_IDLE:r->phase=FR_APPLY_IDLE;return 0;
 case FR_APPLY_IDLE:r->idle_completed=true;if(r->alarm_supported){r->phase=FR_IDLE_OUTCOME;return 0;}break;
 case FR_IDLE_OUTCOME:if(flags&~RISC_RESIDENT_REPLY_REDRAW){files_resident_fail(r);return -1;}r->alarm_cancelled_input=!!(flags&RISC_RESIDENT_REPLY_REDRAW);break;
 case FR_MANUAL:r->pending_attention&=~(r->manual_reason==RISC_RESIDENT_CHECKPOINT_EXIT?RISC_RESIDENT_ATTENTION_EXIT:RISC_RESIDENT_ATTENTION_CONTROLS);break;
 default:files_resident_fail(r);return -1;
 }
 r->phase=0;return 1;
}
