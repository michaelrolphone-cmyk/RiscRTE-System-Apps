/* Real Settings/Quick/alarm controllers over a delayed immutable provider.
 * Existing quick fixture supplies contact scripts and preference semantics. */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
static bool fixture_retained_fence;
static unsigned fixture_allocations,fixture_frees;
static void *fixture_malloc(size_t size){assert(!fixture_retained_fence);void*p=malloc(size);if(p)++fixture_allocations;return p;}
static void fixture_free(void*p){assert(!fixture_retained_fence);if(p)++fixture_frees;free(p);}
#define malloc fixture_malloc
#define free fixture_free
#define WATCH_ASYNC_CONTEXT_LAUNCH_FIXTURE
#define PORTABLE_QUICK_FIXTURE_MAIN legacy_quick_main
#include "quick_adapter_test.c"
#undef malloc
#undef free
static unsigned physical_submits,physical_statuses,physical_started,physical_latency=40;
static unsigned frame_releases,retain_calls,normal_calls,busy_service_calls,busy_inputs;
static unsigned failure_mode,crown_at_poll;
static bool physical_busy,physical_failure,retained_display,crown_pending;
#ifdef WATCH_ASYNC_STOPWATCH_FIXTURE
static bool stopwatch_crown_failure,stopwatch_paint_failure;
static unsigned stopwatch_acquires,stopwatch_releases;
void watch_test_stopwatch_main(void);
#endif
static uint16_t physical_snapshot[SETTINGS_FIXTURE_WIDTH*SETTINGS_FIXTURE_HEIGHT];
static void physical_immutable(void){if(physical_busy)assert(!memcmp(framebuffer,physical_snapshot,sizeof(framebuffer)));}
static void normal_allowed(void){physical_immutable();assert(!physical_failure&&!retained_display);++normal_calls;}
static bool async_acquire_frame(void*c,uint32_t f,risc_display_surface_v1*out){normal_allowed();assert(!physical_busy);return frame_acquire(c,f,out);}
static void async_release_frame(void*c,risc_display_frame_v1 f){normal_allowed();assert(!physical_busy);++frame_releases;frame_release(c,f);}
static bool async_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 normal_allowed();assert(!physical_busy&&!surface.frame&&o->queue_policy==RISC_DISPLAY_QUEUE_FIFO);
 bool ok=frame_submit(c,f,r,n,o,t);assert(ok);++physical_submits;physical_busy=true;physical_started=ticks;memcpy(physical_snapshot,framebuffer,sizeof(framebuffer));return true;
}
static bool async_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 (void)c;assert(t==displays&&physical_busy&&!retained_display);physical_immutable();++physical_statuses;
#ifdef WATCH_ASYNC_STOPWATCH_FIXTURE
 if(stopwatch_paint_failure){stopwatch_paint_failure=false;physical_failure=true;}
#endif
 if(physical_failure){out->state=RISC_DISPLAY_PRESENT_FAILED;return true;}
 if((uint32_t)(ticks-physical_started)>=physical_latency){physical_busy=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;}
 else out->state=RISC_DISPLAY_PRESENT_ACTIVE;
 return true;
}
static bool async_retain(void){assert(!retained_display&&(physical_failure||output_bad));++retain_calls;retained_display=true;fixture_retained_fence=true;return true;}
static bool async_exit(void){assert(!retained_display);
#ifdef WATCH_ASYNC_STOPWATCH_FIXTURE
 if(stopwatch_crown_failure){stopwatch_crown_failure=false;physical_failure=true;crown_pending=true;}
#endif
 return crown_pending;}
