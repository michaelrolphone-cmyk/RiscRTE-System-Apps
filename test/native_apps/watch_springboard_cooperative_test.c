#include "fixtures/installed_catalog_provider.h"
/* Real Springboard + adapter + exact deployed cooperative display state machine.
 * Hardware row cost and CPU-work clock are explicit host test parameters. */
#define _POSIX_C_SOURCE 200809L
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscRuntimeV1.h"
#include "RiscTouchV1.h"
#include "RiscProviderV2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define WIDTH 240u
#define HEIGHT 240u
#define STRIDE 480u
#define FORMAT RISC_DISPLAY_FORMAT_RGB565
#define DISPLAY_FLAGS RISC_DISPLAY_INFO_ASYNC_PRESENT
#define DISPLAY_ID "twatch-panel"
static unsigned long long now_us;
static unsigned yield_calls,panel_polls,rows_written,completed,renders,checkpoints,raw_polls;
static unsigned work_us=512,poll_period=1,inside_paint,pumps_in_paint;
static unsigned last_submit,max_submit_gap,submits,completed_before_stop;
static unsigned max_paint_us,paint_start,stop_ms=4000;
static unsigned raster_health_reads,raster_clock_reads;
static double host_paint_ns;
static struct timespec host_start;
static uint32_t spi_timeout_ms;
static uint8_t pixels[STRIDE*HEIGHT],accepted_pixels[STRIDE*HEIGHT];
static uint64_t clock_now(void*c){(void)c;return now_us/1000;}
static void clock_sleep(void*c,uint32_t n){(void)c;now_us+=n*1000u;}
static const struct {void*context;uint64_t(*monotonic_ms)(void*);void(*sleep_ms)(void*,uint32_t);} clock_api={NULL,clock_now,clock_sleep};
static const __typeof__(clock_api)*timer=&clock_api;
static bool hw_start(const risc_provider_dependency_v1*d,size_t n){(void)d;(void)n;return true;}
static void hw_submit(void){assert(!inside_paint);memcpy(accepted_pixels,pixels,sizeof(pixels));unsigned now=(unsigned)(now_us/1000);if(submits && now-last_submit>max_submit_gap)max_submit_gap=now-last_submit;last_submit=now;++submits;}
static uint32_t hw_row_min_budget_ms(void){return 1;}
static bool hw_row(uint32_t y,const uint8_t*p){assert(y<HEIGHT&&p==pixels+y*STRIDE);assert(!memcmp(pixels,accepted_pixels,sizeof(pixels)));assert(spi_timeout_ms>=2);++rows_written;now_us+=100;return true;}
static int hw_finish(void){++completed;return 1;}
static bool hw_brightness(uint16_t a,uint16_t b){(void)a;(void)b;return true;}
static bool hw_stop(void){return true;}
#include "watch_cooperative_display_fixture.h"
static bool info_for_nova(void*c,risc_display_info_v1*out){bool ok=get_info(c,out);out->nominal_refresh_millihz=20000;return ok;}
static risc_display_output_api_v1 output;
void watch_probe_render_begin(void){inside_paint=1;paint_start=(unsigned)now_us;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&host_start);++renders;}
void watch_probe_render_end(void){struct timespec end;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&end);host_paint_ns+=(end.tv_sec-host_start.tv_sec)*1e9+end.tv_nsec-host_start.tv_nsec;unsigned cost=(unsigned)now_us-paint_start;if(cost>max_paint_us)max_paint_us=cost;inside_paint=0;}
void watch_probe_work(void){++checkpoints;now_us+=work_us;}
void watch_probe_after_poll(t5_app_input_t*in){if(now_us/1000>=stop_ms){in->exit_requested=true;completed_before_stop=completed;}}
static bool runtime_monotonic(uint32_t*out){if(inside_paint)++raster_clock_reads;*out=(uint32_t)(now_us/1000);return true;}
static bool runtime_health(risc_runtime_health_v1*h){if(inside_paint)++raster_health_reads;h->uptime_ms=(uint32_t)(now_us/1000);return true;}
static void runtime_yield(uint32_t n){++yield_calls;if(inside_paint)++pumps_in_paint;if(yield_calls%poll_period==0){++panel_polls;display_poll(8);}now_us+=(n?n:1)*1000u;}
static bool diagnostic(const char*s){(void)s;return true;}
static bool launch(const char*s){(void)s;assert(!"no launch allowed");return false;}
static bool retain(void){assert(!"healthy probe cannot retain");return false;}
static uint64_t subscribe(void*c){(void)c;return 1;}
static bool unsubscribe(void*c,uint64_t n){(void)c;return n==1;}
static bool touch_poll(void*c,size_t n){(void)c;(void)n;++raw_polls;return true;}
static int32_t touch_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;(void)e;return 0;}
static bool touch_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=(risc_touch_snapshot_v1){.width=240,.height=240};unsigned ms=(unsigned)(now_us/1000);if(ms>=200 && ms<3800){unsigned phase=(ms-200)%1200;unsigned x=phase<600?50+phase/5:170-(phase-600)/5;s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=120};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,subscribe,unsubscribe,touch_poll,touch_next,touch_snapshot};
static bool runtime_acquire(const char*s,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)v;(void)id;if(!strcmp(s,RISC_APP_CATALOG_CAPABILITY))g->api=&fixture_catalog_api;else if(!strcmp(s,"display.output"))g->api=&output;else if(!strcmp(s,"input.touch.raw"))g->api=&touch_api;else return false;return true;}
static bool runtime_release(risc_runtime_capability_v1*g){g->api=NULL;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=runtime_health,.yield_ms=runtime_yield,.diagnostic=diagnostic,.request_launch=launch,.acquire=runtime_acquire,.release=runtime_release,.retain_invocation=retain,.monotonic_ms=runtime_monotonic};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
const t5_app_manifest_t portable_catalog[]={

 {.display_name="Clock",.file_name="clock.elf",.icon="solid:f017",.compatible=true},
 {.display_name="Battery",.file_name="battery.elf",.icon="solid:f240",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 {.display_name="Calculator",.file_name="calculator.elf",.icon="solid:f1ec",.compatible=true},
 {.display_name="Stopwatch",.file_name="stopwatch.elf",.icon="solid:f2f2",.compatible=true},
 {.display_name="Alarms",.file_name="alarms.elf",.icon="solid:f0f3",.compatible=true},
 {.display_name="Countdown",.file_name="countdown.elf",.icon="solid:f254",.compatible=true},
 {.display_name="Wi-Fi",.file_name="wifi_settings.elf",.icon="solid:f1eb",.compatible=true},
 {.display_name="Points in Time",.file_name="points_in_time.elf",.icon="solid:f783",.compatible=true},
 {.display_name="Frequency Generator",.file_name="frequency_generator.elf",.icon="solid:f028",.compatible=true},
 {.display_name="Audio Spectrum",.file_name="audio_spectrum.elf",.icon="solid:f130",.compatible=true},
 {.display_name="File Browser",.file_name="file_browser.elf",.icon="solid:f07c",.compatible=true},
 {.display_name="BLE Scanner",.file_name="ble_scanner.elf",.icon="solid:f7c0",.compatible=true},
 {.display_name="LoRa Messages",.file_name="lora_messages.elf",.icon="solid:f27a",.compatible=true},
 {.display_name="Clock",.file_name="clock.elf",.icon="solid:f017",.compatible=true},
 {.display_name="Battery",.file_name="battery.elf",.icon="solid:f240",.compatible=true},
 {.display_name="Settings",.file_name="settings.elf",.icon="solid:f013",.compatible=true},
 {.display_name="Calculator",.file_name="calculator.elf",.icon="solid:f1ec",.compatible=true},
 {.display_name="Stopwatch",.file_name="stopwatch.elf",.icon="solid:f2f2",.compatible=true},
 {.display_name="Alarms",.file_name="alarms.elf",.icon="solid:f0f3",.compatible=true}};
const unsigned portable_catalog_count=19;
void app_main(void);int app_module_init(void);void app_module_fini(void);
int main(int argc,char**argv){assert(argc==3);work_us=(unsigned)atoi(argv[1]);poll_period=(unsigned)atoi(argv[2]);output=api;output.get_info=info_for_nova;assert(start(NULL,0));assert(app_module_init()==0);app_main();app_module_fini();assert(!active&&!held);printf("{\"work_us_per512\":%u,\"panel_visit_every_yields\":%u,\"renders\":%u,\"submits\":%u,\"completed_before_stop\":%u,\"max_submit_gap_ms\":%u,\"yield_calls\":%u,\"paint_yields\":%u,\"raster_checkpoints\":%u,\"raw_polls\":%u,\"panel_polls\":%u,\"rows_written\":%u,\"max_paint_us\":%u,\"raster_health_reads\":%u,\"raster_clock_reads\":%u,\"host_cpu_mean_paint_us\":%.3f}\n",work_us,poll_period,renders,submits,completed_before_stop,max_submit_gap,yield_calls,pumps_in_paint,checkpoints,raw_polls,panel_polls,rows_written,max_paint_us,raster_health_reads,raster_clock_reads,host_paint_ns/1000/renders);}
