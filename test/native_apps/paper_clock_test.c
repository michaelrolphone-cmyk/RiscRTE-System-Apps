#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#include "PortableRtcClock.h"
#include "PortableTimeFormat.h"
#include "RiscBatteryGaugeV1.h"
#include "../../Apps/PaperPresentation.h"
#ifdef PORTABLE_ALARM_CLIENT
#include "AlarmServiceV1.h"
static uint8_t saved_pixels[60*800];
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void app_main(void);int app_module_init(void);void app_module_fini(void);
#ifndef CATALOG_COUNT
#define CATALOG_COUNT 5
#endif
#ifdef TEST_NATIVE_LANDSCAPE
#define PANEL_WIDTH 800
#define PANEL_HEIGHT 480
#else
#define PANEL_WIDTH 480
#define PANEL_HEIGHT 800
#endif
#define ENTRY(n) {.display_name=n,.file_name=n ".elf",.icon="solid:f017",.compatible=true}
const t5_app_manifest_t portable_catalog[]={
 {.display_name="File Browser",.file_name="file_browser.elf",.icon="solid:f07c",.compatible=true},
 {.display_name="Serial Monitor",.file_name="serial_monitor.elf",.icon="solid:f120",.compatible=true},
 {.display_name="Bluetooth Scanner",.file_name="ble_scanner.elf",.icon="solid:f7c0",.compatible=true},
 {.display_name="Points in Time",.file_name="points_in_time.elf",.icon="solid:f017",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 ENTRY("Six"),ENTRY("Seven"),ENTRY("Eight"),ENTRY("Nine"),ENTRY("Ten"),ENTRY("Eleven"),ENTRY("Twelve"),ENTRY("Thirteen"),ENTRY("Fourteen"),ENTRY("Fifteen"),ENTRY("Sixteen"),ENTRY("Seventeen")};
const unsigned portable_catalog_count=CATALOG_COUNT;
static unsigned scenario,ms,polls,grants,subs,frames,presents,launches;
static uint8_t pixels[60*800];static char launched[128];
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=ms;return polls<120;}
static void yield_ms(uint32_t n){ms+=n;}
static bool diagnostic(const char *s){(void)s;return true;}
static bool launch_app(const char *s){launches++;if(scenario==9&&launches==1)return false;snprintf(launched,sizeof(launched),"%s",s);return true;}
static bool get_info(void *c,risc_display_info_v1 *o){(void)c;memset(o,0,sizeof(*o));o->width=PANEL_WIDTH;o->height=PANEL_HEIGHT;o->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1);o->flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE;o->damage_x_alignment=o->damage_width_alignment=8;return true;}
static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(f==RISC_DISPLAY_FORMAT_MONO1);assert(!frames);frames=1;memset(pixels,0xcd,sizeof(pixels));*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=PANEL_WIDTH,.height=PANEL_HEIGHT,.size_bytes=sizeof(pixels),.stride_bytes=PANEL_WIDTH/8,.pixel_format=f};return true;}
static void release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1&&frames);frames=0;}
static bool submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){
 (void)c;if(presents){assert(n==1);assert(r&&r->x>=0&&r->y>=0&&r->width&&r->height&&(unsigned)r->x+r->width<=PANEL_WIDTH&&(unsigned)r->y+r->height<=PANEL_HEIGHT);assert(r->x%8==0&&r->width%8==0);}else assert(!n);assert(f==1&&frames);assert(o->intent==(presents?RISC_DISPLAY_PRESENT_QUALITY:RISC_DISPLAY_PRESENT_CLEAN));

#ifdef PORTABLE_ALARM_CLIENT
 if(!presents)memcpy(saved_pixels,pixels,sizeof(pixels));
#endif
 frames=0;*token=++presents;
 if(presents==1){const char *path=getenv("PAPER_FRAME");if(path){FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P4\n%u %u\n",PANEL_WIDTH,PANEL_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),out)==sizeof(pixels));fclose(out);}}
 return true;
}
static bool present_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;(void)t;s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t subscribe(void *c){(void)c;subs++;return 1;}
static bool unsubscribe(void *c,uint64_t n){(void)c;assert(n==1&&subs);subs--;return true;}
static bool poll_touch(void *c,size_t n){(void)c;assert(n==1);polls++;return true;}
static int32_t next_touch(void *c,uint64_t n,risc_touch_event_v1 *e){(void)c;(void)n;(void)e;return 0;}
#ifdef PORTABLE_ALARM_CLIENT
static unsigned alarm_acks,alarm_steps,alarm_stops;
static bool alarm_fired;
static alarm_status_v1 alarm_state={.api_version=1,.struct_size=sizeof(alarm_state),.state=ALARM_STATE_READY};
static int32_t alarm_status(void *c,alarm_status_v1 *o){(void)c;*o=alarm_state;return ALARM_OK;}
static int32_t alarm_step(void *c){(void)c;alarm_steps++;
 if(scenario==13 && !alarm_fired && polls>=4){alarm_fired=true;alarm_state.state=ALARM_STATE_ALERT;alarm_state.occurrence=(alarm_token_v1){1,1,1,1};}
 if(alarm_state.state==ALARM_STATE_DISMISSING){alarm_state.state=ALARM_STATE_READY;alarm_state.occurrence=(alarm_token_v1){0};}
 return ALARM_OK;
}
static int32_t alarm_refresh(void *c){(void)c;return ALARM_OK;}
static int32_t alarm_ack(void *c,const alarm_token_v1 *t){(void)c;assert(!memcmp(t,&alarm_state.occurrence,sizeof(*t)));alarm_acks++;alarm_state.state=ALARM_STATE_DISMISSING;return ALARM_PENDING;}
static int32_t alarm_prepare(void *c,alarm_sleep_v1 *o){(void)c;(void)o;return ALARM_INVALID;}
static int32_t alarm_stop(void *c){(void)c;alarm_stops++;return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_prepare,alarm_stop};
#endif
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;memset(s,0,sizeof(*s));s->width=480;s->height=800;
 unsigned step=polls;bool down=step>=2&&step<=4;int x=240,y=360;
 if(scenario==0||scenario>=10)down=false;
 if(scenario==13){down=step==7;x=240;y=650;}
 if(scenario==1&&step>=3)x+=45;
 if(scenario==2&&step>=3)x-=45;
 if(scenario==3&&step>=3)y-=75;
 if(scenario==4&&step>=3)y+=75;
 if(scenario==6&&step>=3){x+=39;y+=66;}
 if(scenario==7){down=step<5;if(step>=2)x+=45;}
 if(scenario==8&&step==3){s->contact_count=2;return true;}
 if(scenario==8&&step>=4)x+=80;
 if(scenario==9){down=(step>=2&&step<=4)||(step>=7&&step<=9);if(step==3||step==4||step==8||step==9)x+=45;}
 if(down){s->contact_count=1;s->contacts[0].id=1;s->contacts[0].x=x;s->contacts[0].y=y;}
 return true;
}
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;*out=(twatch_rtc_time_v1){2026,10,6,2,20,59,0};if(scenario==10&&polls>8){out->hour=21;out->minute=0;}return scenario!=11;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=rtc_read};
static bool battery_read(void*c,risc_battery_sample_v1*out){(void)c;*out=(risc_battery_sample_v1){.percent=84};return scenario!=12;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,battery_read};
static int32_t get(void*c,const char*k,void*out,uint32_t cap,uint32_t*len){(void)c;assert(!strcmp(k,"time_format")&&cap>=4);uint8_t b[]={0x54,1,1,0xa4};memcpy(out,b,4);*len=4;return 0;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,NULL};
static const risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=get_info,.acquire=acquire_frame,.release=release_frame,.submit=submit,.present_status=present_status};
static const risc_touch_api_v1 t={1,sizeof(t),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot};
static bool acquire(const char *name,uint32_t v,uint64_t id,risc_runtime_capability_v1 *g){
#ifdef PORTABLE_ALARM_CLIENT
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(v==1&&!id);g->api=&alarm_api;grants++;return true;}
#endif
 if(!strcmp(name,"rtc.clock")){assert(v==2&&!id);g->api=&rtc_api;grants++;return true;}
 if(!strcmp(name,"board.battery")){assert(v==1&&!id);g->api=&battery_api;grants++;return true;}
 if(!strcmp(name,"storage.key-value")){assert(v==1&&id==1);g->api=&kv;grants++;return true;}
 assert(v==1&&!id&&g->struct_size==sizeof(*g));if(!strcmp(name,"display.output"))g->api=&d;else if(!strcmp(name,"input.touch.raw"))g->api=&t;else return false;grants++;return true;}
