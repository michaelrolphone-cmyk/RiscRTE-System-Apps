#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscAppCatalogV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscTouchV1.h"
#include "PortableRtcClock.h"
#ifdef TEST_NATIVE_TIME
#include "RiscRealtimeV1.h"
#include "PortableTimeZonePreference.h"
#endif
#include "SpringboardPresentation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CATALOG_COUNT
#define CATALOG_COUNT 3
#endif
void app_main(void);int app_module_init(void);void app_module_fini(void);
#ifdef NOVA_WATCH_CATALOG
const t5_app_manifest_t portable_catalog[]={
#include "watch_suite_catalog.inc"
};
#else
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
#endif
const unsigned portable_catalog_count=CATALOG_COUNT;
static unsigned direction,positive_seen,negative_seen,visible_positive,visible_negative,settled_checks;
static unsigned report_period=20,report_jitter,report_repeated,events_emitted;
static unsigned cost_ns, read_ms, bus_us, panel_ms, yield_extra_ms, last_work, gaps, max_queue, max_age, events_taken, cancellations, frame_start, max_frame, max_complete, completed_token, work_ns_remainder;
static unsigned long long work_ns_total, poll_ms_total;
static unsigned present_started,max_touch_gap,script_ms;
static bool clock_gap;
static unsigned ms,polls,subs,grants,frames,presents,scenario,launches,last_present;
void audit_frame_begin(void){frame_start=ms;}
void audit_work(unsigned pixels){unsigned delta=pixels-last_work;last_work=pixels;unsigned long long ns=(unsigned long long)delta*cost_ns+work_ns_remainder;work_ns_total+=(unsigned long long)delta*cost_ns;unsigned dt=(unsigned)(ns/1000000);work_ns_remainder=(unsigned)(ns%1000000);ms+=dt;script_ms+=dt;}
#ifndef TEST_WIDTH
#define TEST_WIDTH 240
#define TEST_HEIGHT 240
#endif
static uint16_t pixels[TEST_WIDTH*TEST_HEIGHT];static char launched[128];
/* App frames are rate-limited at their logical present boundary. Deferred
 * raster completion and an explicit ownership drain may submit closer together. */
static unsigned logical_presents,last_logical_present,launch_ms;
static void (*original_present)(bool);
static void logical_present(bool full){
 /* Slow-cost experiment records cadence, rather than imposing an expected result. */
 last_logical_present=ms;++logical_presents;original_present(full);
}
const t5_app_api_v1 *__real_t5_app_get_api(uint32_t version);
const t5_app_api_v1 *__wrap_t5_app_get_api(uint32_t version){
 const t5_app_api_v1 *source=__real_t5_app_get_api(version);
 if(!source)return NULL;
 static t5_app_api_v1 observed;observed=*source;
 original_present=source->present;observed.present=logical_present;return &observed;
}
/* Physical reports advance with time, never with capture-only raster polls.
 * The explicit scheduler stall leaves the finger script held across the gap. */
static unsigned script_step(void){
#ifdef NOVA_WATCH_CATALOG
 /* A user taps after the first visible frame; raster checkpoints may advance
  * the modeled clock while a full catalog is still being drawn. */
 if(!completed_token)return 0;
#endif
 if(!report_jitter)return script_ms/report_period;
 return (script_ms/(2*report_period))*2+((script_ms%(2*report_period))>=report_period-2);
}
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=ms;
#if defined(NOVA_WATCH_CATALOG) || defined(TEST_NATIVE_TIME)
 return true; /* Test completion is an orderly exit, not a failed Runtime. */
#else
 return script_step()<350;
#endif
}
static void yield_ms(uint32_t n){
 unsigned elapsed=scenario==23?8u:scenario==24?33u:n+yield_extra_ms;ms+=elapsed;script_ms+=elapsed;
 if((scenario==19||scenario==30)&&!clock_gap&&script_step()>=6){ms+=1000;clock_gap=true;}
}
static bool diagnostic(const char *s){(void)s;return true;}
static bool launch_app(const char *s){launch_ms=ms;++launches;if(scenario==11)return false;strcpy(launched,s);return true;}
static bool get_info(void *c,risc_display_info_v1 *o){(void)c;memset(o,0,sizeof(*o));o->width=TEST_WIDTH;o->height=TEST_HEIGHT;o->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);o->nominal_refresh_millihz=scenario==22?0:60000;
#ifdef TEST_UNKNOWN_REFRESH
 o->nominal_refresh_millihz=0;
#endif
#ifdef TEST_SLOW_REFRESH
 o->nominal_refresh_millihz=10000;
