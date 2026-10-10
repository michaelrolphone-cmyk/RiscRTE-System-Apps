#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#include "PortableRtcClock.h"
#include "../../Apps/SpringboardPresentation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CATALOG_COUNT
#define CATALOG_COUNT 3
#endif
void app_main(void);int app_module_init(void);void app_module_fini(void);
const t5_app_manifest_t portable_catalog[]={
 {.display_name="Clock",.file_name="default.elf",.icon="solid:f017",.compatible=true},
#ifdef NOVA_DAILY_CATALOG
 {.display_name="Battery",.file_name="battery.elf",.icon="solid:f240",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 {.display_name="Calculator",.file_name="calculator.elf",.icon="solid:f00a",.compatible=true},
 {.display_name="Stopwatch",.file_name="stopwatch.elf",.icon="solid:f2f2",.compatible=true}};
#else
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 {.display_name="Battery",.file_name="battery.elf",.icon="solid:f240",.compatible=true}};
#endif
const unsigned portable_catalog_count=CATALOG_COUNT;
static unsigned present_started,last_touch_ms,max_touch_gap,script_ms;
static bool clock_gap;
static unsigned ms,polls,subs,grants,frames,presents,scenario,launches,last_present,max_presents;
static uint16_t pixels[240*240];static char launched[128];
/* Physical reports advance with time, never with capture-only raster polls.
 * The explicit scheduler stall leaves the finger script held across the gap. */
