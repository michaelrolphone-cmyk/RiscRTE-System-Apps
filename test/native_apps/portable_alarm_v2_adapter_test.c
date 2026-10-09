/* Real adapter, toolbar, modal and Quick paths; only providers are doubles. */
#define TEST_NATIVE_TOOLBAR_QUICK
#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
static int32_t alarm_result,alarm_error;
static unsigned alarm_steps,alarm_status_calls;
static bool modal_back;
static int32_t tagged_status(void *context,alarm_status_v1 *out){
 (void)context;io();++alarm_status_calls;
 *out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=alarm_error?ALARM_STATE_BLOCKED:ALARM_STATE_READY,.error=alarm_error};
 return retain_alarm_status?ALARM_RETAINED:ALARM_OK;
}
static int32_t tagged_step(void *context){(void)context;io();++alarm_steps;return alarm_result;}
static int32_t tagged_refresh(void *context){(void)context;io();++refreshes;return retain_refresh?ALARM_RETAINED:alarm_result;}
static int32_t tagged_ack(void *context,const alarm_token_v1 *token){(void)token;return tagged_step(context);}
static int32_t tagged_stop(void *context){(void)context;io();++alarm_stops;return retain_alarm_stop?ALARM_RETAINED:ALARM_OK;}
static alarm_service_descriptor_v2 tagged={.base={2,sizeof(tagged),NULL,tagged_status,tagged_step,tagged_refresh,tagged_ack,fx_alarm_prepare,tagged_stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=0};
static bool tagged_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(strcmp(name,ALARM_SERVICE_CAPABILITY))return fx_acquire(name,version,instance,out);
 io();assert(version==2&&!instance);++acquires;++live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=&tagged.base};return true;
}
static void tagged_yield(uint32_t ms){fx_yield(ms);if(modal_back&&alarm_modal)navigation_pending=T5_APP_BUTTON_BACK;}
int main(int argc,char **argv){
 assert(argc==2);const char *name=argv[1];fx_runtime.acquire=tagged_acquire;fx_runtime.yield_ms=tagged_yield;
 assert(app_module_init()==0);const paper_presentation *view=paper_presentation_get();assert(view);
 view->begin();view->text(20,20,300,"FOREGROUND",1,false,true);present(false);assert(alarm_pixels_valid);
 if(!strcmp(name,"storage")||!strcmp(name,"rtc")){
  alarm_result=alarm_error=!strcmp(name,"storage")?ALARM_STORAGE:ALARM_RTC;modal_back=true;
  bool consumed=false;assert(alarm_foreground(&consumed)&&consumed&&alarm_error_seen&&!failed&&!retained);
  assert(alarms.status.error==alarm_error&&alarms.status.state==ALARM_STATE_BLOCKED&&alarm_steps>=1&&alarm_status_calls>=1);
  assert(!alarm_modal&&!alarm_stops);quick.ui.action_dnd=true;assert(quick_apply(PQA_DND));
  assert(!failed&&!retained&&refreshes==1);alarm_result=alarm_error=0;
 }else if(!strcmp(name,"visual")||!strcmp(name,"sound")){
  tagged.output_modes=!strcmp(name,"sound")?ALARM_MODE_SOUND:0;
  quick.ui.volume_valid=true;quick_paper_capabilities();assert(quick.ui.volume_valid==(tagged.output_modes!=0));
  quick_paper_render("13:42",true,70);present(false);assert(!retained);
 }else if(!strcmp(name,"step-retained")){
  alarm_result=ALARM_RETAINED;unsigned previous=alarm_status_calls;assert(!alarm_audio_pump());assert(alarm_status_calls==previous);check_retained(view);
 }else if(!strcmp(name,"status-retained")){
  retain_alarm_status=true;assert(!portable_alarm_status(&alarms));check_retained(view);
 }else if(!strcmp(name,"copied-retained")){
  alarm_error=ALARM_RETAINED;assert(!portable_alarm_status(&alarms));check_retained(view);
 }else if(!strcmp(name,"quick-retained")){
  quick.ui.action_dnd=true;retain_refresh=true;assert(!quick_apply(PQA_DND));assert(refreshes==1&&kv_writes==1);check_retained(view);
 }else if(!strcmp(name,"ack-retained")){
  alarm_result=ALARM_RETAINED;alarm_token_v1 token={0};assert(!portable_alarm_acknowledge(&alarms,&token));check_retained(view);
 }else if(!strcmp(name,"stop-retained")){
  retain_alarm_stop=true;assert(!portable_alarm_failure_stop(&alarms)&&alarm_stops==1);check_retained(view);
 }else assert(0);
 if(!retained){app_module_fini();assert(!live&&!frames&&!subscriptions);}
 printf("API2 production adapter %s PASS\n",name);return 0;
}