#endif
#ifdef TEST_RETAINING_PANEL
 o->flags|=RISC_DISPLAY_INFO_RETAINS_IMAGE;
#endif
o->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;o->nominal_refresh_millihz=0;o->typical_present_latency_us=0;return true;}
static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.size_bytes=sizeof(pixels),.width=TEST_WIDTH,.height=scenario==10?200:TEST_HEIGHT,.stride_bytes=TEST_WIDTH*2,.pixel_format=f};return true;}
static void release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1&&frames);frames=0;}
static uint64_t frame_signature=1469598103934665603ull;
static void save_frame(void){for(unsigned i=0;i<TEST_WIDTH*TEST_HEIGHT;i++){frame_signature^=pixels[i];frame_signature*=1099511628211ull;}
 const char *dir=getenv("NOVA_FRAMES");if(!dir||presents!=1)return;char name[512];snprintf(name,sizeof(name),"%s/frame-%04u-%06u.rgb565",dir,presents,ms);FILE *f=fopen(name,"wb");assert(f);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
}
static bool submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){(void)c;(void)r;(void)n;(void)o;assert(f==1&&frames);if(scenario==9||(scenario==34&&script_step()>=6))return false;frames=0;
#ifdef NOVA_WATCH_CATALOG
 if(!presents)script_ms=0;
#endif
 if(presents){assert(ms>=last_present);}
 if(ms-frame_start>max_frame){max_frame=ms-frame_start;}last_present=ms;*token=++presents;present_started=ms;save_frame();return true;}
static bool present_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;(void)t;s->state=ms-present_started<panel_ms?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(s->state==RISC_DISPLAY_PRESENT_COMPLETE&&t!=completed_token){if(!completed_token)script_ms=0;completed_token=(unsigned)t;if(ms-frame_start>max_complete)max_complete=ms-frame_start;}return true;}
extern const risc_touch_api_v1 *bridge_start(unsigned*);
extern unsigned bridge_poll(unsigned,unsigned,bool,unsigned,size_t,bool*);
extern void bridge_stop(void);
static const risc_touch_api_v1 *real_touch;
static uint64_t subscribe(void*c){(void)c;subs++;return real_touch->subscribe(NULL);}
static bool unsubscribe(void*c,uint64_t n){(void)c;subs--;return real_touch->unsubscribe(NULL,n);}
static bool poll_touch(void*c,size_t n){(void)c;unsigned age=completed_token?script_ms:0;int delta=0;bool down=age>=100&&age<1700;
 if(age>=100&&age<500)delta=(int)(age-100)/5;else if(age<800&&age>=500)delta=80;
 else if(age>=800&&age<1600)delta=80-(int)(age-800)/5;else if(age>=1600)delta=-80;
 if(direction&1){delta=-delta;}unsigned x=233,y=233;if(direction<2)x=(unsigned)(233+delta);else y=(unsigned)(233+delta);
 bool ok=false;unsigned elapsed=bridge_poll(x,y,down,ms,n,&ok);ms+=(unsigned)elapsed;script_ms+=elapsed;polls++;poll_ms_total+=elapsed;return ok;}
