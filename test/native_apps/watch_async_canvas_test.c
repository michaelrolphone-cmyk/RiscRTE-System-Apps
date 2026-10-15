#define _POSIX_C_SOURCE 200809L
#include "fixtures/installed_catalog_provider.h"
/* Opt-in async adapter regression: one provider-owned token, one replaceable
 * complete app image, and no ownership release after uncertain completion.
 * These providers model selected Watch semantics, not a physical panel. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
/* A refused runtime fence must park without a callback, allocation or free.
 * Only the forked child's bounded POSIX timer may interrupt that park. */
static volatile sig_atomic_t refused_retain_seen;
static void no_call_after_refusal(void) {if(refused_retain_seen)_exit(72);}
static void parked_alarm(int signal_number) {(void)signal_number;_exit(refused_retain_seen?0:73);}
static unsigned allocations, frees,allocation_attempts,fail_allocation;
static void *counted_malloc(size_t size) {no_call_after_refusal();if(++allocation_attempts==fail_allocation)return NULL;void *p=malloc(size);if(p)++allocations;return p;}
static void counted_free(void *p) {no_call_after_refusal();if(p)++frees;free(p);}
#define malloc counted_malloc
#define free counted_free
#include "../../lib/PortableApps/src/adapter.c"
#undef malloc
#undef free
const t5_app_manifest_t portable_catalog[]={{.display_name="Clock",.file_name="clock.elf",.compatible=true}};
const unsigned portable_catalog_count=1;
static uint16_t provider_pixels[240*243],owned_snapshot[240*243],outgoing[240*240];
static unsigned now_ms,flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE;
static unsigned grants,subscriptions,leases,acquires,releases,submits,statuses,inputs,launches,retains;
static unsigned yields,health_reads,clock_reads;
static unsigned provider_yield_ms;

