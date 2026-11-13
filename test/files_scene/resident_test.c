#include <assert.h>
#include <stdio.h>
#include "../../Apps/files_scene_resident.h"
static files_resident resident;
static bool live,dirty,replace,absent,clear_absent;
static unsigned retains,calls,settles,observations;
static int status,clean,observe_status;
static uint32_t flags,pending,last_reason;
static bool alive(void*p){(void)p;return live;}
static void retain(void*p){(void)p;retains++;live=false;}
static int settle(void*p){(void)p;settles++;return clean;}
static int checkpoint(uint64_t i,const risc_resident_request_v1*q,risc_resident_reply_v1*r){assert(i==7&&q->struct_size==sizeof(*q)&&!q->flags&&!q->reserved);assert(q->reason!=RISC_RESIDENT_CHECKPOINT_POLL&&q->reason!=RISC_RESIDENT_CHECKPOINT_POLICY);last_reason=q->reason;calls++;r->flags=flags;if(dirty)r->struct_size++;if(replace)live=false;return status;}
static int attention(uint64_t i,risc_resident_attention_v1*out){assert(i==7);observations++;if(observe_status==0){out->supported_flags=7;out->pending_flags=pending;}if(dirty)out->struct_size++;return observe_status;}
static int reg(uint64_t i,const risc_resident_attention_callbacks_v1*c){(void)i;(void)c;return 0;}
static int reject(uint64_t i,uint32_t b){(void)i;(void)b;assert(0);return 0;}
static bool bind(risc_resident_client_v1*c){if(absent){if(clear_absent)memset(c,0,c->struct_size);if(dirty)c->invocation=7;return false;}risc_resident_attention_client_v1*a=(void*)c;*a=(risc_resident_attention_client_v1){.base={.api_version=1,.struct_size=sizeof(*a),.invocation=7,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint},.attention_tag=RISC_RESIDENT_ATTENTION_TAG,.attention_version=1,.register_attention=reg,.attention=attention,.reject_manual=reject};return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.resident_shell=bind};
static void reset(void){live=true;dirty=replace=absent=clear_absent=false;retains=calls=settles=observations=0;status=observe_status=0;clean=1;flags=pending=last_reason=0;assert(files_resident_init(&resident,&rt,alive,retain,settle,NULL));}
int main(void){
 reset();assert(files_resident_step(&resident,true,false,true)==0&&last_reason==RISC_RESIDENT_QUERY_ALARM_ONLY);assert(files_resident_step(&resident,true,false,true)==0&&last_reason==RISC_RESIDENT_QUERY_IDLE_ONLY);assert(files_resident_step(&resident,true,false,true)==0&&last_reason==RISC_RESIDENT_APPLY_IDLE_ONLY);status=RISC_RESIDENT_BUSY;assert(files_resident_step(&resident,true,false,true)==0&&last_reason==RISC_RESIDENT_QUERY_LAST_IDLE_ALARM);status=0;flags=RISC_RESIDENT_REPLY_REDRAW;assert(files_resident_step(&resident,true,false,true)==1&&resident.alarm_cancelled_input&&resident.idle_completed);
 reset();pending=RISC_RESIDENT_ATTENTION_ALARM;assert(files_resident_capture(&resident,10)&&observations==1);assert(files_resident_capture(&resident,25)&&observations==1);assert(files_resident_capture(&resident,26)&&observations==2);assert(files_resident_step(&resident,true,true,false)==0&&!calls);assert(files_resident_step(&resident,false,false,false)==0&&!calls);clean=0;assert(files_resident_step(&resident,true,false,false)==0&&!calls);clean=1;assert(files_resident_step(&resident,true,false,false)==0&&last_reason==RISC_RESIDENT_QUERY_ALARM_ONLY);assert(files_resident_step(&resident,true,false,false)==1&&last_reason==RISC_RESIDENT_CHECKPOINT_ALARM&&!resident.pending_attention);
 reset();assert(files_resident_manual(&resident,RISC_RESIDENT_CHECKPOINT_CONTROLS));status=RISC_RESIDENT_BUSY;assert(files_resident_step(&resident,true,false,false)==0&&resident.phase==FR_MANUAL);status=RISC_RESIDENT_HANDOFF;assert(files_resident_step(&resident,true,false,false)==1&&resident.exit_requested);
 reset();dirty=true;assert(files_resident_manual(&resident,RISC_RESIDENT_CHECKPOINT_EXIT));assert(files_resident_step(&resident,true,false,false)==-1&&retains==1);unsigned prior=calls;assert(files_resident_step(&resident,true,false,false)==-1&&calls==prior);
 reset();replace=true;assert(files_resident_manual(&resident,RISC_RESIDENT_CHECKPOINT_EXIT));assert(files_resident_step(&resident,true,false,false)==-1&&!live);
 reset();flags=RISC_RESIDENT_REPLY_REDRAW;assert(files_resident_step(&resident,true,false,true)==-1&&retains==1);
 reset();absent=true;assert(files_resident_init(&resident,&rt,alive,retain,settle,NULL)&&!resident.enabled);dirty=true;assert(!files_resident_init(&resident,&rt,alive,retain,settle,NULL)&&retains==1);
 reset();status=RISC_RESIDENT_BUSY;assert(files_resident_step(&resident,true,false,true)==0&&resident.phase==FR_QUERY_IDLE);assert(files_resident_step(&resident,true,false,true)==1&&resident.unsupported&&!resident.phase);
 reset();assert(!files_resident_manual(&resident,RISC_RESIDENT_CHECKPOINT_POLL)&&retains==1);
 reset();absent=clear_absent=true;assert(files_resident_init(&resident,&rt,alive,retain,settle,NULL)&&!resident.enabled&&!retains);dirty=true;assert(!files_resident_init(&resident,&rt,alive,retain,settle,NULL)&&retains==1);
 puts("10 Files resident contract groups PASS (endpoint doubles)");
}