static int32_t next_touch(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;int32_t rc=real_touch->next(NULL,n,e);if(rc==-1)++gaps;if(rc==1){events_taken++;printf("EDGE time=%u kind=%u x=%u y=%u id=%u\n",ms,e->kind,e->x,e->y,e->id);}return rc;}
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;return real_touch->snapshot(NULL,s);}
void direction_cancel(void){++cancellations;}
void direction_state(float x,float y,int vx,int vy,bool down,bool visible,bool settled){
 unsigned age=script_ms;if(!completed_token)return;if(getenv("TEST_DIAG_GAP")){if(age>4000){assert(!down&&settled);settled_checks++;}return;}if(age>4000){assert(!down&&settled);settled_checks++;}float axis=direction<2?x:y;float orth=direction<2?y:x;int shown=direction<2?vx:vy;
 if(down&&age>=550&&age<790){assert(orth==0);if((direction^(PORTABLE_TOUCH_ROTATION==180))&1){assert(axis<0);negative_seen++;if(visible&&shown<0)visible_negative++;}else{assert(axis>0);positive_seen++;if(visible&&shown>0)visible_positive++;}}
 if(down&&age>=1630&&age<1690){assert(orth==0);if((direction^(PORTABLE_TOUCH_ROTATION==180))&1){assert(axis>0);positive_seen++;if(visible&&shown>0)visible_positive++;}else{assert(axis<0);negative_seen++;if(visible&&shown<0)visible_negative++;}}
 printf("STATE time=%u age=%u down=%u logical=%f,%f completed=%d,%d valid=%u\n",ms,age,down,x,y,vx,vy,visible);
}
static const risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=get_info,.acquire=acquire_frame,.release=release_frame,.submit=submit,.present_status=present_status};
static bool clock_read(void *c,twatch_rtc_time_v1 *out){(void)c;*out=(twatch_rtc_time_v1){2026,10,4,0,0,40,0};return scenario!=33;}
static const twatch_rtc_api_v1 clock_api={.api_version=2,.struct_size=sizeof(clock_api),.read=clock_read};
static const risc_touch_api_v1 t={1,sizeof(t),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot};
#if defined(NOVA_WATCH_CATALOG) || defined(TEST_NATIVE_TIME)
static bool battery_read(void*c,risc_battery_sample_v1*out){(void)c;*out=(risc_battery_sample_v1){.millivolts=4000,.percent=80};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,battery_read};
#endif
#ifdef NOVA_WATCH_CATALOG
static int32_t catalog_snapshot(void*c,risc_app_catalog_snapshot_v1*out){(void)c;out->count=CATALOG_COUNT;out->generation=1;return 0;}
static int32_t catalog_get(void*c,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1*out){
 (void)c;assert(generation==1);if(index>=CATALOG_COUNT)return RISC_APP_CATALOG_NOT_FOUND;
 const t5_app_manifest_t*m=&portable_catalog[index];out->flags=RISC_APP_CATALOG_AVAILABLE;
 strcpy(out->display_name,m->display_name);strcpy(out->file_name,m->file_name);strcpy(out->icon,m->icon);snprintf(out->id,sizeof(out->id),"app-%u",index);return 0;
}
static const risc_app_catalog_v1 catalog_api={1,sizeof(catalog_api),NULL,catalog_snapshot,catalog_get};
static bool exit_requested(void){return script_step()>=350;}
static bool retain(void){assert(!"unexpected retained fixture invocation");return true;}
#endif
#ifdef TEST_NATIVE_TIME
static unsigned native_reads,zone_reads;
static int32_t native_read(void *context,risc_realtime_snapshot_v1 *out){
 assert(context==&native_reads);++native_reads;
 *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_VALID,
  .epoch_seconds=1767268800+ms/1000,.monotonic_before_us=(uint64_t)ms*1000,.monotonic_after_us=(uint64_t)ms*1000};
 if(scenario==40){out->validity=RISC_REALTIME_UNSET;out->epoch_seconds=0;}
 return RISC_REALTIME_OK;
}
static int32_t zone_get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *size){
 (void)context;++zone_reads;assert(!strcmp(key,"time_zone"));*size=0;
 if(scenario==41)return RISC_KEY_VALUE_IO;
 if(scenario==42){
  uint8_t record[44]={'T','Z',1,0};memcpy(record+4,"America/Denver",15);
  uint8_t sum=0xa5;for(unsigned i=0;i<44;++i)if(i!=3)sum^=record[i];record[3]=sum;
  assert(capacity>=sizeof(record));memcpy(out,record,sizeof(record));*size=sizeof(record);return RISC_KEY_VALUE_OK;
 }
 return RISC_KEY_VALUE_NOT_FOUND;
}
static const risc_realtime_api_v1 native_api={1,sizeof(native_api),&native_reads,native_read};
static const risc_key_value_v1 zone_api={.api_version=1,.struct_size=sizeof(zone_api),.get=zone_get};
static bool native_exit(void){return script_step()>=350;}
static bool native_retain(void){assert(!"unexpected native time custody retention");return true;}
#endif
static bool acquire(const char *name,uint32_t v,uint64_t id,risc_runtime_capability_v1 *g){assert(g->struct_size==sizeof(*g));
#ifdef TEST_NATIVE_TIME
 g->slot=grants+1;g->generation=1;
 if(!strcmp(name,"board.battery")){assert(id==0&&v==1);g->api=&battery_api;++grants;return true;}
 if(!strcmp(name,"storage.key-value")){assert(id==1&&v==1);g->api=&zone_api;++grants;return true;}
 if(!strcmp(name,"runtime.realtime")){assert(id==0&&v==1);g->api=&native_api;++grants;return true;}
 assert(strcmp(name,"runtime.realtime-control")&&strcmp(name,"rtc.clock"));
#endif
 assert(!id);
#ifdef NOVA_WATCH_CATALOG
 g->slot=grants+1;g->generation=1;
 if(!strcmp(name,"board.battery")){g->api=&battery_api;++grants;return true;}
 if(!strcmp(name,"app.catalog")){g->api=&catalog_api;++grants;return true;}
#endif
 if(!strcmp(name,"rtc.clock")){assert(v==2);g->api=&clock_api;++grants;return true;}assert(v==1);if(!strcmp(name,"display.output"))g->api=&d;else if(!strcmp(name,"input.touch.raw"))g->api=&t;else return false;++grants;return true;}