static bool async_runtime_health(risc_runtime_health_v1*out){assert(!retained_display);return quick_health(out);}
static bool async_runtime_release(risc_runtime_capability_v1*grant){normal_allowed();return test_release(grant);}
static bool async_runtime_diagnostic(const char*message){assert(!retained_display);return test_diagnostic(message);}
static bool async_runtime_launch(const char*path){normal_allowed();assert(!physical_busy);return quick_launch(path);}
static void async_yield(uint32_t ms){
 assert(!retained_display);physical_immutable();test_yield(ms);
 if(physical_busy&&((failure_mode==1)||(failure_mode==2&&quick_modal)||(failure_mode==3&&alarm_modal)||(failure_mode==4&&alarm_fake.state==ALARM_STATE_LOADING)||(failure_mode==5&&alarm_fake.state==ALARM_STATE_CUE)))physical_failure=true;
 if(((failure_mode==7&&alarm_modal)||(failure_mode==8&&alarm_fake.state==ALARM_STATE_LOADING)||(failure_mode==9&&alarm_fake.state==ALARM_STATE_CUE))&&!crown_pending){crown_pending=true;crown_at_poll=polls;}
 if(failure_mode==10&&ms==1&&ticks>=60000)crown_pending=true;
 if(failure_mode==6&&quick_modal){crown_pending=true;quick.ui.pending|=PQA_WIFI|PQA_VOLUME_COMMIT;quick.ui.action_volume=99;}
}
static bool async_touch_poll(void*c,size_t n){normal_allowed();if(failure_mode==7&&crown_pending)assert(polls<crown_at_poll+3);if(physical_busy)++busy_inputs;return touch_poll(c,n);}
static int32_t async_service_step(void*c){normal_allowed();if(physical_busy)++busy_service_calls;return quick_step(c);}
static int32_t async_service_status(void*c,alarm_status_v1*out){normal_allowed();return service_status(c,out);}
static int32_t async_service_refresh(void*c){normal_allowed();return service_refresh(c);}
static int32_t async_service_ack(void*c,const alarm_token_v1*t){normal_allowed();return service_ack(c,t);}
static int32_t async_service_stop(void*c){normal_allowed();return service_stop(c);}
static int32_t async_get(void*c,const char*k,void*d,uint32_t cap,uint32_t*s){normal_allowed();
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!strcmp(k,PORTABLE_CONTEXT_ENABLED_KEY)){*s=0;return RISC_KEY_VALUE_NOT_FOUND;}
#endif
 return quick_get(c,k,d,cap,s);}
static int32_t async_put(void*c,const char*k,const void*d,uint32_t s){normal_allowed();return quick_put(c,k,d,s);}
static bool async_brightness(void*c,uint16_t level,uint16_t max){normal_allowed();return quick_brightness(c,level,max);}
static risc_key_value_v1 async_kv;
static bool async_runtime_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 normal_allowed();if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(version==1&&instance==1);grant->api=&async_kv;++grants;return true;}
 return quick_acquire(name,version,instance,grant);
}
#ifdef WATCH_ASYNC_STOPWATCH_FIXTURE
static int32_t stopwatch_get(void*c,const char*key,void*data,uint32_t capacity,uint32_t*size){(void)c;(void)data;(void)capacity;normal_allowed();assert(!strcmp(key,"stopwatch"));*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t stopwatch_put(void*c,const char*key,const void*data,uint32_t size){(void)c;(void)key;(void)data;(void)size;assert(!"No Stopwatch save is requested");return RISC_KEY_VALUE_IO;}
static const risc_key_value_v1 stopwatch_store={1,sizeof(stopwatch_store),NULL,stopwatch_get,stopwatch_put};
static bool stopwatch_acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*grant){
 normal_allowed();assert(!instance);
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(version==1);grant->api=&stopwatch_store;}
 else{assert(!strcmp(name,"rtc.clock")&&version==2);grant->api=&rtc_api;}
 ++stopwatch_acquires;++grants;return true;
}
static bool stopwatch_release(risc_runtime_capability_v1*grant){normal_allowed();++stopwatch_releases;return test_release(grant);}
static risc_runtime_api_v1 stopwatch_runtime;
const risc_runtime_api_v1*watch_test_runtime_get_api(uint32_t version){assert(version==1);return &stopwatch_runtime;}
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
static unsigned context_captures,context_steps,context_pauses,workflow_claims,workflow_reports;
static bool complete_inside_paint;
static bool async_context_capture(void*c){(void)c;normal_allowed();assert(!frame_count);++context_captures;
 if(complete_inside_paint&&surface.frame==CANVAS_DRAWING_FRAME){complete_inside_paint=false;ticks+=physical_latency;unsigned before=physical_submits;assert(canvas_progress());assert(!physical_busy&&physical_submits==before&&surface.frame==CANVAS_DRAWING_FRAME);}
 return true;}
