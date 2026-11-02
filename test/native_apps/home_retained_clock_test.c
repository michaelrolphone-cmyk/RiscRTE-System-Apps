#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "PortableHomeRetainedClock.h"
static jmp_buf terminal;
typedef struct {
 int fault,at,kind;bool retained,held,refuse_enter,cross;
 unsigned samples,scenes,seeds,presents,stages,prepares,resumes,enters,invalidates,refreshes;
 uint8_t staged[PORTABLE_DESK_CLOCK_RECORD_BYTES];
 int64_t epoch;
} fixture;
static int point(fixture *s){assert(!s->retained);++s->at;if(s->at!=s->fault)return 1;if(s->kind==2){s->retained=true;return -2;}return 0;}
static bool ready(void*p){return !((fixture*)p)->retained;}
static int cancelled(void*p){fixture*s=p;int r=point(s);return r==1?0:1;}
static int sample(void*p,risc_realtime_snapshot_v1*out){fixture*s=p;assert(!s->held);++s->samples;int r=point(s);if(r!=1)return PORTABLE_REALTIME_IO;*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.epoch_seconds=s->epoch,.validity=RISC_REALTIME_VALID};return PORTABLE_REALTIME_OK;}
static bool project(void*p,const risc_realtime_snapshot_v1*in,portable_realtime_estimate*out,uint32_t*at){fixture*s=p;if(point(s)!=1)return false;*out=(portable_realtime_estimate){in->epoch_seconds,0};if(s->cross&&s->held){out->epoch_seconds+=60;s->epoch+=60;s->cross=false;}*at=100;return true;}
static bool scene(void*p,const portable_desk_config*c,int64_t minute){fixture*s=p;assert(!s->held);assert(portable_desk_config_valid(c));assert(minute%60==0);++s->scenes;return point(s)==1;}
static int seed(void*p){fixture*s=p;assert(!s->held);++s->seeds;return point(s);}
static bool present(void*p,bool full){fixture*s=p;assert(!s->held);(void)full;++s->presents;return point(s)==1;}
static int refresh(void*p,portable_desk_config*c,int64_t now,bool rewind){fixture*s=p;assert(!s->held);(void)c;(void)now;(void)rewind;++s->refreshes;int r=point(s);return r==1?0:-2;}
static void held(void*p,bool value){fixture*s=p;assert(!s->retained);s->held=value;}
static void invalidate(void*p){fixture*s=p;assert(!s->retained&&!s->held);++s->invalidates;}
static void retain(void*p){((fixture*)p)->retained=true;}
static int stage(void*p,const portable_desk_record*r){fixture*s=p;assert(!s->held);++s->stages;int status=point(s);if(status==1)assert(portable_desk_encode(r,s->staged,sizeof(s->staged)));return status;}
static int prepare(void*p){fixture*s=p;assert(s->held);++s->prepares;return point(s);}
static int resume(void*p){fixture*s=p;assert(s->held);++s->resumes;return point(s);}
static int enter(void*p,const portable_desk_record*r,uint32_t duration,uint32_t at){fixture*s=p;assert(s->held);assert(at==100&&duration>0&&duration<=60000);uint8_t bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];assert(portable_desk_encode(r,bytes,sizeof(bytes)));assert(!memcmp(bytes,s->staged,sizeof(bytes)));++s->enters;int status=point(s);if(status!=1)return status;if(s->refuse_enter)return 0;longjmp(terminal,1);}
static fixture state;
static int run(bool timer,int fault,int kind,bool cross,bool refusal){
 state=(fixture){.epoch=INT64_C(1704067237),.fault=fault,.kind=kind,.cross=cross,.refuse_enter=refusal};
 portable_desk_config config={.face=0,.time_format=1};strcpy(config.time_zone,"UTC");
 portable_desk_record record={.config=config,.displayed_minute=INT64_C(1704067200),.refresh_modulo=1,.has_image=true};
 uint8_t before[PORTABLE_DESK_CLOCK_RECORD_BYTES];assert(portable_desk_encode(&record,before,sizeof(before)));
 portable_desk_record copy;assert(portable_desk_decode(before,sizeof(before),&copy));
 uint8_t after[PORTABLE_DESK_CLOCK_RECORD_BYTES];assert(portable_desk_encode(&copy,after,sizeof(after)));assert(!memcmp(before,after,sizeof(before)));
 portable_home_retained_hooks hooks={&state,ready,cancelled,sample,project,scene,seed,present,refresh,held,invalidate,retain};
 portable_desk_sleep_ops ops={&state,cancelled,stage,prepare,resume,enter};
 if(setjmp(terminal)){assert(state.enters==1&&!state.retained&&state.held);return 1;}
 int result=portable_home_retained_run(&config,&record,timer,&hooks,&ops);
 assert((result==-2)==state.retained);assert(result==0||result==-2);
 if(state.retained)assert(state.invalidates==0 || state.cross==false);
 return result;
}
int main(void){
 assert(PORTABLE_DESK_CLOCK_RECORD_BYTES==408);
 assert(run(true,0,0,false,false)==1);assert(state.seeds==1&&state.refreshes==1&&state.stages==1);int operations=state.at;
 assert(run(false,0,0,false,false)==1);assert(state.seeds==0&&state.refreshes==0);
 assert(run(true,0,0,false,true)==0);assert(state.resumes==1&&!state.held);
 assert(run(true,0,0,true,false)==1);assert(state.resumes==1&&state.presents==2);
 for(int kind=1;kind<=2;kind++)for(int fault=1;fault<=operations;fault++)(void)run(true,fault,kind,false,false);
 printf("shared retained controller: 408-byte roundtrip, timer/foreground, crossing, refusal and %d injected custody paths PASS\n",operations*2);
 return 0;
}