static bool release(risc_runtime_capability_v1 *g){assert(grants&&g->api);--grants;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static unsigned diag_count;static uint32_t diag_words[14];
static uint32_t trace(uint32_t id,uint32_t phase,uint32_t value){
 if(phase==8&&(value>>24)>=0xe0&&(value>>24)<=0xe4){assert(diag_count<14);diag_words[diag_count++]=value;}
 return id;
}
static void check_diagnostic(void){
 if(TEST_DIAG_OFF||getenv("TEST_DIAG_GAP")){assert(!diag_count);return;}assert(diag_count==14);assert((diag_words[0]>>24)==0xe0);assert((diag_words[1]>>24)==0xe1);
 for(unsigned i=2;i<diag_count;i+=3){unsigned raw=diag_words[i],mapped=diag_words[i+1],delta=diag_words[i+2];assert((raw>>24)==0xe2&&(mapped>>24)==0xe3&&(delta>>24)==0xe4);unsigned px=(raw>>10)&1023,py=raw&1023,x=(mapped>>10)&1023,y=mapped&1023;assert((PORTABLE_TOUCH_ROTATION==180?465-px:px)==x&&(PORTABLE_TOUCH_ROTATION==180?465-py:py)==y);int dx=(delta>>10)&1023,dy=delta&1023;if(dx&512)dx-=1024;if(dy&512)dy-=1024;unsigned slot=(raw>>22)&3;if(slot==1||slot==2){int d=direction<2?dx:dy;assert((direction<2?dy:dx)==0);int wanted=((direction^(PORTABLE_TOUCH_ROTATION==180))&1)?-1:1;if(slot==2)wanted=-wanted;assert(d*wanted>0);}printf("GESTURE_RECORD %08x %08x %08x\n",raw,mapped,delta);}
 puts("PASS actual decoded CST to mapped contact to dispatched Nova delta diagnostic");
}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=acquire,.release=release,.trace=trace
#ifdef NOVA_WATCH_CATALOG
 ,.retain_invocation=retain,.exit_requested=exit_requested
#elif defined(TEST_NATIVE_TIME)
 ,.retain_invocation=native_retain,.exit_requested=native_exit
#endif
 };
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&rt:NULL;}
int main(int argc,char **argv){
     if(getenv("AUDIT_PERIOD_MS")){report_period=(unsigned)atoi(getenv("AUDIT_PERIOD_MS"));}assert(report_period>=5);report_jitter=getenv("AUDIT_JITTER")!=NULL;report_repeated=getenv("AUDIT_REPEATED")!=NULL;direction=(unsigned)atoi(getenv("DIRECTION"));assert(direction<4);assert(argc==3||argc==6);if(argc==6){bus_us=(unsigned)atoi(argv[3]);panel_ms=(unsigned)atoi(argv[4]);yield_extra_ms=(unsigned)atoi(argv[5]);}cost_ns=(unsigned)atoi(argv[1]);read_ms=(unsigned)atoi(argv[2]);scenario=99;
     real_touch=bridge_start(&ms);assert(app_module_init()==0);app_main();app_module_fini();assert(!frames&&!grants&&!subs);bridge_stop();printf("PARITY frames=%u image_signature=%016llx polls=%u events=%u launches=%u\n",presents,(unsigned long long)frame_signature,polls,events_taken,launches);check_diagnostic();if(getenv("TEST_DIAG_GAP")){assert(gaps&&cancellations&&settled_checks&&!launches);}else assert(positive_seen&&negative_seen&&visible_positive&&visible_negative&&settled_checks&&!launches);
     printf("{\"pixel_cost_ns\":%u,\"poll_cost_ms\":%u,\"elapsed_ms\":%u,\"render_pixel_ms\":%.3f,\"poll_ms\":%llu,\"polls\":%u,\"frames\":%u,\"max_completed_frame_ms\":%u,\"max_raw_poll_gap_ms\":%u,\"events_taken\":%u,\"events_emitted\":%u,\"max_dispatch_age_ms\":%u,\"max_queue\":%u,\"queue_gaps\":%u,\"gap_cancellations\":%u,\"launches\":%u}\n",cost_ns,read_ms,ms,work_ns_total/1000000.0,poll_ms_total,polls,presents,max_complete,max_touch_gap,events_taken,events_emitted,max_age,max_queue,gaps,cancellations,launches);
    }
    