static bool async_context_pause(void*c){(void)c;normal_allowed();++context_pauses;return true;}
static bool async_context_step(void*c,const contexts_policy_v1*p){(void)c;(void)p;normal_allowed();++context_steps;return true;}
static bool async_context_status(void*c,contexts_status_v1*out){(void)c;normal_allowed();*out=(contexts_status_v1){.struct_size=sizeof(*out)};return true;}
static bool async_context_fingerprint(void*c,uint32_t operation,void*request){
 (void)c;normal_allowed();
 if(operation==CONTEXTS_FP_EVALUATE){contexts_evaluate_v1*q=request;assert(q->struct_size==sizeof(*q));q->result=(cr_result){0};return true;}
 if(operation==CONTEXTS_FP_WORKFLOW){
  contexts_workflow_v1*action=request;assert(action->struct_size==sizeof(*action));
  if(action->claim){++workflow_claims;action->available=true;action->step.kind=CR_LAUNCH;strcpy(action->step.text,"wifi_settings.elf");}
  else ++workflow_reports;
  return true;
 }
 if(operation==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*out=request;*out=(contexts_fingerprint_status_v1){.struct_size=sizeof(*out)};return true;}
 assert(operation==CONTEXTS_FP_CONFIG);return true;
}
static contexts_fingerprint_service_v1 async_fingerprint_api;
static const contexts_service_v1 async_context_api={.api_version=1,.struct_size=sizeof(async_context_api),.step=async_context_step,.pause=async_context_pause,.status=async_context_status,.capture_audio=async_context_capture};
#endif
static void async_setup(void){
 setup();quick_runtime.health=async_runtime_health;quick_runtime.release=async_runtime_release;quick_runtime.diagnostic=async_runtime_diagnostic;quick_runtime.request_launch=async_runtime_launch;quick_runtime.yield_ms=async_yield;quick_runtime.exit_requested=async_exit;quick_runtime.retain_invocation=async_retain;
 quick_display_api.acquire=async_acquire_frame;quick_display_api.release=async_release_frame;quick_display_api.submit=async_submit;quick_display_api.present_status=async_status;quick_display_api.set_brightness=async_brightness;
 quick_touch_api.poll=async_touch_poll;quick_alarm_api.step=async_service_step;quick_alarm_api.status=async_service_status;quick_alarm_api.refresh=async_service_refresh;quick_alarm_api.acknowledge=async_service_ack;quick_alarm_api.stop_only=async_service_stop;
 async_kv=quick_kv_api;async_kv.get=async_get;async_kv.put=async_put;quick_runtime.acquire=async_runtime_acquire;
#ifdef PORTABLE_CONTEXTS_CLIENT
 contexts_client.api=&async_context_api;contexts_client.runtime=&quick_runtime;contexts_client.loaded=true;
 contexts_client.grant=(risc_runtime_capability_v1){.struct_size=sizeof(contexts_client.grant),.api=&async_context_api};++grants;
#endif
 canvas_pixels=malloc(sizeof(framebuffer));assert(canvas_pixels);canvas_async=true;
 previous_pixels=malloc(sizeof(framebuffer));assert(previous_pixels);previous_valid=false;
 clear_color(0x1357);present(false);
 /* The restore image is newer than the still-owned physical image. */
 clear_color(0x2468);present(false);memcpy(expected_background,canvas_pixels,sizeof(expected_background));
 assert(physical_busy&&canvas_token&&!frame_count&&!surface.frame);
}
static void check_app_background(void){assert(canvas_drain());background_unchanged();assert(!memcmp(alarm_pixels,expected_background,sizeof(expected_background)));assert(busy_service_calls&&busy_inputs);}
static void retained_checks(void){
 assert(failed&&canvas_retained&&native_sleep_retained&&retained_display&&retain_calls==1);
 unsigned calls=normal_calls,states=physical_statuses,live=grants,freed=fixture_frees,allocated=fixture_allocations;void*p=canvas_pixels;
 app_module_fini();app_module_fini();assert(!canvas_progress());
 assert(fixture_frees==freed&&fixture_allocations==allocated&&normal_calls==calls&&physical_statuses==states&&grants==live&&live&&canvas_pixels==p&&retain_calls==1&&stop_calls==(output_bad?3u:0u)&&!return_launches&&!wifi_launches);
 /* Test-owned teardown after checking production retention. */
 free(canvas_pixels);canvas_pixels=NULL;free(previous_pixels);previous_pixels=NULL;free(alarm_pixels);alarm_pixels=NULL;free(quick_background);quick_background=NULL;
}
int main(int argc,char**argv){
 assert(argc==2);const char*test=argv[1];async_setup();
#ifdef WATCH_ASYNC_STOPWATCH_FIXTURE
 if(!strcmp(test,"stopwatch-crown-retain")||!strcmp(test,"stopwatch-paint-retain")){
  stopwatch_runtime=quick_runtime;stopwatch_runtime.acquire=stopwatch_acquire;stopwatch_runtime.release=stopwatch_release;
  stopwatch_crown_failure=!strcmp(test,"stopwatch-crown-retain");stopwatch_paint_failure=!stopwatch_crown_failure;
  watch_test_stopwatch_main();assert(stopwatch_acquires==2&&!stopwatch_releases);retained_checks();
  printf("Async real pinned Stopwatch: %s PASS\n",test);return 0;
 }
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!strcmp(test,"failure-context-launch")){
  async_fingerprint_api=(contexts_fingerprint_service_v1){.base=async_context_api,.fingerprint_abi=0x31504643u,.fingerprint=async_context_fingerprint};
  async_fingerprint_api.base.struct_size=sizeof(async_fingerprint_api);contexts_client.api=&async_fingerprint_api.base;contexts_client.grant.api=contexts_client.api;contexts_client.rules_available=true;
  failure_mode=1;assert(!contexts_rules_tick());assert(workflow_claims==1&&!workflow_reports&&!context_launch_pending&&!wifi_launches);retained_checks();
 }
 else if(!strcmp(test,"contexts-paint")){unsigned before=context_captures;complete_inside_paint=true;clear();fill(0,0,240,240,0x789a);assert(context_captures>before+10&&physical_submits==1&&surface.frame==CANVAS_DRAWING_FRAME);physical_immutable();present(false);assert(canvas_drain());cleanup();}
 else if(!strcmp(test,"failure-context-message-entry")){context_message_open=true;strcpy(context_message,"Test message");physical_failure=true;t5_app_input_t in={0};assert(!poll(&in,8));retained_checks();}
 else if(!strcmp(test,"failure-context-message")){context_message_open=true;strcpy(context_message,"Test message");failure_mode=1;t5_app_input_t in={0};assert(!poll(&in,8));retained_checks();}
 else
