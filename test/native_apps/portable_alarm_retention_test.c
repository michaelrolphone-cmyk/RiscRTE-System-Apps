/* Execute production API1 client + foreground adapter, with every native call
 * fenced by the fake Runtime's promotion boundary. No hardware claims. */
#define PORTABLE_ALARM_TERMINAL_RETENTION
#define PORTABLE_ALARM_CUSTOM_SLEEP_FIXTURE
#define PORTABLE_CUSTOM_RUNTIME_FIXTURE
#define PORTABLE_QUICK_FIXTURE_MAIN legacy_quick_fixture
#include "quick_adapter_test.c"
static unsigned io_calls,retentions,trigger,sleep_phase;
static bool forbidden,runtime_missing,activation_lost,release_lost,activation_denied;
static int32_t ordinary_step;
static void io(void){assert(!forbidden);io_calls++;}
static bool promote(void){assert(!forbidden);forbidden=true;retentions++;return true;}
static struct {risc_runtime_api_v1 base;bool(*confirm)(void);bool(*retain)(void);} runtime;
static alarm_service_sleep_v1 guarded_service;
static bool health_checked(risc_runtime_health_v1 *out){io();return quick_health(out);}
static void yield_checked(uint32_t ms){io();test_yield(ms);}
static bool diagnostic_checked(const char *s){io();return test_diagnostic(s);}
static bool release_checked(risc_runtime_capability_v1 *g){io();if(release_lost){runtime_missing=true;return false;}return test_release(g);}
static bool launch_checked(const char *s){io();return quick_launch(s);}
static bool acquire_checked(const char *s,uint32_t v,uint64_t id,risc_runtime_capability_v1 *g){
 io();if(!strcmp(s,ALARM_SERVICE_CAPABILITY)){assert(v==1&&!id);if(activation_lost){runtime_missing=true;return false;}if(activation_denied)return false;g->api=&guarded_service;grants++;return true;}
 return quick_acquire(s,v,id,g);
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&!runtime_missing?&runtime.base:NULL;}
static int32_t status_checked(void *c,alarm_status_v1 *s){io();if(trigger==2)return ALARM_RETAINED;int32_t r=service_status(c,s);if(trigger==3)s->error=ALARM_RETAINED;return r;}
static int32_t step_checked(void *c){io();return trigger==1?ALARM_RETAINED:ordinary_step?ordinary_step:service_step(c);}
static int32_t refresh_checked(void *c){io();return trigger==4?ALARM_RETAINED:service_refresh(c);}
static int32_t ack_checked(void *c,const alarm_token_v1 *t){io();return trigger==5?ALARM_RETAINED:service_ack(c,t);}
static int32_t stop_checked(void *c){io();return trigger==6?ALARM_RETAINED:service_stop(c);}
static int32_t prepare_checked(void *c,alarm_sleep_v1 *s){(void)c;io();assert(s);return trigger==7?ALARM_RETAINED:ALARM_OK;}
static int32_t resume_checked(void *c,const alarm_sleep_v1 *s){(void)c;io();assert(s);return trigger==8?ALARM_RETAINED:ALARM_OK;}
static bool display_acquire_checked(void*c,uint32_t f,risc_display_surface_v1*s){io();return frame_acquire(c,f,s);}
static void display_release_checked(void*c,risc_display_frame_v1 f){io();frame_release(c,f);}
static bool display_submit_checked(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){io();return capture_submit(c,f,r,n,o,t);}
static bool display_status_checked(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){io();return frame_status(c,t,s);}
static bool navigation_reset_checked(void*c){io();return nav_reset(c);}
static bool navigation_poll_checked(void*c,risc_input_navigation_frame_v1*f){io();return nav_poll(c,f);}
static unsigned background_calls;
#ifdef PORTABLE_BLE_BROADCAST
static bool background_pause(void*c){(void)c;io();background_calls++;return true;}
static const telemetry_broadcast_v1 background={.api_version=1,.struct_size=sizeof(background),.pause=background_pause};
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
static bool context_pause(void*c){(void)c;io();background_calls++;return true;}
static bool context_status(void*c,contexts_status_v1*s){(void)c;io();*s=(contexts_status_v1){.struct_size=sizeof(*s)};return true;}
static bool context_capture(void*c){(void)c;io();return true;}
static const contexts_service_v1 context={.api_version=1,.struct_size=sizeof(context),.pause=context_pause,.status=context_status,.capture_audio=context_capture};
#endif
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){
 (void)r;(void)d;(void)g;io();assert(a && a!=alarms.api);alarm_sleep_v1 s={.struct_size=sizeof(s)};
 if(sleep_phase==0)(void)a->prepare_sleep(a->context,&s);
 else if(sleep_phase==1)(void)((const alarm_service_sleep_v1*)a)->resume_sleep(a->context,&s);
 else (void)a->step(a->context);
 /* Even an erroneous ordinary hook return cannot reopen touch or render. */
 return 0;
}
static void begin(void){
 setup();runtime.base=quick_runtime;runtime.base.struct_size=sizeof(runtime);runtime.retain=promote;
 runtime.base.health=health_checked;runtime.base.yield_ms=yield_checked;runtime.base.diagnostic=diagnostic_checked;
 runtime.base.acquire=acquire_checked;runtime.base.release=release_checked;runtime.base.request_launch=launch_checked;rt=&runtime.base;
 quick_display_api.acquire=display_acquire_checked;quick_display_api.release=display_release_checked;
 quick_display_api.submit=display_submit_checked;quick_display_api.present_status=display_status_checked;
 nav_api.reset=navigation_reset_checked;nav_api.poll=navigation_poll_checked;
 guarded_service=(alarm_service_sleep_v1){.base={1,sizeof(guarded_service),NULL,status_checked,step_checked,refresh_checked,ack_checked,prepare_checked,stop_checked},.resume_sleep=resume_checked};
 assert(portable_alarm_close(&alarms,rt));assert(portable_alarm_open(&alarms,rt));
#ifdef PORTABLE_BLE_BROADCAST
 broadcast_client.api=&background;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
 contexts_client.api=&context;
#endif
}
static void check_terminal(void){
 assert(forbidden && retentions==1 && portable_adapter_retained() && portable_app_sleep_retained());
 unsigned before=io_calls,live=grants,background_before=background_calls;
 const alarm_service_v1*a=alarm_sleep_api();alarm_sleep_v1 s={.struct_size=sizeof(s)};alarm_token_v1 t={0};alarm_status_v1 st={.struct_size=sizeof(st)};
 for(unsigned n=0;n<3;n++){
  assert(!portable_alarm_pump(&alarms)&&!portable_alarm_status(&alarms));
  assert(!portable_alarm_refresh(&alarms)&&!portable_alarm_acknowledge(&alarms,&t));
  assert(!portable_alarm_failure_stop(&alarms)&&!portable_alarm_close(&alarms,rt)&&!portable_alarm_open(&alarms,rt));
  if(a){assert(a->step(a->context)==ALARM_RETAINED&&a->status(a->context,&st)==ALARM_RETAINED);
  assert(a->refresh(a->context)==ALARM_RETAINED&&a->acknowledge(a->context,&t)==ALARM_RETAINED);
  assert(a->stop_only(a->context)==ALARM_RETAINED&&a->prepare_sleep(a->context,&s)==ALARM_RETAINED);
  assert(((const alarm_service_sleep_v1*)a)->resume_sleep(a->context,&s)==ALARM_RETAINED);}
  bool consumed=false;assert(!alarm_foreground(&consumed)&&!alarm_audio_pump()&&!alarm_failure()&&!idle_sleep());
  assert(!portable_background_stop()&&!quick_apply(PQA_VOLUME_COMMIT));
  clear();present(false);input_service();assert(millis_now()==0);assert(!launch(0));
  t5_app_input_t in;assert(!poll(&in,1));portable_adapter_retain();app_module_fini();assert(app_module_init()==-1);
 }
 assert(io_calls==before && grants==live && background_calls==background_before && retentions==1);
}
int main(int argc,char**argv){
 assert(argc==2);unsigned route=(unsigned)atoi(argv[1]);begin();
 bool consumed=false;
 if(route<=2){trigger=route+1;assert(!alarm_foreground(&consumed));}
 else if(route==3){trigger=4;alarm_fake.state=ALARM_STATE_BLOCKED;alarm_fake.error=ALARM_STORAGE;tap(3,100,215);assert(!alarm_foreground(&consumed));}
 else if(route==4){trigger=5;alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};tap(3,100,170);assert(!alarm_foreground(&consumed));}
 else if(route==5){trigger=6;failed=true;assert(!alarm_failure());}
 else if(route==6){trigger=6;failed=true;app_module_fini();}
 else if(route==7){trigger=3;app_module_fini();}
 else if(route>=8&&route<=10){trigger=route==8?7:route==9?8:1;sleep_phase=route-8;assert(!idle_sleep());}
 else if(route==11){portable_adapter_retain();}
 else if(route==12){
  for(int e=ALARM_STORAGE;e>=ALARM_RTC;e--){ordinary_step=e;alarm_fake.state=ALARM_STATE_BLOCKED;alarm_fake.error=e;assert(portable_alarm_pump(&alarms)&&!portable_adapter_retained());}
  ordinary_step=0;alarm_fake.state=ALARM_STATE_READY;alarm_fake.error=0;
  assert(portable_alarm_refresh(&alarms));assert(!portable_adapter_retained());
  app_module_fini();assert(!grants&&!forbidden);
  /* A second ordinary invocation gets a fresh client/facade, while a retained
   * invocation above rejects repeated init without even querying Runtime. */
#ifdef PORTABLE_BLE_BROADCAST
  broadcast_client.api=NULL;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  contexts_client.api=NULL;
#endif
  begin();assert(portable_alarm_pump(&alarms));app_module_fini();assert(!grants&&!forbidden);
  puts("Ordinary RTC/storage errors and repeated clean adapter invocation PASS");return 0;
 }else if(route==13){trigger=4;assert(!quick_apply(PQA_VOLUME_COMMIT));}
 else if(route==14){assert(portable_alarm_close(&alarms,rt));activation_lost=true;assert(!portable_alarm_open(&alarms,rt));}
 else if(route==15){release_lost=true;assert(!portable_alarm_close(&alarms,rt));}
 else if(route==16){
  assert(portable_alarm_close(&alarms,rt));activation_denied=true;assert(!portable_alarm_open(&alarms,rt)&&!portable_adapter_retained());
  activation_denied=false;assert(portable_alarm_open(&alarms,rt));app_module_fini();assert(!grants&&!forbidden);
  puts("Ordinary unavailable activation remains recoverable PASS");return 0;
 }
 else assert(0);
 check_terminal();printf("Terminal API1 adapter route %u PASS\n",route);return 0;
}
