/* The real Nova controller, raw-touch adapter and RGB565 renderer. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/springboard.c"
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"

#ifndef PAGING_CATALOG_COUNT
#define PAGING_CATALOG_COUNT 21
#endif
#define APP(n,g) {.display_name=n,.file_name=n ".elf",.icon="solid:" g,.compatible=true}
const t5_app_manifest_t portable_catalog[]={
 APP("Clock","f017"), APP("Battery","f240"), APP("Settings","f013"),
 APP("Calculator","f1ec"), APP("Stopwatch","f2f2"), APP("Alarms","f0f3"),
 APP("Countdown","f254"), APP("Wi-Fi","f1eb"), APP("Points in Time","f783"),
 APP("Frequency Generator","f028"), APP("Audio Spectrum","f130"), APP("File Browser","f07c"),
 APP("BLE Scanner","f7c0"), APP("LoRa Messages","f27a"), APP("Clock Two","f017"),
 APP("Battery Two","f240"), APP("Settings Two","f013"), APP("Calculator Two","f1ec"),
 APP("Stopwatch Two","f2f2"), APP("Alarms Two","f0f3"), APP("Countdown Two","f254")};
const unsigned portable_catalog_count=PAGING_CATALOG_COUNT;
int app_module_init(void);void app_module_fini(void);
static uint16_t guarded[240*243+2];
static unsigned ms,grants,subscriptions,frames,presents,launches,cases;
static unsigned present_input_mode,present_until,present_samples;
static bool present_active;
static bool opened,whole_app,then_tap,inherited,reverse,defer_render;
static char launched[64],page_caption[32];
static nova_state scene;
static const springboard_presentation *production;
static springboard_presentation observed;
static t5_app_api_v1 extended_catalog_api;
static bool extended_get(uint32_t i,t5_app_manifest_t *out){
 if(!out || i>=count)return false;
 *out=portable_catalog[i%21];return true;
}
typedef struct {int contacts,x,y,id,fault;unsigned events;int kind,event_x,event_y,event_id;} raw_sample;
static raw_sample raw;
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=ms;return !whole_app || ms<1200;}
static void yield_ms(uint32_t n){ms+=n;}
static bool diagnostic(const char *s){(void)s;return true;}
static bool launch_app(const char *s){++launches;snprintf(launched,sizeof(launched),"%s",s);return true;}
static bool get_info(void *c,risc_display_info_v1 *o){(void)c;*o=(risc_display_info_v1){.width=240,.height=240,
 .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.nominal_refresh_millihz=60000,.typical_present_latency_us=16000};return true;}
static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(!frames);frames=1;
 *s=(risc_display_surface_v1){.frame=1,.pixels=guarded+1,.size_bytes=240*243*2,
 .width=240,.height=240,.stride_bytes=243*2,.pixel_format=f};return true;}
static void release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1&&frames);frames=0;}
static void guards(void){assert(guarded[0]==0xa55a && guarded[240*243+1]==0xa55a);
 for(unsigned y=0;y<240;y++)for(unsigned x=240;x<243;x++)assert(guarded[1+y*243+x]==0xa55a);}
static bool submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 (void)c;(void)r;(void)n;(void)o;assert(f==1&&frames);guards();frames=0;*t=++presents;
 if(present_input_mode && scene.page_unshown){present_until=ms+120;present_active=true;}
 return true;}
static bool status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){
 (void)c;(void)t;present_active=present_active && ms<present_until;
 s->state=present_active?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t subscribe(void *c){(void)c;++subscriptions;return 1;}
static bool unsubscribe(void *c,uint64_t n){(void)c;assert(n==1&&subscriptions);--subscriptions;return true;}
static bool poll_touch(void *c,size_t n){(void)c;assert(n==1);return raw.fault!=1;}
static int32_t next_touch(void *c,uint64_t n,risc_touch_event_v1 *e){
 (void)c;assert(n==1);if(raw.fault==2)return -1;
 if(raw.fault==3){memset(e,0,sizeof(*e));return 1;}
 if(whole_app && raw.contacts && ((ms>=240 && ms<440) || ms>=500)){
  raw.events=1;raw.kind=RISC_TOUCH_EVENT_UP;raw.event_x=raw.x;raw.event_y=raw.y;raw.contacts=0;
 }
 if(present_active && present_input_mode==2 && raw.contacts && ms>=present_until-32){
  raw.events=1;raw.kind=RISC_TOUCH_EVENT_UP;raw.event_x=raw.x;raw.event_y=raw.y;raw.contacts=0;
 }
 if(!raw.events)return 0;
 --raw.events;*e=(risc_touch_event_v1){.kind=raw.kind,.id=raw.event_id,.x=raw.event_x,.y=raw.event_y};
#if PORTABLE_TOUCH_ROTATION == 180
 e->x=(uint16_t)(239-e->x);e->y=(uint16_t)(239-e->y);
#endif
 return 1;
}
static bool snapshot(void *c,risc_touch_snapshot_v1 *s){
 (void)c;if(raw.fault==4)return false;
 if(present_active){
  raw.contacts=present_input_mode==1 || ms<present_until-32;raw.x=raw.y=120;++present_samples;
 }
 if(whole_app){
  raw=(raw_sample){0};
  unsigned start=inherited?0:80;
  if(ms>=start && ms<240){raw.contacts=1;raw.x=reverse?80:180;raw.y=120;
   if(ms>=160)raw.x=reverse?180:80;}
  if(then_tap && ms>=440 && ms<500){raw.contacts=1;raw.x=120;raw.y=120;}
 }
 memset(s,0,sizeof(*s));s->width=s->height=240;s->contact_count=raw.contacts;
 s->contacts[0].id=raw.id;s->contacts[0].x=(uint16_t)raw.x;s->contacts[0].y=(uint16_t)raw.y;
#if PORTABLE_TOUCH_ROTATION == 180
 s->contacts[0].x=(uint16_t)(239-s->contacts[0].x);s->contacts[0].y=(uint16_t)(239-s->contacts[0].y);
#endif
 return true;
}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=get_info,
 .acquire=acquire_frame,.release=release_frame,.submit=submit,.present_status=status};
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot};
static bool acquire(const char *name,uint32_t v,uint64_t id,risc_runtime_capability_v1 *g){assert(v==1&&!id);
 if(!strcmp(name,"display.output"))g->api=&display_api;
 else if(!strcmp(name,"input.touch.raw"))g->api=&touch_api;else return false;
 ++grants;return true;}
static bool release(risc_runtime_capability_v1 *g){assert(grants&&g->api);--grants;g->api=NULL;return true;}
static const risc_runtime_api_v1 runtime={1,sizeof(runtime),health,yield_ms,diagnostic,launch_app,acquire,release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
static void caption(int y,const char *text,bool clock,uint32_t rgb){
 if(y==224)snprintf(page_caption,sizeof(page_caption),"%s",text);
 production->caption(y,text,clock,rgb);
}
static void close_scene(void){if(!opened)return;app_module_fini();opened=false;assert(!grants&&!subscriptions&&!frames);guards();}
static void open_scene(bool held){
 close_scene();raw=(raw_sample){.contacts=held?1:0,.x=180,.y=120};ms=0;whole_app=false;defer_render=false;
 present_input_mode=present_until=present_samples=0;present_active=false;
 launches=0;launched[0]=page_caption[0]=0;
 for(unsigned i=0;i<sizeof(guarded)/sizeof(*guarded);i++)guarded[i]=0xa55a;
 assert(app_module_init()==0);opened=true;api=t5_app_get_api(T5_APP_ABI_VERSION);assert(api);
 assert(api->installed_apps_refresh());count=api->installed_apps_count();assert(count==PAGING_CATALOG_COUNT);selected=0;
 production=springboard_presentation_get();assert(production);observed=*production;observed.caption=caption;
 scene=(nova_state){.view=&observed,.initial=true,.pending=-1,.time_known=true,.hour=10,.minute=40};nova_layout(&scene);
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 springboard_contact initial={0};production->contact(&initial);nova_contact_input(&scene,&initial,0);
#endif
 nova_render(&scene);++cases;
}
static void step(int contacts,int x,int y,uint32_t elapsed){
 if(raw.contacts && !contacts && !raw.events){raw.events=1;raw.kind=RISC_TOUCH_EVENT_UP;raw.event_x=x;raw.event_y=y;raw.event_id=raw.id;}
 raw.contacts=contacts;raw.x=x;raw.y=y;ms+=elapsed;
 t5_app_input_t input;assert(api->poll(&input,0));springboard_contact sample={0};production->contact(&sample);
 nova_contact_input(&scene,&sample,elapsed);nova_tick(&scene,elapsed>250?250:elapsed);
 if(scene.dirty && !defer_render)nova_render(&scene);
 assert(!launches);
}
static void neutral(void){step(0,0,0,20);}
static void swipe(int direction,unsigned elapsed){
 int start=direction>0?180:80,end=direction>0?80:180;
 neutral();
 step(1,start,120,20);step(1,end,120,elapsed-20);step(0,end,120,20);
 assert(scene.pending<0 && !scene.down);
}
static void save_frame(const char *name){
 const char *dir=getenv("NOVA_VISUALS");if(!dir)return;char path[512];
 snprintf(path,sizeof(path),"%s/%s-%u.ppm",dir,name,PAGING_CATALOG_COUNT);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++){
  unsigned p=guarded[1+y*243+x];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
  assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
static void expect_page(unsigned first){if(scene.first!=first)fprintf(stderr,"case %u: page %u expected %u, contact age %u, moved %u\n",cases,scene.first,first,scene.contact_ms,scene.moved);assert(scene.first==first);assert(selected>=first && selected<first+scene.n);
 if(count>19){char expected[32];snprintf(expected,sizeof(expected),"%u / %u",first/19+1,(count+18)/19);assert(!strcmp(page_caption,expected));}
 assert(scene.pending<0);
}
static void whole_app_case(bool held,bool right,bool tap){
 close_scene();raw=(raw_sample){0};ms=0;whole_app=true;inherited=held;reverse=right;then_tap=tap;launches=0;launched[0]=0;
 for(unsigned i=0;i<sizeof(guarded)/sizeof(*guarded);i++)guarded[i]=0xa55a;
 assert(app_module_init()==0);opened=true;app_main();
 if(!tap){assert(!launches);if(count>19 && !held)assert(selected==19);else assert(selected<19 || !count);}
 else {assert(launches==1);assert(!strcmp(launched,count>19?"Alarms Two.elf":"Clock.elf"));}
 close_scene();whole_app=false;++cases;
}
int main(void){
 open_scene(false);neutral();save_frame("page-start");swipe(1,120);
 if(count>19){expect_page(19);assert(scene.n==count-19);assert(scene.motion.settled);save_frame("page-left-partial");
  swipe(1,120);expect_page(0);save_frame("page-left-wrap");swipe(-1,120);expect_page(19);save_frame("page-right-wrap");
  swipe(-1,120);expect_page(0);
 }else {assert(!scene.first && scene.pending<0);if(count)assert(scene.motion.x<0);save_frame("single-page-pan");}
 if(count){
  open_scene(false);neutral();step(1,180,120,20);step(1,80,120,330);step(0,80,120,20);
  expect_page(count>19?19:0); /* Inclusive 350 ms boundary. */
  open_scene(false);neutral();step(1,180,120,20);step(1,80,120,331);step(0,80,120,20);
  expect_page(0);assert(scene.motion.x<0);save_frame("slow-pan");
  open_scene(false);neutral();step(1,180,120,20);step(1,130,120,80);step(0,130,120,20);expect_page(count>19?19:0);
  open_scene(false);neutral();step(1,180,120,20);step(1,131,120,80);step(0,131,120,20);expect_page(0);
  /* Diagonal travel, a vertical excursion returning to the start row, and jitter. */
  const int paths[][4]={{80,170,80,170},{160,180,80,120},{184,120,179,120},{100,120,180,120}};
  for(unsigned i=0;i<sizeof(paths)/sizeof(*paths);i++){
   open_scene(false);neutral();step(1,180,120,20);step(1,paths[i][0],paths[i][1],40);
   step(1,paths[i][2],paths[i][3],40);step(0,paths[i][2],paths[i][3],20);expect_page(0);
  }
  /* An explicit UP may contain all final travel. */
  open_scene(false);neutral();step(1,180,120,20);raw.events=1;raw.kind=RISC_TOUCH_EVENT_UP;raw.event_x=80;raw.event_y=120;
  step(0,80,120,100);expect_page(count>19?19:0);
  /* A missing UP endpoint is a contact gap, not a completed page gesture. */
  open_scene(false);neutral();step(1,180,120,20);step(1,80,120,80);
  raw.contacts=0;step(0,80,120,20);expect_page(0);assert(scene.motion.x<0);
  /* Hidden queued movement and a replaced contact cannot become a tap/swipe. */
  open_scene(false);neutral();step(1,180,120,20);raw.events=1;raw.kind=RISC_TOUCH_EVENT_MOVE;raw.event_x=120;raw.event_y=120;
  step(1,180,120,40);step(0,180,120,20);expect_page(0);
  open_scene(false);neutral();step(1,180,120,20);step(1,80,120,40);
  raw.events=1;raw.kind=RISC_TOUCH_EVENT_DOWN;raw.event_x=80;raw.event_y=120;
  step(1,80,120,20);step(0,80,120,20);expect_page(0);
  /* Inherited contact pans; only a new neutral-armed contact can page. */
  open_scene(true);step(1,180,120,20);step(1,80,120,80);step(0,80,120,20);expect_page(0);
  assert(scene.motion.x<0);swipe(1,120);expect_page(count>19?19:0);
  /* Poll/event/queue/snapshot failure, multitouch, identity change, out of bounds. */
  for(unsigned fault=1;fault<=7;fault++){
   open_scene(false);neutral();step(1,180,120,20);step(1,130,120,40);
   if(fault<=4)raw.fault=fault;
   if(fault==6)raw.id=1;
   step(fault==5?2:1,fault==7?260:100,120,40);assert(!scene.down && scene.pending<0);
   raw.fault=0;raw.id=0;step(1,80,120,20);step(0,80,120,20);expect_page(0);
   swipe(-1,120);expect_page(count>19?19:0);
  }
  /* Long gaps cannot be hidden by capped physics integration or later samples. */
  open_scene(false);neutral();step(1,180,120,20);step(1,130,120,1000);
  step(1,100,120,20);step(1,80,120,20);step(0,80,120,20);expect_page(0);
  /* A page swipe interrupts a pending icon launch. */
  open_scene(false);neutral();step(1,120,120,20);step(0,120,120,20);assert(scene.pending==0);
  swipe(1,120);expect_page(count>19?19:0);assert(!scene.pulse);
  /* A fresh center tap after a page switch is eligible on the new page. */
  if(count>19){neutral();step(1,120,120,20);step(0,120,120,20);assert(scene.pending==0 && scene.first==19);}
  /* Existing bottom-caption page control still wraps and cannot launch. */
  open_scene(false);neutral();step(1,120,232,20);step(0,120,232,20);expect_page(count>19?19:0);
  if(count>19){neutral();step(1,120,232,20);step(0,120,232,20);expect_page(0);}
  if(count>19)for(unsigned route=0;route<3;route++){
   open_scene(false);neutral();defer_render=true;
   if(route==2)nova_show_page(&scene,19); /* The cross-page button route. */
   else if(route==1){step(1,120,232,20);step(0,120,232,20);}else swipe(1,120);
   assert(scene.first==19 && scene.page_unshown);
   step(1,120,120,10);assert(scene.page_neutral && scene.pending<0);
   nova_render(&scene);defer_render=false;assert(!scene.page_unshown);
   step(0,120,120,10);assert(scene.pending<0 && !scene.page_neutral);
   step(1,120,120,20);step(0,120,120,20);assert(scene.pending==0);
  }
  /* The production adapter samples DOWN/UP while a real present is pending. */
  if(count>19)for(unsigned mode=1;mode<=2;mode++){
   open_scene(false);neutral();present_input_mode=mode;swipe(1,120);
   assert(present_samples>=2 && scene.page_neutral && !scene.page_unshown);
   present_input_mode=0;
   if(mode==1){step(1,120,120,10);assert(scene.page_neutral && scene.pending<0);}
   step(0,120,120,10);assert(scene.pending<0 && !scene.page_neutral);
   step(1,120,120,20);step(0,120,120,20);assert(scene.pending==0);
  }
  /* Small tap jitter remains a tap; its net travel must not become paging. */
  open_scene(false);neutral();step(1,120,120,20);step(1,122,120,20);step(0,122,120,20);
  assert(scene.pending==0 && !scene.first);
 }
 whole_app_case(false,false,false);whole_app_case(false,true,false);whole_app_case(true,false,false);
 if(count>19 || count==1)whole_app_case(false,false,true);
 /* The shared controller supports more pages than the Watch client's 21-app
  * admission bound. Extend only catalog reads to prove direction unambiguously
  * with four pages; input and pixel rendering remain the production adapter. */
 open_scene(false);extended_catalog_api=*api;extended_catalog_api.installed_apps_get=extended_get;api=&extended_catalog_api;
 count=58;nova_layout(&scene);nova_render(&scene);neutral();
 const unsigned forward[]={19,38,57,0},backward[]={57,38,19,0};
 for(unsigned i=0;i<4;i++){swipe(1,120);expect_page(forward[i]);}
 for(unsigned i=0;i<4;i++){swipe(-1,120);expect_page(backward[i]);}
 close_scene();printf("Nova paging: %u real controller/adapter cases, %u apps, %u guarded production frames, clean teardown\n",cases,portable_catalog_count,presents);
}