static bool release(risc_runtime_capability_v1 *g){assert(grants&&g->api);grants--;g->api=NULL;return true;}
static const risc_runtime_api_v1 rt={1,sizeof(rt),health,yield_ms,diagnostic,launch_app,acquire,release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&rt:NULL;}
int main(int argc,char**argv){assert(argc==2);scenario=(unsigned)atoi(argv[1]);assert(app_module_init()==0);
#ifdef PORTABLE_RTC_WALL_TIME
 const paper_presentation*view=paper_presentation_get();uint8_t h=0,m=0;assert(view);if(scenario==11)assert(!view->clock(&h,&m));else assert(view->clock(&h,&m)&&h==20&&m==59);
#endif
 app_main();app_module_fini();assert(!grants&&!subs&&!frames);
 if(scenario>=1&&scenario<=4)assert(launches==1&&!strcmp(launched,"springboard.elf"));
 else if(scenario==9)assert(launches==2&&!strcmp(launched,"springboard.elf"));
 else assert(!launches);
 if(scenario==13){
#ifdef PORTABLE_ALARM_CLIENT
 assert(alarm_acks==1&&presents==3&&!memcmp(saved_pixels,pixels,sizeof(pixels)));
#endif
 }else if(scenario==10)assert(presents==2);else if(scenario!=9)assert(presents==1);
 printf("Paper clock scenario %u: %u frames, %u launches, all grants released\n",scenario,presents,launches);return 0;}
