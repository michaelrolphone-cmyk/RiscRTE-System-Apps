#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
static bool authority=true;static unsigned calls,frees,at_revoke,free_at_revoke,mode;
static void*allocations[128];static unsigned allocations_count;
static void*allocate(size_t n){void*p=malloc(n);assert(p);allocations[allocations_count++]=p;return p;}
static void deallocate(void*p){assert(authority);frees++;for(unsigned i=0;i<allocations_count;i++)if(allocations[i]==p)allocations[i]=NULL;free(p);}
#define malloc allocate
#define free deallocate
#include "canvas_provider.h"
#undef malloc
#undef free
static void callback(unsigned where){assert(authority);calls++;if(mode==where){authority=false;at_revoke=calls;free_at_revoke=frees;}}
static bool alive(void*x){(void)x;return authority;}
static bool cooperate(void*x){(void)x;callback(mode==17?17:8);return authority;}
static unsigned char raster[64];
static bool info(void*x,risc_display_info_v1*out){(void)x;callback(1);*out=(risc_display_info_v1){.width=8,.height=8,.supported_formats=1};return true;}
static bool acquire(void*x,uint32_t f,risc_display_surface_v1*out){(void)x;callback(2);*out=(risc_display_surface_v1){1,raster,8,8,1,64,f};return true;}
static void release(void*x,uint64_t f){(void)x;(void)f;callback(3);}
static bool submit(void*x,uint64_t f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)x;(void)f;(void)r;(void)n;(void)o;callback(4);*t=1;return mode!=3;}
static bool status(void*x,uint64_t t,risc_display_present_status_v1*out){(void)x;(void)t;callback(5);out->state=3;return true;}
static uint64_t subscribe(void*x){(void)x;callback(6);return 1;}
static bool unsubscribe(void*x,uint64_t s){(void)x;(void)s;callback(7);return true;}
static bool poll(void*x,size_t n){(void)x;(void)n;callback(9);return true;}
static int32_t next(void*x,uint64_t s,risc_touch_event_v1*out){(void)x;(void)s;(void)out;callback(10);return 0;}
static bool snapshot(void*x,risc_touch_snapshot_v1*out){(void)x;callback(11);*out=(risc_touch_snapshot_v1){.width=8,.height=8};return true;}
static bool nav_poll(void*x,risc_input_navigation_frame_v1*out){(void)x;callback(12);*out=(risc_input_navigation_frame_v1){0};return true;}
static bool nav_foreground(void*x,const risc_input_foreground_v1*c,size_t count){(void)x;(void)c;callback(!count&&mode==15?15:13);return true;}
static bool nav_reset(void*x){(void)x;callback(mode==16?16:14);return true;}
int main(void){
 risc_display_output_api_v1 d={1,sizeof(d),NULL,info,acquire,release,submit,status,NULL,NULL};
 risc_touch_api_v1 t={1,sizeof(t),NULL,subscribe,unsubscribe,poll,next,snapshot};
 risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,nav_foreground,nav_reset};
 unsigned modes[]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17};
 for(unsigned i=0;i<sizeof(modes)/sizeof(*modes);i++){
  authority=true;mode=0;calls=frees=0;canvas_provider p;canvas_provider_init(&p,&d,&t,0);if(modes[i]>=12)p.navigation=&nav;p.frames.alive=alive;p.frames.cooperate=cooperate;
  uint64_t s=999;int32_t rc;risc_canvas_open_v1 r={sizeof(r),1,3,0,0};
  if(modes[i]==1||modes[i]==6||modes[i]==13||modes[i]==14){mode=modes[i];rc=p.api.open(&p,&r,&s);assert(s==999);}
  else{
   assert(!p.api.open(&p,&r,&s));risc_canvas_frame_v1 f={.struct_size=sizeof(f)};uint64_t receipt=999;
   if(modes[i]==17){mode=17;rc=p.api.begin_frame(&p,s,&f);assert(f.pixels==NULL);}
   else if(modes[i]==2||modes[i]==3||modes[i]==4||modes[i]==8){assert(!p.api.begin_frame(&p,s,&f));mode=modes[i];rc=p.api.submit(&p,s,f.lease,&receipt);assert(receipt==999);}
   else if(modes[i]==5){assert(!p.api.begin_frame(&p,s,&f));assert(!p.api.submit(&p,s,f.lease,&receipt));mode=5;risc_canvas_receipt_v1 out={.struct_size=sizeof(out)};rc=p.api.poll(&p,s,receipt,&out);}
   else if(modes[i]==7||modes[i]==15||modes[i]==16){mode=modes[i];rc=p.api.close(&p,s);}
   else if(modes[i]==11){mode=11;size_t count=0;uint64_t seq=0;rc=p.api.contacts(&p,s,NULL,0,&count,&seq);}
   else{size_t count=0;uint64_t seq=0;assert(!p.api.contacts(&p,s,NULL,0,&count,&seq));mode=modes[i];risc_canvas_event_v1 event={.struct_size=sizeof(event)};rc=p.api.next(&p,s,&event);}
  }
  assert(rc==RISC_CANVAS_RETAINED&&!authority&&calls==at_revoke&&frees==free_at_revoke);
  assert(p.api.close(&p,s)==RISC_CANVAS_RETAINED&&calls==at_revoke&&frees==free_at_revoke);
  /* Test process owns this simulated-retained memory after the assertion. */
  for(unsigned n=0;n<allocations_count;n++)free(allocations[n]);
  allocations_count=0;
 }
 puts("Seventeen callback authority-revocation paths preserve custody and stop calls/frees");
}
