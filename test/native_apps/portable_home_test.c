#include "PortableTouch.h"
#include <assert.h>
#include <stdio.h>
static risc_touch_snapshot_v1 snap;
static risc_touch_event_v1 queue[80];static unsigned length,event_index;
static bool okay=true;static int end=0;static unsigned grants,subscriptions,checks;
static uint64_t sub(void*c){(void)c;subscriptions++;return 1;}
static bool unsub(void*c,uint64_t token){(void)c;assert(token==1);subscriptions--;return true;}
static bool poll(void*c,size_t n){(void)c;(void)n;return okay;}
static int32_t next(void*c,uint64_t token,risc_touch_event_v1*e){(void)c;(void)token;if(event_index<length){*e=queue[event_index++];return 1;}return end;}
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=snap;return true;}
static const risc_touch_api_v1 api={1,sizeof(api),NULL,sub,unsub,poll,next,snapshot};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!strcmp(n,"input.touch.raw")&&v==1&&!id);g->api=&api;grants++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api);g->api=NULL;grants--;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static portable_touch touch;
static portable_touch_sample read(bool expected){portable_touch_sample s;portable_touch_read(&touch,&s);assert(s.home_pressed==expected);assert(!s.released);checks++;return s;}
static void edge(unsigned id,bool down){
 if(event_index==length)event_index=length=0;
 assert(length<sizeof(queue)/sizeof(*queue));
 queue[length++]=(risc_touch_event_v1){.kind=down?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,.id=id,.sequence=++snap.sequence,.timestamp_ms=++snap.timestamp_ms};
 if(down)snap.buttons|=1u<<id;else snap.buttons&=~(1u<<id);
}
int main(void){
 snap.width=480;snap.height=800;snap.buttons=RISC_TOUCH_BUTTON_PRIMARY;
 assert(portable_touch_open(&touch,&runtime));read(false);read(false); /* inherited held */
 edge(0,false);read(false);edge(0,true);read(true);read(false);
 edge(0,false);read(false); /* one short press entirely between snapshots */
 edge(0,true);edge(0,false);read(true);read(false);read(false);
 okay=false;edge(0,true);assert(read(false).cancelled);okay=true;read(false);read(false);
 edge(0,false);read(false);edge(0,true);read(true);
 end=-1;assert(read(false).cancelled);end=0;read(false);edge(0,false);read(false);
 /* A provider overflow reports a stream gap, then rearms only after neutral. */
 edge(0,true);queue[event_index].sequence++;snap.sequence++;assert(read(false).cancelled);
 read(false);edge(0,false);read(false);read(false);edge(0,true);read(true);edge(0,false);read(false);
 /* A healthy burst is no longer mistaken for overflow: one ordered edge per
  * call preserves all 16 presses, even though the latest snapshot is neutral. */
 for(unsigned i=0;i<32;i++)edge(0,!(i&1));
 for(unsigned i=0;i<32;i++)read(!(i&1));
 read(false);
 /* Non-primary buttons never navigate. */
 edge(1,true);read(false);edge(1,false);read(false);
 assert(portable_touch_close(&touch,&runtime));edge(0,true);
 assert(portable_touch_open(&touch,&runtime));read(false);
 uint16_t x=0,y=0;assert(!portable_touch_tap(&touch,&x,&y));assert(portable_touch_close(&touch,&runtime));assert(!grants&&!subscriptions);
 printf("Home input: %u ordered-edge checks, held start/reset, gap/fault rearming and no synthetic taps passed\n",checks);
}
