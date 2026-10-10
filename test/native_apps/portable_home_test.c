#include "PortableTouch.h"
#include <assert.h>
#include <stdio.h>
static risc_touch_snapshot_v1 snap;
static risc_touch_event_v1 queue[40];static unsigned length,event_index;
static bool okay=true;static int end=0;static unsigned grants,subscriptions;
static uint64_t sub(void*c){(void)c;subscriptions++;return 1;}
static bool unsub(void*c,uint64_t token){(void)c;assert(token==1);subscriptions--;return true;}
static bool poll(void*c,size_t n){(void)c;(void)n;event_index=0;return okay;}
static int32_t next(void*c,uint64_t token,risc_touch_event_v1*e){(void)c;(void)token;if(event_index<length){*e=queue[event_index++];return 1;}return end;}
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=snap;return true;}
static const risc_touch_api_v1 api={1,sizeof(api),NULL,sub,unsub,poll,next,snapshot};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!strcmp(n,"input.touch.raw")&&v==1&&!id);g->api=&api;grants++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api);g->api=NULL;grants--;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static portable_touch touch;
static portable_touch_sample read(bool expected){portable_touch_sample s;portable_touch_read(&touch,&s);assert(s.home_pressed==expected);assert(!s.released);return s;}
int main(void){
 snap.width=480;snap.height=800;assert(portable_touch_open(&touch,&runtime));
 snap.buttons=RISC_TOUCH_BUTTON_PRIMARY;read(false);read(false); /* inherited held */
 snap.buttons=0;read(false);snap.buttons=RISC_TOUCH_BUTTON_PRIMARY;read(true);read(false);
 snap.buttons=0;read(false); /* one short press entirely between snapshots */
 length=2;queue[0]=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_BUTTON_DOWN};queue[1]=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_BUTTON_UP};read(true);
 length=0;read(false); /* no tap or repeated Home */
 okay=false;snap.buttons=RISC_TOUCH_BUTTON_PRIMARY;assert(read(false).cancelled);okay=true;read(false);
 snap.buttons=0;read(false);snap.buttons=RISC_TOUCH_BUTTON_PRIMARY;read(true);
 end=-1;assert(read(false).cancelled);end=0;read(false);snap.buttons=0;read(false);
 length=32;for(unsigned i=0;i<length;i++)queue[i]=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_BUTTON_DOWN};assert(read(false).cancelled);
 length=0;snap.buttons=RISC_TOUCH_BUTTON_PRIMARY;read(false);snap.buttons=0;read(false);
 /* Non-primary buttons never navigate. */
 length=1;queue[0]=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_BUTTON_DOWN,.id=1};snap.buttons=2;read(false);length=0;
 assert(portable_touch_close(&touch,&runtime));assert(portable_touch_open(&touch,&runtime));snap.buttons=1;read(false);
 uint16_t x=0,y=0;assert(!portable_touch_tap(&touch,&x,&y));assert(portable_touch_close(&touch,&runtime));assert(!grants&&!subscriptions);
 puts("Home input: snapshot/event edges, held start/reset, gap/fault/overflow rearming and no synthetic taps passed");
}
