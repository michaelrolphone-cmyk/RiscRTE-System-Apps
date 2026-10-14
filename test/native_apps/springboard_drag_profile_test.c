/* Production controller, presenter and adapter; host CPU timings, no device FPS. */
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#define main existing_regression_main
#include "watch_native_springboard_test.c"
#undef main
static uint64_t cost[8];
static uint64_t stamp(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void pb(void){uint64_t t=stamp();production->begin();cost[0]+=stamp()-t;}
static void pc(int x,int y,int r,uint32_t rgb,uint8_t a){uint64_t t=stamp();production->circle(x,y,r,rgb,a);cost[1]+=stamp()-t;}
static bool pi(int x,int y,int r,const char*n,uint8_t a){uint64_t t=stamp();bool ok=production->icon(x,y,r,n,a);cost[2]+=stamp()-t;return ok;}
static void pt(int y,const char*s,bool c,uint32_t rgb){uint64_t t=stamp();production->caption(y,s,c,rgb);cost[3]+=stamp()-t;}
static const t5_app_api_v1 *base_api;
static void pp(bool full){uint64_t t=stamp();base_api->present(full);cost[4]+=stamp()-t;}
int main(int argc,char**argv){
 assert(argc==3);fw=(unsigned)atoi(argv[1]);fh=(unsigned)atoi(argv[2]);pitch=fw+3;use_async=true;
 guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 open_controller();expect_transition=false;
 springboard_presentation p=*production;p.begin=pb;p.circle=pc;p.icon=pi;p.caption=pt;
 t5_app_api_v1 a=*api;base_api=api;a.present=pp;api=&a;
 nova_state s={.view=&p,.pending=-1,.initial=true};nova_layout(&s);s.time_known=true;s.hour=10;s.minute=21;
 uint64_t hash=1469598103934665603ull;
 for(unsigned n=0;n<240;n++){
  uint64_t t=stamp();springboard_contact c=read_contact(n%80!=79,physical(60+(n%60)*2),physical((unsigned)logical_height()/2));cost[5]+=stamp()-t;
  t=stamp();nova_contact_input(&s,&c,40);nova_tick(&s,40);cost[6]+=stamp()-t;
  ticks+=40;t=stamp();nova_render(&s);cost[7]+=stamp()-t;guards();
  for(unsigned y=0;y<fh;y++)for(unsigned x=0;x<fw;x++){hash^=pixels[y*pitch+x];hash*=1099511628211ull;}
 }
 printf("{\"width\":%u,\"height\":%u,\"frames\":240,\"clear_us\":%.3f,\"circles_us\":%.3f,\"icons_us\":%.3f,\"mask_captions_us\":%.3f,\"submit_copy_us\":%.3f,\"input_poll_us\":%.3f,\"ui_update_us\":%.3f,\"render_us\":%.3f,\"pixels_hash\":\"%016llx\"}\n",fw,fh,cost[0]/240000.0,cost[1]/240000.0,cost[2]/240000.0,cost[3]/240000.0,cost[4]/240000.0,cost[5]/240000.0,cost[6]/240000.0,cost[7]/240000.0,(unsigned long long)hash);
 close_controller();free(guarded);return 0;
}