static unsigned script_step(void){return script_ms/20u;}
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=ms;return script_step()<350;}
static void yield_ms(uint32_t n){
 unsigned elapsed=scenario==23?8u:scenario==24?33u:n;ms+=elapsed;script_ms+=elapsed;
 if((scenario==19||scenario==30)&&!clock_gap&&script_step()>=6){ms+=1000;clock_gap=true;}
}
static bool diagnostic(const char *s){(void)s;return true;}
static bool launch_app(const char *s){++launches;if(scenario==11)return false;strcpy(launched,s);return true;}
static bool get_info(void *c,risc_display_info_v1 *o){(void)c;memset(o,0,sizeof(*o));o->width=o->height=240;o->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);o->nominal_refresh_millihz=scenario==22?0:60000;o->typical_present_latency_us=16000;return true;}
static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.size_bytes=sizeof(pixels),.width=240,.height=scenario==10?200:240,.stride_bytes=480,.pixel_format=f};return true;}
static void release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1&&frames);frames=0;}
static void save_frame(void){
 const char *dir=getenv("NOVA_FRAMES");if(!dir)return;char name[512];snprintf(name,sizeof(name),"%s/frame-%04u-%06u.rgb565",dir,presents,ms);FILE *f=fopen(name,"wb");assert(f);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
}
static bool submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){(void)c;(void)r;(void)n;(void)o;assert(f==1&&frames);if(scenario==9||(scenario==34&&script_step()>=6))return false;frames=0;if(presents && scenario!=22)assert(ms-last_present>=40);last_present=ms;*token=++presents;present_started=ms;save_frame();return true;}
static bool present_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;(void)t;s->state=(scenario==36||scenario==37)&&presents>1&&ms-present_started<(scenario==36?120u:600u)?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static risc_touch_snapshot_v1 touch_state;
static risc_touch_event_v1 touch_events[RISC_TOUCH_QUEUE_LENGTH];
static unsigned touch_head,touch_count,last_script_step;
static bool touch_gap;
static risc_touch_contact_v1 script_contact(unsigned id,unsigned x,unsigned y){
#ifdef TEST_ROTATION_180
 x=239u-x;y=239u-y;
#endif
 return (risc_touch_contact_v1){.id=id,.x=(uint16_t)x,.y=(uint16_t)y};
}
static void script_snapshot(unsigned step,risc_touch_snapshot_v1 *s){
 memset(s,0,sizeof(*s));s->width=s->height=240;int x=120,y=120;bool down=false;
 if(scenario==1||scenario==2||scenario==16){down=step<=3;if(scenario==2){x=120+(int)step*20;y=120+(int)step*10;}}
 else if(scenario==6||scenario==7||scenario==14||scenario==15||scenario==23||scenario==24){down=step>=2&&step<=6;if(step>=3){x=120+(int)(step-2)*20;y=120+(int)(step-2)*12;}}
 else if(scenario==18){down=step>=2&&step<=6;x=120+(int)(step>3?step-3:0)*20;}
 else down=step==2;
 if(scenario==8||scenario==13||scenario==21||scenario==32||scenario==33)down=false;
 if((scenario>=26&&scenario<=28)||scenario==38)down=step>=2&&step<=4;
 if(scenario==30)down=step>=2&&step<=8;
 if(scenario==16&&step==6)down=true;
 if(scenario==20&&step==5)down=true;
 if(scenario==22){down=step==2;x=160;y=100;}
 if(scenario==12){x=20;y=20;}
 if(scenario==4&&step==3){down=true;s->contacts[0].id=2;}
 if(scenario==5&&step==3){down=true;s->contact_count=2;s->contacts[1]=script_contact(1,180,120);}
 if(down){if(!s->contact_count)s->contact_count=1;s->contacts[0]=script_contact(s->contacts[0].id,(unsigned)x,(unsigned)y);}
}
static uint64_t subscribe(void *c){
 (void)c;++subs;touch_head=touch_count=0;touch_gap=false;last_script_step=script_step();
 script_snapshot(last_script_step,&touch_state);touch_state.timestamp_ms=ms;return 1;
}
static bool unsubscribe(void *c,uint64_t n){(void)c;assert(n==1&&subs);--subs;return true;}
static int contact_index(const risc_touch_snapshot_v1 *s,unsigned id){
 for(unsigned i=0;i<s->contact_count;++i)if(s->contacts[i].id==id)return (int)i;
 return -1;
}
static void touch_emit(unsigned kind,risc_touch_contact_v1 p){
 assert(touch_count<RISC_TOUCH_QUEUE_LENGTH);
 touch_events[(touch_head+touch_count++)%RISC_TOUCH_QUEUE_LENGTH]=(risc_touch_event_v1){
  .sequence=++touch_state.sequence,.timestamp_ms=ms,.kind=kind,.id=p.id,.x=p.x,.y=p.y};
}
static bool poll_touch(void *c,size_t n){
 (void)c;assert(n==1);++polls;
 if(last_touch_ms&&ms-last_touch_ms>max_touch_gap)max_touch_gap=ms-last_touch_ms;
 last_touch_ms=ms;
 unsigned step=script_step();if(step==last_script_step)return true;last_script_step=step;
 risc_touch_snapshot_v1 next;script_snapshot(step,&next);
 /* Keep explicit faulty/batched reports distinct from ordinary transitions. */
 if(step==3&&((scenario>=25&&scenario<=28)||scenario==38)){
  risc_touch_contact_v1 p=script_contact(0,120,120);
  if(scenario==25)touch_emit(0,p);
  /* Ordered UP then DOWN can span two genuine reports even at the same
   * monotonic timestamp. It ends one gesture and starts another. The selected
   * GT911 emits UP only for removed IDs and DOWN only for absent new IDs. */
  if(scenario==26){touch_emit(RISC_TOUCH_EVENT_UP,p);touch_emit(RISC_TOUCH_EVENT_DOWN,p);}
  if(scenario==38)touch_emit(RISC_TOUCH_EVENT_DOWN,p); /* Duplicate DOWN while held is malformed. */
  if(scenario==27){touch_emit(RISC_TOUCH_EVENT_MOVE,script_contact(0,180,120));touch_emit(RISC_TOUCH_EVENT_MOVE,p);}
  if(scenario==28){p.id=1;touch_emit(RISC_TOUCH_EVENT_DOWN,p);touch_emit(RISC_TOUCH_EVENT_UP,p);}
 }
 /* New IDs precede retired IDs so an identity replacement cannot become a tap. */
 for(unsigned i=0;i<next.contact_count;++i){
  risc_touch_contact_v1 p=next.contacts[i];int old=contact_index(&touch_state,p.id);
  if(old<0)touch_emit(RISC_TOUCH_EVENT_DOWN,p);
  else if(p.x!=touch_state.contacts[old].x||p.y!=touch_state.contacts[old].y)touch_emit(RISC_TOUCH_EVENT_MOVE,p);
 }
 for(unsigned i=0;i<touch_state.contact_count;++i)if(contact_index(&next,touch_state.contacts[i].id)<0){
  risc_touch_contact_v1 p=touch_state.contacts[i];
  if(step==3&&(scenario==29||scenario==31))p=script_contact(0,scenario==29?260u:200u,120);
  touch_emit(RISC_TOUCH_EVENT_UP,p);
 }
 next.sequence=touch_state.sequence;next.timestamp_ms=ms;touch_state=next;
 if(step==3&&(scenario==3||scenario==18)){touch_head=touch_count=0;touch_gap=true;}
 return !(scenario==17&&step==3);
}
static int32_t next_touch(void *c,uint64_t n,risc_touch_event_v1 *e){
 (void)c;assert(n==1);if(touch_gap){touch_gap=false;return -1;}
 if(!touch_count)return 0;
 *e=touch_events[touch_head];touch_head=(touch_head+1u)%RISC_TOUCH_QUEUE_LENGTH;--touch_count;return 1;
}
static bool snapshot(void *c,risc_touch_snapshot_v1 *s){(void)c;*s=touch_state;return true;}
static const risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=get_info,.acquire=acquire_frame,.release=release_frame,.submit=submit,.present_status=present_status};
static bool clock_read(void *c,twatch_rtc_time_v1 *out){(void)c;*out=(twatch_rtc_time_v1){2026,10,4,0,0,40,0};return scenario!=33;}
static const twatch_rtc_api_v1 clock_api={.api_version=2,.struct_size=sizeof(clock_api),.read=clock_read};
static const risc_touch_api_v1 t={1,sizeof(t),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot};
static bool acquire(const char *name,uint32_t v,uint64_t id,risc_runtime_capability_v1 *g){assert(!id&&g->struct_size==sizeof(*g));if(!strcmp(name,"rtc.clock")){assert(v==2);g->api=&clock_api;++grants;return true;}assert(v==1);if(!strcmp(name,"display.output"))g->api=&d;else if(!strcmp(name,"input.touch.raw"))g->api=&t;else return false;++grants;return true;}
static bool release(risc_runtime_capability_v1 *g){assert(grants&&g->api);--grants;g->api=NULL;return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&rt:NULL;}
int main(int argc,char **argv){
 assert(argc==2);scenario=(unsigned)atoi(argv[1]);assert(app_module_init()==0);
#ifdef PORTABLE_RTC_UTC8_DENVER
 const springboard_presentation *v=springboard_presentation_get();uint8_t hour=99,minute=99;assert(v);
 if(scenario!=33){assert(v->clock(&hour,&minute));assert(hour==10&&minute==40);}else assert(!v->clock(&hour,&minute));
#endif
 app_main();app_module_fini();assert(!frames&&!grants&&!subs);
 bool expect=CATALOG_COUNT>0&&(scenario==0||scenario==16||scenario==19||scenario==20||scenario==22||scenario==26||scenario==30||scenario==36||scenario==37);
 assert(!!launched[0]==expect);
 assert(launches==((expect||scenario==11)&&CATALOG_COUNT?1u:0u));
 max_presents=ms/40+1;assert(presents<=max_presents);
 #ifndef PORTABLE_RETAINED_RGB565_HANDOFF
 if(scenario==1||scenario==3||scenario==4||scenario==5||scenario==17||scenario==18||scenario==25)assert(presents<=2);
#else
 if(scenario==1||scenario==3||scenario==4||scenario==5||scenario==17||scenario==18||scenario==25)assert(presents<=7);
#endif
 if(scenario==6||scenario==7||scenario==14||scenario==15||scenario==23||scenario==24)assert(presents<100);
 if(scenario==36||scenario==37)assert(max_touch_gap<=32);
 printf("NOVA real app scenario %u, catalog %u: %u bounded frames, %u launch requests, clean teardown\n",scenario,portable_catalog_count,presents,launches);
}
