#include <assert.h>
#include <stdio.h>
#include "../../Apps/files_scene_effects.h"
static files_effects s;
static unsigned calls,applies,progresses,done_count,retains,outcome,mode;
static bool live;static uint32_t pending;
static bool alive(void*c){(void)c;return live;}
static void retain(void*c){(void)c;++retains;}
static int settle(void*c){(void)c;return mode==1?0:1;}
static bool done(void*c,unsigned effect,unsigned result){(void)c;assert(effect==1);++done_count;if(result!=3)pending=0;return true;}
static int call(void*c,uint32_t reason){
 (void)c;++calls;
 if(mode==4){live=false;return 0;}
 if(mode==5){assert(files_effects_step(&s,pending,true)==-1);return 0;}
 if(reason==RISC_SETTINGS_QUERY(1))return mode==2?RISC_RESIDENT_BUSY:0;
 if(reason==RISC_SETTINGS_APPLY(1)){++applies;return 0;}
 if(reason==RISC_SETTINGS_PROGRESS(1)){++progresses;if(mode==6)return RISC_RESIDENT_BUSY;outcome=1;return 0;}
 for(unsigned i=1;i<=3;i++)if(reason==RISC_SETTINGS_RESULT(1,i))return mode!=3&&i==outcome?0:RISC_RESIDENT_BUSY;
 assert(0);return -1;
}
static void reset(unsigned m,unsigned result){calls=applies=progresses=done_count=retains=0;live=true;mode=m;outcome=result;pending=2;files_effects_init(&s,alive,retain,settle,call,done,NULL);}
static int run(void){int rc=0;for(unsigned i=0;i<30&&!rc;i++)rc=files_effects_step(&s,pending,true);return rc;}
int main(void){
 reset(0,1);for(unsigned i=0;i<10;i++)assert(!files_effects_step(&s,pending,false));assert(!calls);assert(run()==1&&applies==1&&!pending&&!retains);
 reset(0,2);assert(run()==1&&applies==1&&!pending&&!progresses);assert(!files_effects_step(&s,pending,true)&&applies==1);
 reset(0,3);assert(run()==1&&applies==1&&progresses==1&&done_count==2&&!retains);
 reset(6,3);assert(run()==-1&&applies==1&&progresses==1&&retains==1);
 reset(1,1);assert(!run()&&!calls);mode=0;assert(run()==1&&applies==1);
 reset(2,1);assert(run()==2&&!applies&&pending==2&&s.unsupported);
 reset(3,1);assert(run()==-1&&applies==1&&retains==1);unsigned before=calls;assert(run()==-1&&calls==before);
 reset(4,1);assert(run()==-1&&retains==1&&!applies);
 reset(5,1);assert(run()==-1&&retains==1&&!applies);
 reset(0,1);assert(!files_effects_step(&s,pending,true));pending=0;assert(run()==-1&&retains==1);
 puts("Files effects: 10 state/custody scenarios PASS");
}