static bool clock_failed;
static unsigned latency=40,submitted_at,status_error,damage_count;
static risc_display_rect_v1 last_damage;
static bool provider_busy,finish_now,reject_submit,zero_token,bad_surface,exit_pending,fail_after_yield;
static bool retain_ok=true,retention_seen;
#ifdef PORTABLE_STAGE_PROFILE
static unsigned trace_calls,trace_phases[9];
static uint32_t trace_stage(uint32_t id,uint32_t phase,uint32_t value) {
 no_call_after_refusal();assert(!retention_seen && id==0 && phase>=4 && phase<=8);
 if(phase==8)assert((value>>24)>=0x40u && (value>>24)<=0x4eu);
 trace_calls++;trace_phases[phase]++;return 0;
}
#endif
static void immutable(void) {if(provider_busy)assert(!memcmp(provider_pixels,owned_snapshot,sizeof(provider_pixels)));}
static void normal_io(void) {no_call_after_refusal();assert(!retention_seen);immutable();}
static bool health(risc_runtime_health_v1 *h){no_call_after_refusal();assert(!retention_seen);++health_reads;h->uptime_ms=now_ms;return true;}
static bool monotonic(uint32_t*out){no_call_after_refusal();assert(!retention_seen);++clock_reads;*out=now_ms;return !clock_failed;}
static void yield_ms(uint32_t ms){no_call_after_refusal();immutable();++yields;now_ms+=ms+provider_yield_ms;if(fail_after_yield)status_error=RISC_DISPLAY_PRESENT_FAILED;}
static bool diagnostic(const char *s){no_call_after_refusal();assert(s);return true;}
static bool native_exit(void){no_call_after_refusal();return exit_pending;}
static bool retain(void){
 no_call_after_refusal();++retains;retention_seen=true;
 if(!retain_ok){assert(failed&&canvas_uncertain&&!canvas_retained&&retains==1&&grants&&canvas_pixels);refused_retain_seen=1;}
 return retain_ok;
}
static bool launch_app(const char *s){normal_io();assert(!provider_busy&&!leases);assert(!strcmp(s,"clock.elf"));++launches;return true;}
static bool get_info(void*c,risc_display_info_v1*out){(void)c;no_call_after_refusal();*out=(risc_display_info_v1){.width=240,.height=240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=flags,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000};return true;}
static bool acquire_frame(void*c,uint32_t format,risc_display_surface_v1*out){
 (void)c;normal_io();assert(!provider_busy&&!leases);++acquires;++leases;
 *out=(risc_display_surface_v1){.frame=1,.pixels=provider_pixels,.width=bad_surface?239:240,.height=240,.stride_bytes=486,.size_bytes=sizeof(provider_pixels),.pixel_format=format};return true;
}
static void release_frame(void*c,risc_display_frame_v1 frame){(void)c;normal_io();assert(frame==1&&leases==1&&!provider_busy);--leases;++releases;}
static bool submit_frame(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1*damage,size_t count,const risc_display_present_options_v1*options,risc_display_present_token_v1*token){
 (void)c;normal_io();assert(frame==1&&leases==1&&!provider_busy);assert(options&&options->queue_policy==RISC_DISPLAY_QUEUE_FIFO);assert(count<=1);damage_count=(unsigned)count;if(count){assert(damage);last_damage=*damage;}
 if(canvas_async)assert(!surface.frame); /* Only sealed, complete images cross. */
 if(reject_submit)return false;
 --leases;++submits;provider_busy=true;submitted_at=now_ms;*token=zero_token?0:submits;
 memcpy(owned_snapshot,provider_pixels,sizeof(provider_pixels));return true;
}
static bool present_status(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){
 (void)c;normal_io();assert(token==submits&&provider_busy);++statuses;
 if(status_error==99)return false;
 if(status_error){out->state=(uint8_t)status_error;return true;}
 if(finish_now || (uint32_t)(now_ms-submitted_at)>=latency){out->state=RISC_DISPLAY_PRESENT_COMPLETE;provider_busy=false;finish_now=false;}
 else out->state=(now_ms==submitted_at)?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_ACTIVE;
 return true;
}
static uint64_t subscribe(void*c){(void)c;normal_io();++subscriptions;return 1;}
static bool unsubscribe(void*c,uint64_t s){(void)c;normal_io();assert(s==1&&subscriptions);--subscriptions;return true;}
static bool poll_touch(void*c,size_t n){(void)c;normal_io();assert(n==1&&!leases&&!status_error);++inputs;return true;}
static risc_touch_event_v1 edges[12];
static unsigned edge_at,edge_count;
static bool touch_terminal;
static int32_t next_touch(void*c,uint64_t s,risc_touch_event_v1*out){(void)c;(void)s;normal_io();if(touch_terminal)return -2;if(edge_at<edge_count){*out=edges[edge_at++];return 1;}return 0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;normal_io();*out=(risc_touch_snapshot_v1){.width=240,.height=240,.sequence=edge_count,.timestamp_ms=edge_count};return true;}
static const risc_display_output_api_v1 provider={.api_version=1,.struct_size=sizeof(provider),.get_info=get_info,.acquire=acquire_frame,.release=release_frame,.submit=submit_frame,.present_status=present_status};
static const risc_touch_api_v1 touch_provider={1,sizeof(touch_provider),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot};
static bool acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){normal_io();assert(version==1&&!instance);if(!strcmp(name,RISC_APP_CATALOG_CAPABILITY))g->api=&fixture_catalog_api;else if(!strcmp(name,"display.output"))g->api=&provider;else if(!strcmp(name,"input.touch.raw"))g->api=&touch_provider;else return false;++grants;return true;}
static bool release(risc_runtime_capability_v1*g){normal_io();assert(grants&&g->api&&!provider_busy&&!leases);--grants;g->api=NULL;return true;}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=acquire,.release=release,.exit_requested=native_exit,.retain_invocation=retain};
static const risc_runtime_api_v1 *runtime_source=&runtime;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){no_call_after_refusal();return version==1?runtime_source:NULL;}
static void init(void){
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<243;x++){provider_pixels[y*243+x]=x<240?(uint16_t)(x*73+y*19):0xa55a;if(x<240)outgoing[y*240+x]=provider_pixels[y*243+x];}
 assert(app_module_init()==0);
}
static void draw(uint16_t color){clear_color(color);assert(!failed);}
static void paint(uint16_t color){draw(color);present(false);}
static void check_color(uint16_t color){for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)assert(provider_pixels[y*243+x]==color);}
static void finish(void){app_module_fini();assert(!grants&&!subscriptions&&!leases&&!provider_busy);assert(allocations==frees&&!canvas_pixels&&!previous_pixels);for(unsigned y=0;y<240;y++)for(unsigned x=240;x<243;x++)assert(provider_pixels[y*243+x]==0xa55a);}
static void check_retained(void){
 assert(failed&&canvas_uncertain&&canvas_retained&&retains==1&&retention_seen);
 unsigned old_statuses=statuses,old_frees=frees,old_grants=grants,old_submits=submits;
 assert(!canvas_progress());app_module_fini();app_module_fini();
 assert(retains==1&&statuses==old_statuses&&frees==old_frees&&grants==old_grants&&submits==old_submits&&grants&&canvas_pixels);
 /* Test-only teardown after proving production retained every owned resource. */
 free(canvas_pixels);canvas_pixels=NULL;free(previous_pixels);previous_pixels=NULL;free(handoff_old);handoff_old=NULL;free(handoff_scratch);handoff_scratch=NULL;
}
int main(int argc,char**argv){
 assert(argc==2);const char*test=argv[1];static void *short_runtime;
 if(!strcmp(test,"fallback-no-async"))flags&=~RISC_DISPLAY_INFO_ASYNC_PRESENT;
 if(!strcmp(test,"fallback-retains-image"))flags|=RISC_DISPLAY_INFO_RETAINS_IMAGE;
 if(!strcmp(test,"fallback-no-retain"))runtime.retain_invocation=NULL;
 if(!strcmp(test,"fallback-short-runtime")){runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;short_runtime=malloc(runtime.struct_size);assert(short_runtime);memcpy(short_runtime,&runtime,runtime.struct_size);runtime_source=short_runtime;}
 if(!strncmp(test,"clock-",6))runtime.monotonic_ms=monotonic;
 if(!strcmp(test,"clock-missing"))runtime.monotonic_ms=NULL;
 if(!strcmp(test,"clock-truncated")){
  runtime.struct_size=RISC_RUNTIME_MONOTONIC_V1_SIZE-1;
  short_runtime=malloc(runtime.struct_size);assert(short_runtime);
  memcpy(short_runtime,&runtime,runtime.struct_size);runtime_source=short_runtime;
 }
 if(!strcmp(test,"canvas-allocation-failure")){
  fail_allocation=1;assert(app_module_init()!=0);assert(allocation_attempts==1&&!allocations&&!frees&&!grants&&!subscriptions&&!leases&&!acquires&&!submits&&!canvas_pixels);
  puts("Async canvas: canvas-allocation-failure PASS");return 0;
 }
#ifdef PORTABLE_STAGE_PROFILE
 if(!strncmp(test,"stage-profile-",14)){runtime.monotonic_ms=monotonic;runtime.trace=trace_stage;}
#endif
 init();
#ifdef PORTABLE_STAGE_PROFILE
 if(!strcmp(test,"stage-profile-bounded")){
  for(unsigned i=0;i<128;i++){now_ms+=40;paint((uint16_t)(i+1));finish_now=true;assert(canvas_progress());}
  assert(trace_calls==33 && trace_phases[4]==1 && trace_phases[5]==1 && trace_phases[6]==1 && trace_phases[8]==30);finish();assert(trace_phases[8]==45);
 }else if(!strcmp(test,"stage-profile-short")){
  paint(0x1234);finish_now=true;assert(canvas_progress());assert(trace_phases[8]==0);
  now_ms=2100;t5_app_input_t in={0};assert(poll_input(&in,1));
  assert(trace_phases[8]==15);finish();
 }else if(!strcmp(test,"stage-profile-retained")){
  paint(0x1234);status_error=RISC_DISPLAY_PRESENT_FAILED;assert(!canvas_progress());
  unsigned before=trace_calls;stage_profile_render_begin();stage_profile_finish(STAGE_POLL,0);
  check_retained();assert(trace_calls==before);
 }else if(!strcmp(test,"stage-profile-clock-failure")){
  assert(stage_profile.enabled);clock_failed=true;stage_profile_finish(STAGE_POLL,0);
  assert(!stage_profile.enabled && !failed);clock_failed=false;finish();
 }else
#endif
 if(!strncmp(test,"clock-",6)){
  assert(canvas_async);health_reads=clock_reads=0;
  paint(0x1111);
  if(!strcmp(test,"clock-failure")){
   clock_failed=true;assert(!canvas_progress());assert(clock_reads&&!health_reads);check_retained();
  }else{
   now_ms+=CANVAS_PUMP_INTERVAL_MS;draw(0x2222);present(false);finish();
   if(!strcmp(test,"clock-monotonic"))assert(clock_reads&&!health_reads);
   else assert(health_reads&&!clock_reads);
  }
 }
 else if(!strcmp(test,"poll-wait-provider-cost") || !strcmp(test,"poll-wait-provider-cost-wrap")){
  /* Exact owner-task semantics: a provider step precedes the requested sleep.
   * Its 2ms synthetic cost is separate from the 1ms requested scheduler tick. */
  if(!strcmp(test,"poll-wait-provider-cost-wrap"))now_ms=UINT32_MAX-10u;
  runtime.monotonic_ms=monotonic;latency=1000;paint(0x1234);
  provider_yield_ms=2;last_poll_at=now_ms;
  unsigned start=now_ms,before=yields;t5_app_input_t in={0};
  assert(poll_input(&in,20));
  assert(now_ms-start==21 && yields-before==7);
  assert(inputs==1 && canvas_token && provider_busy);
  provider_yield_ms=0;finish_now=true;finish();
 }
 else if(!strncmp(test,"fallback-",9)){assert(!canvas_async&&!canvas_pixels);paint(0x1234);assert(!provider_busy&&submits==1&&now_ms>=latency);finish();}
 else if(!strcmp(test,"mailbox")){
  assert(canvas_async&&canvas_pixels&&previous_pixels);paint(0x1111);assert(submits==1&&provider_busy&&!leases&&!surface.frame);
  paint(0x2222);paint(0x3333);assert(submits==1&&canvas_sealed);check_color(0x1111);
  finish_now=true;assert(canvas_progress());assert(submits==2&&provider_busy&&!canvas_sealed);check_color(0x3333);
  finish_now=true;assert(canvas_progress());paint(0x3333);assert(submits==2&&!canvas_sealed&&!provider_busy);finish();
 }else if(!strcmp(test,"mid-paint")){
  paint(0x1111);paint(0x2222);draw(0x3333);fill(0,0,240,120,0x4444);finish_now=true;
  unsigned before=submits;assert(canvas_progress());assert(!provider_busy&&submits==before&&!leases);check_color(0x1111);
  input_service();assert(submits==before&&surface.frame==CANVAS_DRAWING_FRAME);
  fill(0,120,240,120,0x4444);present(false);assert(submits==before+1);check_color(0x4444);finish();
 }else if(!strcmp(test,"raster-pump")||!strcmp(test,"raster-pump-complete")||!strcmp(test,"raster-pump-failure")){
  paint(0x1111);unsigned before=yields;
  now_ms+=CANVAS_PUMP_INTERVAL_MS-1;clear_color(0x2222);assert(yields==before);
  now_ms+=1;
  if(!strcmp(test,"raster-pump-complete"))latency=CANVAS_PUMP_INTERVAL_MS+1;
  if(!strcmp(test,"raster-pump-failure"))fail_after_yield=true;
  fill(0,0,240,240,0x3333);assert(yields==before+1&&submits==1);
  if(fail_after_yield)check_retained();
  else {
   assert(surface.frame==CANVAS_DRAWING_FRAME&&!canvas_sealed);
   check_color(0x1111); /* Mid-raster pumping never copies partial output. */
   if(latency==CANVAS_PUMP_INTERVAL_MS+1)assert(!canvas_token&&!provider_busy);
   else assert(canvas_token&&provider_busy);
   fill(0,0,240,240,0x4444);assert(yields==before+1);
   present(false);finish();assert(submits==2);check_color(0x4444);
  }
 }else if(!strcmp(test,"touch-terminal")){
  paint(0x1111);touch_terminal=true;
  portable_touch_sample sample;input_take(&sample);
  assert(failed&&optional_services_retained&&canvas_retained&&retains==1);
  unsigned old_grants=grants,old_frees=frees;app_module_fini();
  assert(grants==old_grants&&frees==old_frees&&provider_busy);
 }else if(!strncmp(test,"ordered-taps-",13)){
  latency=(unsigned)atoi(test+13);paint(0x1111);
  for(unsigned i=0;i<12;i++)edges[i]=(risc_touch_event_v1){.sequence=i+1,.timestamp_ms=i+1,.kind=i%2?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN,.id=0,.x=60,.y=80};
  edge_count=12;
  /* Capturing while the submitted frame drains cannot consume logical keys. */
  for(unsigned i=0;i<8;i++)input_capture();
  for(unsigned i=0;i<12;i++){
   portable_touch_sample sample;input_take(&sample);
   assert(sample.valid&&!sample.cancelled&&sample.tap_eligible);
   assert(i%2?sample.released:sample.began);
   assert(sample.x==60&&sample.y==80);
   paint((uint16_t)(0x2220+i));
  }
  finish();check_color(0x222b);
 }else if(!strcmp(test,"damage")){
  paint(0x1234);finish_now=true;assert(canvas_progress());draw(0x1234);fill(0,10,240,3,0x5678);present(false);
  assert(submits==2&&damage_count==1&&last_damage.x==0&&last_damage.y==10&&last_damage.width==240&&last_damage.height==3);finish();
 }else if(!strcmp(test,"fini-pending")){paint(0x1111);paint(0x2222);finish();assert(submits==2&&now_ms>=2*latency);check_color(0x2222);
 }else if(!strcmp(test,"fini-drawing")){paint(0x1111);draw(0x2222);finish();assert(submits==1);check_color(0x1111);
 }else if(!strcmp(test,"crown")){paint(0x1111);paint(0x2222);exit_pending=true;t5_app_input_t in={0};assert(poll(&in,8));assert(in.exit_requested&&(in.buttons&T5_APP_BUTTON_BACK)&&!launches&&!provider_busy&&submits==2);finish();
 }else if(!strcmp(test,"launch")){paint(0x1111);paint(0x2222);assert(launch(0)&&launches==1&&!provider_busy&&submits==2);finish();
 }else if(!strcmp(test,"handoff")){
  assert(springboard_presentation_get()&&handoff_pending);draw(0x07e0);assert(handoff_active&&acquires==1&&releases==1&&!leases);
  for(unsigned y=0;y<240;y++)assert(!memcmp(handoff_old+y*240,outgoing+y*240,480));
  present(false);for(unsigned y=0;y<240;y++)assert(!memcmp(provider_pixels+y*243,outgoing+y*240,480));
  now_ms=180;paint(0x07e0);assert(!handoff_active);finish();check_color(0x07e0);
 }else if(!strcmp(test,"submit-rejected")||!strcmp(test,"invalid-surface")){
  reject_submit=!strcmp(test,"submit-rejected");bad_surface=!reject_submit;draw(0x1234);present(false);assert(failed&&!retains&&!leases&&!provider_busy&&releases==1);finish();
 }else if(!strcmp(test,"retain-refused")){
  paint(0x1234);pid_t child=fork();assert(child>=0);
  if(!child){
   struct sigaction action={0};action.sa_handler=parked_alarm;assert(!sigemptyset(&action.sa_mask));assert(!sigaction(SIGALRM,&action,NULL));
   status_error=RISC_DISPLAY_PRESENT_FAILED;retain_ok=false;alarm(1);
   (void)canvas_progress();_exit(74); /* Returning violates retained ownership. */
  }
  int result=0;pid_t waited;do{waited=waitpid(child,&result,0);}while(waited<0&&errno==EINTR);
  assert(waited==child&&WIFEXITED(result)&&WEXITSTATUS(result)==0);
  /* The parent's independent copy never failed and may clean up normally. */
  finish();
 }else{
  if(!strcmp(test,"timeout-wrap"))now_ms=UINT32_MAX-5000u;
  if(!strcmp(test,"zero-token"))zero_token=true;
  paint(0x1234);
  if(!strcmp(test,"failure-after-yield")){fail_after_yield=true;unsigned before=inputs;t5_app_input_t in={0};assert(!poll(&in,8));assert(inputs==before);}
  else if(!strcmp(test,"failure-fini-yield")){fail_after_yield=true;app_module_fini();}
  else if(!strcmp(test,"failure-crown-yield")){fail_after_yield=true;exit_pending=true;t5_app_input_t in={0};assert(!poll(&in,8)&&in.exit_requested);}
  else if(!strcmp(test,"timeout")||!strcmp(test,"timeout-wrap")){latency=20000;now_ms+=10000;assert(!canvas_progress());}
  else if(!strcmp(test,"zero-token")){assert(!canvas_progress());}
  else{status_error=!strcmp(test,"status-false")?99:!strcmp(test,"status-failed")?RISC_DISPLAY_PRESENT_FAILED:!strcmp(test,"status-superseded")?RISC_DISPLAY_PRESENT_SUPERSEDED:!strcmp(test,"status-unknown")?255:0;assert(status_error);assert(!canvas_progress());}
  check_retained();
 }
 free(short_runtime);
 printf("Async canvas: %s PASS\n",test);return 0;
}
