/* Production X4 adapter with strict provider doubles. No hardware qualification. */
#define main original_toolbar_main
#include "portable_native_toolbar_test.c"
#undef main
#include "PortableBroadcastAppData.h"
static unsigned context_calls,context_pauses,context_steps,capture_calls,max_gap,last_capture,sleep_calls;
static bool observing,pause_refused,capture_refused,release_refused,storage_refused,async_display=true,room_ready;
static int sleep_result;
static unsigned acquire_fault;
static contexts_service_v1 bad_ctx_api;
static contexts_policy_v1 observed_policy;
static uint32_t preset_result_value;
static contexts_status_v1 copied_status={.struct_size=sizeof(copied_status)};
static bool ctx_pause(void *c){(void)c;io();++context_calls;++context_pauses;if(pause_refused)return false;observing=false;return true;}
static bool ctx_step(void *c,const contexts_policy_v1 *p){(void)c;io();assert(!surface.frame);++context_calls;++context_steps;observed_policy=*p;observing=p->enabled&&(p->audio_allowed||p->radio_allowed);return true;}
static bool ctx_status(void *c,contexts_status_v1 *out){(void)c;io();++context_calls;*out=copied_status;return true;}
static bool ctx_capture(void *c){(void)c;io();++context_calls;++capture_calls;unsigned gap=ticks-last_capture;if(gap>max_gap)max_gap=gap;last_capture=ticks;return !capture_refused;}
static bool ctx_mask(void *c,uint32_t x){(void)c;(void)x;io();return true;}
static bool ctx_record(void *c,uint32_t a,uint32_t b,uint32_t d,const void *e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;io();return true;}
static bool ctx_finish(void *c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;io();return true;}
static int32_t ctx_label(void *c,uint32_t a,uint32_t b,contexts_label_v1 *out){(void)c;(void)a;(void)b;(void)out;io();return 0;}
static bool ctx_claim(void *c,uint32_t a,uint32_t b,const char *name,uint32_t d){(void)c;(void)a;(void)b;(void)name;(void)d;io();return true;}
static bool ctx_result(void *c,uint32_t a,uint32_t b,uint32_t result){(void)c;(void)a;(void)b;io();preset_result_value=result;return true;}
static const contexts_service_v1 ctx_api={.api_version=1,.struct_size=sizeof(ctx_api),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=ctx_mask,.begin_export=ctx_mask,.export_record=ctx_record,.finish_export=ctx_finish,.label=ctx_label,.claim_preset=ctx_claim,.preset_result=ctx_result,.capture_audio=ctx_capture};
static int32_t ctx_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size){
 assert(!observing);if(storage_refused){io();return RISC_KEY_VALUE_CONTEXT;}
 if(!strcmp(key,PORTABLE_CONTEXT_ENABLED_KEY)){io();const uint8_t value[]={'C',1,1,0xa4};assert(capacity>=4);memcpy(out,value,4);*size=4;return RISC_KEY_VALUE_OK;}
 if(room_ready&&!strcmp(key,"ctx_p0")){
  io();portable_context_preset p;portable_context_preset_init(&p);p.source=CONTEXTS_RADIO;p.slot=0;p.enabled=true;p.dnd=true;
  p.actions=PORTABLE_CONTEXT_ACTIONS&~PORTABLE_CONTEXT_IDLE;strcpy(p.name,"Study");assert(capacity>=64&&portable_context_preset_encode(&p,out));*size=64;return RISC_KEY_VALUE_OK;
 }
 return fx_kv_get(c,key,out,capacity,size);
}
static const risc_key_value_v1 ctx_kv={1,sizeof(ctx_kv),NULL,ctx_get,fx_kv_put};
static alarm_service_descriptor_v2 ctx_alarm={.base={2,sizeof(ctx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_refresh,fx_alarm_ack,fx_alarm_prepare,fx_alarm_stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=0};
static bool ctx_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);if(async_display)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static unsigned busy_until;
static uint8_t busy_pixels[sizeof(pixels)];
static bool ctx_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 if(busy_until&&ticks<busy_until){io();assert(token&&!memcmp(busy_pixels,pixels,sizeof(pixels)));out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 return fx_present(c,token,out);
}
static risc_display_output_api_v1 ctx_display;
static bool ctx_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 const void *api=NULL;
 if(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)){
  assert(version==1&&!instance);api=&ctx_api;
  if(acquire_fault==1){io();return false;}
  if(acquire_fault==2){io();++acquires;++live;out->slot=acquires;out->generation=1;out->api=api;return false;}
  if(acquire_fault==3){io();out->struct_size=4;return false;}
  if(acquire_fault==4){io();return true;}
  if(acquire_fault==5){bad_ctx_api=ctx_api;bad_ctx_api.capture_audio=NULL;api=&bad_ctx_api;}
 }
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==2&&!instance);api=&ctx_alarm;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(version==1&&instance==1);api=&ctx_kv;}
 else if(!strcmp(name,"display.output"))api=&ctx_display;
 else return fx_acquire(name,version,instance,out);
 io();++acquires;++live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=api};return true;
}
static bool ctx_release(risc_runtime_capability_v1 *grant){if(release_refused&&grant->api==&ctx_api){io();return false;}return fx_release(grant);}
int portable_app_idle_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a){
 assert(r==rt&&d==display&&b==gauge&&a==alarms.api);io();assert(!observing&&!contexts_client.api&&!subscriptions&&!surface.frame&&!paper_token);++sleep_calls;return sleep_result;
}
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a){return portable_app_idle_sleep(r,d,b,a);}
static unsigned data_calls;
static int32_t ctx_data_stat(void *c,const char *name,uint32_t *size,uint64_t *revision) {
 (void)c;io();assert(!observing&&!strcmp(name,"model"));++data_calls;*size=4;*revision=7;return RISC_APP_DATA_OK;
}
static int32_t ctx_data_read(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *revision) {
 assert(expected==7&&cap==4);int32_t result=ctx_data_stat(c,name,size,revision);memcpy(out,"test",4);return result;
}
static int32_t ctx_data_replace(void *c,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
 assert(expected==7&&size==4&&!memcmp(bytes,"test",4));uint64_t revision;return ctx_data_stat(c,name,&size,&revision);
}
static const risc_app_data_v1 ctx_data={1,sizeof(ctx_data),NULL,ctx_data_stat,ctx_data_read,ctx_data_replace};
static void verify_retained(void){
 assert(retained&&barriers==1);unsigned old=calls,old_free=free_calls;
 fill(0,0,480,800,0);np_pixel(1,1,0,255);present(false);input_service();
 t5_app_input_t in;assert(!poll(&in,20));assert(!portable_contexts_service());
 assert(!portable_contexts_stop()&&!portable_contexts_enable(true));app_module_fini();
 assert(calls==old&&free_calls==old_free);
}
int main(int argc,char **argv){
 assert(argc==2);const char *which=argv[1];
 ctx_display=fx_display;ctx_display.get_info=ctx_info;ctx_display.present_status=ctx_present;fx_runtime.acquire=ctx_acquire;fx_runtime.release=ctx_release;
 if(!strcmp(which,"missing-empty"))acquire_fault=1;
 assert(app_module_init()==0&&!retained);assert(width()==480&&height()==800&&paper_presentation_get());
 if(acquire_fault==1) {
  assert(!portable_contexts_service()&&!failed&&!retained&&!context_calls);
  assert(contexts_tick()&&!observing&&!context_calls);
  t5_app_input_t in;assert(poll(&in,20)&&!failed&&!retained);
  quick.ui.action_dnd=true;assert(quick_apply(PQA_DND)&&dnd_saved);
  assert(custody_launch("default.elf")&&!retained);
  acquire_fault=0;assert(portable_contexts_service()==&ctx_api&&contexts_tick()&&observing);
  app_module_fini();assert(!live&&!subscriptions&&!frames&&!retained);
  puts("Context X4 clean missing provider keeps polling, settings and launch usable, then recovers PASS");return 0;
 }
 if(!strcmp(which,"dirty-refused")||!strcmp(which,"missing-size")||!strcmp(which,"empty-success")||!strcmp(which,"invalid-api")) {
  assert(contexts_suspend());acquire_fault=!strcmp(which,"dirty-refused")?2:!strcmp(which,"missing-size")?3:!strcmp(which,"empty-success")?4:5;
  if(acquire_fault==2)assert(!contexts_tick());else assert(!portable_contexts_service());
  verify_retained();
  printf("Context X4 %s preserves terminal custody fence PASS\n",which);return 0;
 }
 assert(contexts_tick()&&observing);assert(observed_policy.audio_allowed&&observed_policy.radio_allowed);
 assert(portable_contexts_action_mask()==(PORTABLE_CONTEXT_IDLE|PORTABLE_CONTEXT_DND));
 if(!strcmp(which,"normal")){
  t5_app_input_t in;max_gap=0;last_capture=ticks;assert(poll(&in,120));assert(capture_calls>=15&&max_gap<=8);
  unsigned before=capture_calls;clear_color(0xffff);assert(capture_calls>before+700);present(false);assert(!failed);
  quick.ui.action_dnd=true;assert(quick_apply(PQA_DND)&&!observing&&dnd_saved);
  assert(contexts_tick()&&observing);assert(custody_launch("default.elf")&&!observing&&!contexts_client.api);
 }else if(!strcmp(which,"busy-state")){
  assert(portable_paper_frame_ready());clear_color(0xffff);present(false);
  memcpy(busy_pixels,pixels,sizeof(pixels));busy_until=ticks+2300;
  unsigned before=context_steps;t5_app_input_t in;
#ifdef PORTABLE_BLE_BROADCAST
  unsigned before_broadcast=fixture_broadcast_steps;
#endif
  for(unsigned n=0;n<20;n++){assert(poll(&in,20));assert(paper_token&&!display_settled&&!automatic_idle_ready()&&automatic_idle_interaction_ready());}
  assert(context_steps>=before+20&&observing&&capture_calls>0&&ticks<busy_until);
#ifdef PORTABLE_BLE_BROADCAST
  assert(fixture_broadcast_steps>=before_broadcast+20);
#endif
  assert(portable_paper_frame_drain()&&!paper_token&&display_settled);
 }else if(!strcmp(which,"mask")){
  copied_status.radio=(contexts_source_status_v1){.source=CONTEXTS_RADIO,.model_generation=1,.room_slot=0,.room_valid=true,.current=true,.room_entry=1};strcpy(copied_status.radio.room_name,"Study");room_ready=true;ticks+=1001;
  assert(contexts_tick()&&dnd_saved&&kv_writes==1&&preset_result_value==CONTEXTS_PRESET_PARTIAL);
 }else if(!strcmp(which,"synchronous")){
  info.flags&=~RISC_DISPLAY_INFO_ASYNC_PRESENT;assert(contexts_tick()&&!observed_policy.audio_allowed&&observed_policy.radio_allowed);
 }else if(!strcmp(which,"auto-sleep")){
  ticks=60001;last_activity=0;sleep_result=1;t5_app_input_t in;
  assert(poll(&in,0)&&sleep_calls==1&&!observing&&!contexts_client.api);
  assert(contexts_tick()&&observing);
 }else if(!strcmp(which,"sleep")||!strcmp(which,"sleep-retained")){
  sleep_result=!strcmp(which,"sleep-retained")?-2:1;automatic_idle=true;
  bool ok=idle_sleep();assert(sleep_calls==1&&!observing);
  if(sleep_result==-2){assert(!ok);verify_retained();puts("Context X4 retained Light fence PASS");return 0;}
  assert(ok&&contexts_tick()&&observing);
 }else if(!strcmp(which,"app-data")||!strcmp(which,"app-data-refused")){
  portable_broadcast_app_data bound;const risc_app_data_v1 *api=portable_broadcast_data_bind(&bound,&ctx_data,rt);assert(api);
  pause_refused=!strcmp(which,"app-data-refused");uint32_t size=0;uint64_t revision=0;char data[4];
  int32_t result=api->stat(api->context,"model",&size,&revision);
  if(pause_refused){assert(result==RISC_APP_DATA_RETAINED&&!data_calls);verify_retained();puts("Context X4 app-data refusal fence PASS");return 0;}
  assert(result==RISC_APP_DATA_OK&&size==4&&revision==7&&data_calls==1);
  assert(contexts_tick()&&observing);assert(api->read(api->context,"model",7,data,4,&size,&revision)==RISC_APP_DATA_OK&&data_calls==2);
  assert(contexts_tick()&&observing);assert(api->replace(api->context,"model",7,data,4)==RISC_APP_DATA_OK&&data_calls==3);
 }else if(!strcmp(which,"pending-capture")){
  assert(portable_paper_frame_ready());clear_color(0xffff);present(false);assert(paper_token&&!display_settled);
  capture_refused=true;assert(!paper_present_progress());verify_retained();puts("Context X4 pending presentation fence PASS");return 0;
 }else if(!strcmp(which,"enable-pause")){
  pause_refused=true;assert(!portable_contexts_enable(false));verify_retained();puts("Context X4 enable pause fence PASS");return 0;
 }else if(!strcmp(which,"capture-raster")){
  clear_color(0xffff);capture_refused=true;fill(0,0,480,800,0);verify_retained();puts("Context X4 raster fence PASS");return 0;
 }else if(!strcmp(which,"capture-input")){
  capture_refused=true;input_service();verify_retained();puts("Context X4 input fence PASS");return 0;
 }else if(!strcmp(which,"capture-present")){
  clear_color(0xffff);capture_refused=true;present(false);verify_retained();puts("Context X4 presentation fence PASS");return 0;
 }else if(!strcmp(which,"pause")){
  pause_refused=true;unsigned writes=kv_writes;quick.ui.action_dnd=true;assert(!quick_apply(PQA_DND)&&kv_writes==writes);verify_retained();puts("Context X4 pause fence PASS");return 0;
 }else if(!strcmp(which,"storage")){
  storage_refused=true;contexts_client.loaded=false;assert(!contexts_tick());verify_retained();puts("Context X4 storage fence PASS");return 0;
 }else if(!strcmp(which,"release")){
  release_refused=true;assert(!contexts_suspend());verify_retained();puts("Context X4 release fence PASS");return 0;
 }else assert(!"unknown test");
 app_module_fini();assert(!live&&!subscriptions&&!frames&&!retained&&!observing);
 printf("Context X4 %s PASS\n",which);return 0;
}