#endif
 if(!strcmp(test,"idle-repainting")){
  last_activity=0;
  for(unsigned n=0;n<4;n++){
   ticks=59952+n*16;clear_color((uint16_t)(0x3000+n));present(false);assert(canvas_token||canvas_sealed);
   t5_app_input_t in={0};bool ok=poll(&in,8);
   if(n<3)assert(ok&&!native_sleep_calls);else assert(!ok&&native_sleep_calls==1);
  }
  assert(native_sleep_retained&&!canvas_token&&!canvas_sealed&&!physical_busy&&!frame_count&&!subscriptions);
  unsigned live=grants,freed=fixture_frees;app_module_fini();assert(grants==live&&live&&fixture_frees==freed);
  free(canvas_pixels);free(previous_pixels);free(alarm_pixels);
 }
 else if(!strcmp(test,"idle-crown-drain")){
  ticks=59990;clear_color(0x4321);present(false);clear_color(0x5432);present(false);ticks=60000;last_activity=0;last_poll_at=ticks;failure_mode=10;
  t5_app_input_t in={0};assert(poll(&in,8)&&in.exit_requested&&crown_pending&&!native_sleep_calls&&!physical_busy&&!canvas_token&&!canvas_sealed&&!return_launches);cleanup();
 }
 else if(!strcmp(test,"idle-contact-drain")){
  ticks=59990;clear_color(0x4321);present(false);clear_color(0x5432);present(false);ticks=60000;last_activity=0;
  tap(polls+2,100,160);t5_app_input_t in={0};assert(poll(&in,8)&&!native_sleep_calls);
  assert(input_pending&&input_sample.down&&last_activity>=60000&&subscriptions);
  unsigned taps=0;for(unsigned n=0;n<4;n++){assert(poll(&in,8));if(in.tapped){++taps;assert(in.touch_x==100&&in.touch_y==160);}}
  assert(taps==1&&!native_sleep_calls);assert(canvas_drain());cleanup();
 }
 else if(!strcmp(test,"healthy-poll")){t5_app_input_t in={0};assert(poll(&in,8)&&!in.exit_requested&&physical_busy&&busy_service_calls&&busy_inputs);check_app_background();cleanup();}
 else if(!strcmp(test,"controls")){opening(3);tap(80,120,222);drain_to(160);check_app_background();cleanup();}
 else if(!strcmp(test,"controls-settings")){settings_render(0,0);editor_field=3;sv_switch(SV_VALUE);memcpy(expected_background,canvas_pixels,sizeof(expected_background));opening(3);tap(80,120,222);drain_to(160);check_app_background();assert(sv_page==SV_VALUE&&settings_editing&&editor_field==3);cleanup();}
 else if(!strcmp(test,"controls-save")){opening(3);tap(80,44,90);tap(81,110,90);tap(82,182,90);tap(88,182,62);tap(89,44,62);tap(90,110,62);tap(96,44,62);tap(100,182,62);tap(106,51,139);tap(112,120,222);drain_to(180);check_app_background();assert(pref_value[0]==100&&pref_writes[0]==3&&pref_value[1]==0&&pref_writes[1]==2);cleanup();}
 else if(!strcmp(test,"torch-alarm")){opening(3);tap(80,193,185);inject_alarm_at=95;tap(110,100,165);drain_to(180);check_app_background();assert(alarm_injected&&acks==1&&alarm_fake.state==ALARM_STATE_READY&&!quick.ui.torch&&hardware_brightness==40);cleanup();}
 else if(!strcmp(test,"alarm")){alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};tap(3,100,165);bool consumed=false;assert(alarm_foreground(&consumed)&&consumed&&acks==1);check_app_background();cleanup();}
 else if(!strcmp(test,"crown-controls")){failure_mode=6;quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;t5_app_input_t in={0};assert(poll(&in,8));assert(in.exit_requested&&crown_pending&&!pref_writes[1]&&!return_launches&&!wifi_launches&&!physical_busy);assert(!memcmp(framebuffer,expected_background,sizeof(expected_background)));cleanup();}
 else if(!strcmp(test,"crown-alarm")){failure_mode=7;alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};t5_app_input_t in={0};assert(poll(&in,8));assert(in.exit_requested&&crown_pending&&!acks&&!return_launches&&!wifi_launches&&!physical_busy);cleanup();}
 else if(!strcmp(test,"crown-loading")||!strcmp(test,"crown-cue")){bool cue=!strcmp(test,"crown-cue");failure_mode=cue?9:8;alarm_fake.state=cue?ALARM_STATE_CUE:ALARM_STATE_LOADING;t5_app_input_t in={0};assert(poll(&in,8));assert(in.exit_requested&&crown_pending&&!acks&&!return_launches&&!wifi_launches&&!physical_busy&&stop_calls==3);cleanup();}
 else if(!strcmp(test,"crown-alarm-stop-failure")){failure_mode=7;output_bad=true;alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};t5_app_input_t in={0};assert(!poll(&in,8));assert(crown_pending&&!acks&&stop_calls==3);retained_checks();}
 else if(!strcmp(test,"wifi-handoff")){opening(3);tap(80,51,185);t5_app_input_t in={0};while(!in.exit_requested)assert(poll(&in,8));assert(wifi_launches==1&&!return_launches&&!physical_busy);cleanup();}
 else{
  if(!strcmp(test,"failure-poll"))failure_mode=1;
  else if(!strcmp(test,"failure-controls")){failure_mode=2;quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;}
  else if(!strcmp(test,"failure-alarm")){failure_mode=3;alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};}
  else if(!strcmp(test,"failure-loading")){failure_mode=4;alarm_fake.state=ALARM_STATE_LOADING;}
  else if(!strcmp(test,"failure-cue")){failure_mode=5;alarm_fake.state=ALARM_STATE_CUE;}
  else assert(!"unknown scenario");
  t5_app_input_t in={0};assert(!poll(&in,8));retained_checks();
 }
 printf("Async modal: %s PASS\n",test);return 0;
}
