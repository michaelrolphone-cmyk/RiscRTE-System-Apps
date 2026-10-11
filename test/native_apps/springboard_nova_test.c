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
static unsigned event_index,present_started,last_touch_ms,max_touch_gap;
static unsigned ms,polls,subs,grants,frames,presents,scenario,launches,last_present,max_presents;
static uint16_t pixels[240*240];static char launched[128];
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=ms;return polls<350;}
static void yield_ms(uint32_t n){ms+=scenario==23?8:scenario==24?33:n;if((scenario==19||scenario==30) && polls==6)ms+=1000;}
static bool diagnostic(const char *s){(void)s;return true;}
static bool launch_app(const char *s){++launches;if(scenario==11)return false;strcpy(launched,s);return true;}
static bool get_info(void *c,risc_display_info_v1 *o){(void)c;memset(o,0,sizeof(*o));o->width=o->height=240;o->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);o->nominal_refresh_millihz=scenario==22?0:60000;o->typical_present_latency_us=16000;return true;}
static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.size_bytes=sizeof(pixels),.width=240,.height=scenario==10?200:240,.stride_bytes=480,.pixel_format=f};return true;}
static void release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1&&frames);frames=0;}
static void save_frame(void){
 const char *dir=getenv("NOVA_FRAMES");if(!dir)return;char name[512];snprintf(name,sizeof(name),"%s/frame-%04u-%06u.rgb565",dir,presents,ms);FILE *f=fopen(name,"wb");assert(f);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
}
static bool submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){(void)c;(void)r;(void)n;(void)o;assert(f==1&&frames);if(scenario==9||(scenario==34&&polls>=6))return false;frames=0;if(presents && scenario!=22)assert(ms-last_present>=40);last_present=ms;*token=++presents;present_started=ms;save_frame();return true;}
static bool present_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;(void)t;s->state=(scenario==36||scenario==37)&&presents>1&&ms-present_started<(scenario==36?120u:600u)?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t subscribe(void *c){(void)c;++subs;return 1;}
static bool unsubscribe(void *c,uint64_t n){(void)c;assert(n==1&&subs);--subs;return true;}
static bool poll_touch(void *c,size_t n){(void)c;assert(n==1);++polls;if(last_touch_ms&&ms-last_touch_ms>max_touch_gap)max_touch_gap=ms-last_touch_ms;last_touch_ms=ms;event_index=0;return !(scenario==17&&polls==3);}
static int32_t next_touch(void *c,uint64_t n,risc_touch_event_v1 *e){
 (void)c;(void)n;
 if((scenario==3||scenario==18)&&polls==3)return -1;
 if(scenario==25&&polls==3){memset(e,0,sizeof(*e));return 1;}
 if(polls==3 && event_index<2 && scenario>=26 && scenario<=28){
  *e=(risc_touch_event_v1){.id=scenario==28?1:0,.x=120,.y=120};
  if(scenario==26)e->kind=event_index?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP;
  if(scenario==27){e->kind=RISC_TOUCH_EVENT_MOVE;e->x=event_index?120:180;}
  if(scenario==28)e->kind=event_index?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN;
  ++event_index;return 1;
 }
 if(polls==3 && !event_index && (scenario==29||scenario==31)){
  *e=(risc_touch_event_v1){.id=0,.kind=RISC_TOUCH_EVENT_UP,.x=scenario==29?260:200,.y=120};++event_index;return 1;
 }
 return 0;
}
static bool snapshot(void *c,risc_touch_snapshot_v1 *s){
 (void)c;memset(s,0,sizeof(*s));s->width=s->height=240;int x=120,y=120;bool down=false;
 if(scenario==1||scenario==2||scenario==16){down=polls<=3;if(scenario==2){x=120+(int)polls*20;y=120+(int)polls*10;}}
 else if(scenario==6||scenario==7||scenario==14||scenario==15||scenario==23||scenario==24){down=polls>=2&&polls<=6;if(polls>=3){x=120+(int)(polls-2)*20;y=120+(int)(polls-2)*12;}}
 else if(scenario==18){down=polls>=2&&polls<=6;x=120+(int)(polls>3?polls-3:0)*20;}
 else down=polls==2;
 if(scenario==8||scenario==13||scenario==21||scenario==32||scenario==33)down=false;
 if(scenario>=26&&scenario<=28)down=polls>=2&&polls<=4;
 if(scenario==30)down=polls>=2&&polls<=8;
 if(scenario==16&&polls==6)down=true;
 if(scenario==20&&polls==5)down=true;
 if(scenario==22){down=polls==2;x=160;y=100;}
 if(scenario==12){x=20;y=20;}
 if(scenario==4&&polls==3){down=true;s->contacts[0].id=2;}
 if(scenario==5&&polls==3){down=true;s->contact_count=2;}
 if(down){if(!s->contact_count)s->contact_count=1;s->contacts[0].x=(uint16_t)x;s->contacts[0].y=(uint16_t)y;
#ifdef TEST_ROTATION_180
 s->contacts[0].x=(uint16_t)(239-s->contacts[0].x);s->contacts[0].y=(uint16_t)(239-s->contacts[0].y);
#endif
 }
 return true;
}
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
 bool expect=CATALOG_COUNT>0&&(scenario==0||scenario==16||scenario==19||scenario==20||scenario==22||scenario==30||scenario==36||scenario==37);
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
