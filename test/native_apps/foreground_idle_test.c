/* Actual shared adapter and Settings model. Hardware alone is doubled. */
#define PORTABLE_FOREGROUND_IDLE
#define PORTABLE_FOREGROUND_IDLE_WORK_GUARD
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_SETTINGS_IDLE_TIMER
#define PORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS 15
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_INPUT_NAVIGATION_LOCAL
#define PORTABLE_TEXT_INPUT_CLIENT
#define PORTABLE_TEXT_INPUT_POLICY
#define PORTABLE_AUDIO_CONTINUOUS_CAPTURE
#define PORTABLE_RADIO_CONTINUOUS_CAPTURE
#define PORTABLE_WIFI_SESSION_APP
#define PORTABLE_RETURN_APP "springboard.elf"
#define TEST_SETTINGS_CUSTOM_RUNTIME
#include <stdbool.h>
bool portable_wifi_services_safe(void);
bool portable_wifi_idle_ready(void);
bool portable_wifi_close(void);
bool portable_wifi_stop_pending(void);
#define main original_fixture_main
#include "portable_settings_test.c"
#undef main

static risc_runtime_api_v1 selected_runtime;
static uint32_t timeout_ms=15000;
static unsigned sleeps,retains,service_calls,radio_stops,audio_stops,nav_resets;
static int sleep_status=1,service_status=RISC_RUNTIME_CHECKPOINT_OK;
static int configure_status;
static unsigned configured,timer_changes,timer_writes;
static bool timer_fail;
static bool terminal,work,audio_work,radio_work,wifi_work,hold_wake;
static const char *fault;
static const risc_key_value_v1 timer_kv;
static const risc_touch_api_v1 checked_touch;
static bool retained(void){if(!terminal){++retains;terminal=true;submit_failed=true;}return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&!terminal?&selected_runtime:NULL;}
static int32_t service(void){assert(!terminal);++service_calls;if(fault&&!strcmp(fault,"service-success-retained"))retained();return service_status;}
static bool owned_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(!terminal);bool ok=test_acquire(name,version,instance,g);
 if(ok){g->slot=grants;g->generation=1;}
 if(ok&&g->api==&kv_api)g->api=&timer_kv;
 if(ok&&g->api==&touch_api)g->api=&checked_touch;
 if(sleeps&&fault&&!strcmp(fault,"acquire-retained")&&!strcmp(name,"input.touch.raw"))retained();
 return ok;
}
static bool owned_release(risc_runtime_capability_v1 *g){assert(!terminal);bool ok=test_release(g);if(ok){g->slot=g->generation=0;}return ok;}
static bool checked_unsubscribe(void *c,uint64_t subscription){
 assert(!terminal);bool ok=touch_unsubscribe(c,subscription);
 if(fault&&!strcmp(fault,"unsubscribe-retained"))retained();
 return ok;
}
static bool checked_snapshot(void *c,risc_touch_snapshot_v1 *out){
 assert(!terminal);bool ok=touch_snapshot(c,out);
 if(sleeps&&fault&&!strcmp(fault,"snapshot-retained"))retained();
 return ok;
}
static const risc_touch_api_v1 checked_touch={1,sizeof(checked_touch),NULL,touch_subscribe,checked_unsubscribe,touch_poll,touch_next,checked_snapshot};
static bool checked_nav_foreground(void *c,const risc_input_foreground_v1 *claims,size_t count){
 assert(!terminal);bool ok=nav_foreground(c,claims,count);
 if(!count&&fault&&!strcmp(fault,"navigation-retained"))retained();
 return ok;
}
static bool checked_nav_reset(void *c){assert(!terminal);++nav_resets;return nav_reset(c);}
static bool checked_nav_poll(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;assert(!terminal);*out=(risc_input_navigation_frame_v1){0};
 if(hold_wake)out->buttons=out->pressed=RISC_NAV_HOME;
 return true;
}
static const risc_input_navigation_api_v1 checked_navigation={1,sizeof(checked_navigation),NULL,checked_nav_poll,checked_nav_foreground,checked_nav_reset};
uint32_t portable_foreground_idle_timeout_ms(void){return timeout_ms;}
int portable_foreground_idle_configure(const risc_runtime_api_v1 *r){assert(r==&selected_runtime&&!surface.frame&&!paper_token&&!work&&!audio_work&&!radio_work&&!wifi_work);++configured;return configure_status;}
void portable_foreground_idle_timeout_changed(uint32_t ms){++timer_changes;timeout_ms=ms;}
static uint8_t timer_bytes[5];static uint32_t timer_size;
static int32_t timer_get(void *c,const char *key,void *data,uint32_t capacity,uint32_t *size){
 if(strcmp(key,PORTABLE_SLEEP_IDLE_KEY)&&strcmp(key,PORTABLE_SLEEP_DEEP_KEY))return kv_get(c,key,data,capacity,size);
 assert(!terminal);*size=timer_size;if(!timer_size)return RISC_KEY_VALUE_NOT_FOUND;
 assert(capacity>=timer_size);memcpy(data,timer_bytes,timer_size);return RISC_KEY_VALUE_OK;
}
static int32_t timer_put(void *c,const char *key,const void *data,uint32_t size){
 if(strcmp(key,PORTABLE_SLEEP_IDLE_KEY)&&strcmp(key,PORTABLE_SLEEP_DEEP_KEY))return kv_put(c,key,data,size);
 assert(!terminal);++timer_writes;if(timer_fail)return RISC_KEY_VALUE_IO;
 assert(size==sizeof(timer_bytes));memcpy(timer_bytes,data,size);timer_size=size;return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 timer_kv={1,sizeof(timer_kv),NULL,timer_get,timer_put};
bool portable_app_foreground_idle_inhibited(void){return work;}
bool portable_audio_capture_active(void){return audio_work;}
void portable_audio_capture_resume(void){}
bool portable_audio_services_safe(void){return !terminal;}
bool portable_audio_suspend(void){assert(!terminal);++audio_stops;return true;}
bool portable_radio_capture_active(void){return radio_work;}
bool portable_radio_services_safe(void){return !terminal;}
bool portable_radio_suspend(void){assert(!terminal);++radio_stops;return true;}
bool portable_wifi_services_safe(void){if(fault&&!strcmp(fault,"eligibility-retained"))retained();return !terminal;}
bool portable_wifi_idle_ready(void){return !wifi_work;}
bool portable_wifi_close(void){return !terminal;}
bool portable_wifi_stop_pending(void){return false;}
int portable_app_foreground_idle_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *b){
 assert(!terminal&&r==&selected_runtime&&d==&display_api&&!b);
 assert(!surface.frame&&!paper_token&&!subscriptions&&!touch.grant.api);
 assert(settings_grant.api==&timer_kv&&rtc_grant.api==&rtc_api);
 assert(!writes&&!kv_writes&&!return_launches&&!radio_stops&&!audio_stops);
 ++sleeps;ticks+=3100;return sleep_status;
}
static void begin(void){
 selected_runtime=runtime_api;selected_runtime.acquire=owned_acquire;selected_runtime.release=owned_release;
 selected_runtime.retain_invocation=retained;selected_runtime.service_checkpoint=service;
 scenario=100;int init=app_module_init();if(configure_status==-2){assert(init==-1&&terminal&&native_custody_retained);return;}assert(!init);navigation=&checked_navigation;settings_render(0,0);
 t5_app_input_t input;assert(poll(&input,20));assert(!terminal);
}
static bool step(void){t5_app_input_t input={0};bool ok=poll(&input,20);if(ok)assert(!input.exit_requested&&!input.buttons&&!input.tapped);return ok;}
static void finish(void){assert(!terminal&&!failed);app_module_fini();assert(!grants&&!subscriptions&&!frame_count&&!writes&&!kv_writes&&!return_launches);}
int main(int argc,char **argv){
 assert(argc==2);fault=argv[1];
 if(!strcmp(fault,"configure-unavailable"))configure_status=-1;
 if(!strcmp(fault,"configure-retained"))configure_status=-2;
 if(!strncmp(fault,"retry-",6))configure_status=-3;
 begin();assert(configured==1);
 if(terminal){unsigned before=grants;app_module_fini();assert(grants==before&&retains==1);puts("foreground configure-retained: terminal no-I/O PASS");return 0;}
 if(!strncmp(fault,"retry-",6)&&strcmp(fault,"retry-save")) {
  assert(!foreground_idle_configured&&foreground_idle_retry_pending&&foreground_idle_retry_delay==250);
  uint32_t activity=last_activity,interval=timeout_ms;
  unsigned calls=configured;
  if(!strcmp(fault,"retry-modal")){
   assert(portable_text_adapter_suspend()==RISC_TEXT_ENTRY_OK);
   ticks=foreground_idle_retry_at;bool continuity=true;
   assert(!portable_text_adapter_policy_prepare(RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING,&continuity));
   assert(configured==calls);
   assert(portable_text_adapter_policy_prepare(0,&continuity)==1&&!continuity&&configured==calls);
   configure_status=0;uint32_t flags=0;
   assert(!portable_text_adapter_policy_apply(continuity,&flags));
   assert(configured==calls+1&&foreground_idle_configured&&!sleeps&&last_activity==activity&&timeout_ms==interval);
   assert(!portable_text_adapter_resume());finish();puts("foreground retry-modal: paused-only recovery PASS");return 0;
  }
  if(!strcmp(fault,"retry-backoff")){
   for(unsigned i=0;i<8;++i){
    unsigned services=service_calls;calls=configured;
    for(unsigned n=0;n<20;++n)assert(foreground_idle_retry(foreground_idle_retry_at-1,false));
    assert(configured==calls&&service_calls==services);
    uint32_t old_delay=foreground_idle_retry_delay;
    ticks=foreground_idle_retry_at;assert(foreground_idle_retry(ticks,false));
    assert(configured==calls+1&&foreground_idle_retry_delay==(old_delay<4000?old_delay*2:4000));
    assert(timeout_ms==interval&&last_activity==activity);
   }
   configure_status=0;ticks=foreground_idle_retry_at;assert(foreground_idle_retry(ticks,false));
   assert(foreground_idle_configured&&!foreground_idle_retry_pending&&last_activity==activity);
   finish();puts("foreground retry-backoff: capped no-hot-loop PASS");return 0;
  }
  ticks=foreground_idle_retry_at-2;assert(step()&&configured==calls);
  ticks=foreground_idle_retry_at;
  if(!strcmp(fault,"retry-work"))work=true;
  if(!strcmp(fault,"retry-frame"))clear();
  if(!strcmp(fault,"retry-service-busy"))service_status=RISC_RUNTIME_CHECKPOINT_BUSY;
  if(work||surface.frame||service_status==RISC_RUNTIME_CHECKPOINT_BUSY){
   assert(step()&&configured==calls&&!sleeps);
   work=false;if(surface.frame)present(false);service_status=RISC_RUNTIME_CHECKPOINT_OK;
  }
  if(!strcmp(fault,"retry-success")){
   assert(step()&&configured==calls+1&&!foreground_idle_configured&&foreground_idle_retry_delay==500);
   calls=configured;ticks=foreground_idle_retry_at-2;assert(step()&&configured==calls);
   ticks=foreground_idle_retry_at;
  }
  configure_status=!strcmp(fault,"retry-retained")?-2:!strcmp(fault,"retry-unavailable")?-1:0;
  bool ok=step();assert(configured==calls+1);
  if(configure_status==-2){
   assert(!ok&&terminal&&retains==1);unsigned resources=grants;app_module_fini();assert(grants==resources);
   puts("foreground retry-retained: terminal no-I/O PASS");return 0;
  }
  assert(ok&&!foreground_idle_retry_pending&&last_activity==activity&&timeout_ms==interval&&!sleeps);
  if(configure_status==-1){assert(!foreground_idle_configured);ticks+=60000;assert(step()&&configured==calls+1&&!sleeps);}
  else assert(foreground_idle_configured);
  finish();printf("foreground %s: safe retry without policy mutation PASS\n",fault);return 0;
 }
 if(!strncmp(fault,"timer-",6)||!strcmp(fault,"retry-save")) {
  settings_store=&timer_kv;timer_fail=!strcmp(fault,"timer-failed");
  if(!strcmp(fault,"timer-deep")) {
   assert(settings_activate(0,SETTINGS_TIMER_ROW+1)==T5_APP_SETTING_ERROR);
  }else {
   tap(polls+2,180,110); /* Change the draft without saving. */
   tap(polls+5,!strcmp(fault,"timer-cancel")?60:170,213);
   if(timer_fail)tap(polls+9,60,213);
   uint8_t result=settings_activate(0,SETTINGS_TIMER_ROW);
   if(!strcmp(fault,"timer-save")||!strcmp(fault,"retry-save")){assert(result==T5_APP_SETTING_UPDATED&&timer_changes==1&&timeout_ms==20000&&timer_writes==1);}
   else assert(result==T5_APP_SETTING_NO_CHANGE);
  }
  if(!strcmp(fault,"retry-save"))assert(foreground_idle_configured&&!foreground_idle_retry_pending);
  if(strcmp(fault,"timer-save")&&strcmp(fault,"retry-save"))assert(!timer_changes&&timeout_ms==15000);
  assert(!sleeps&&!writes&&!kv_writes);settings_store=&kv_api;finish();
  printf("foreground %s: confirmed cache update only PASS\n",fault);return 0;
 }
 settings_draft.year=2025;settings_draft.day=28;sv_page=SV_VALUE;editor_field=0;
 twatch_rtc_time_v1 draft=settings_draft;
 unsigned before=grants,diagnostic_before=diagnostics;
 if(!strcmp(fault,"refused"))sleep_status=0;
 if(!strcmp(fault,"retained"))sleep_status=-2;
 if(!strcmp(fault,"failure"))sleep_status=-1;
 if(!strcmp(fault,"unknown"))sleep_status=7;
 if(!strcmp(fault,"work"))work=true;
 if(!strcmp(fault,"audio"))audio_work=true;
 if(!strcmp(fault,"radio"))radio_work=true;
 if(!strcmp(fault,"wifi"))wifi_work=true;
 if(!strcmp(fault,"frame")){clear();assert(surface.frame);}
 if(!strcmp(fault,"service-busy"))service_status=RISC_RUNTIME_CHECKPOINT_BUSY;
 if(!strcmp(fault,"service-retained"))service_status=RISC_RUNTIME_CHECKPOINT_RETAINED;
 if(!strcmp(fault,"disabled"))timeout_ms=0;
 if(!strcmp(fault,"invalid"))timeout_ms=UINT32_MAX;
 if(!strcmp(fault,"wrap")){last_activity=UINT32_MAX-10000u;ticks=last_activity;last_poll_at=ticks;}
 if(!strncmp(fault,"modal",5)){
  assert(portable_text_adapter_suspend()==RISC_TEXT_ENTRY_OK);unsigned detached_grants=grants;
  ticks=last_activity+15000;bool continuity=true;
  assert(portable_text_adapter_policy_prepare(RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY,&continuity)==0&&!sleeps);
  ticks=last_activity+15000;
  assert(portable_text_adapter_policy_prepare(RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING,&continuity)==0&&!sleeps);
  assert(portable_text_adapter_policy_prepare(0,&continuity)==1&&!continuity);
  uint32_t flags=0;
  if(!strcmp(fault,"modal-retained"))sleep_status=-2;
  if(!strcmp(fault,"modal-failure"))sleep_status=-1;
  if(!strcmp(fault,"modal-host-retained"))retained();
  int result=portable_text_adapter_policy_apply(continuity,&flags);
  if(sleep_status<0||terminal){
   assert(result==RISC_TEXT_ENTRY_RETAINED&&terminal&&retains==1);
   assert(!memcmp(&draft,&settings_draft,sizeof(draft))&&!writes&&!kv_writes);
   unsigned resources=grants;app_module_fini();assert(grants==resources);
   assert(diagnostics==diagnostic_before);printf("foreground %s: modal terminal no-I/O PASS\n",fault);return 0;
  }
  assert(!result);
  assert(sleeps==1&&text_input_suspended&&!subscriptions&&grants==detached_grants);
  assert(flags==(RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED));
  assert(!memcmp(&draft,&settings_draft,sizeof(draft))&&!writes&&!kv_writes);
  assert(portable_text_adapter_resume()==RISC_TEXT_ENTRY_OK);finish();puts("foreground modal: draft and custody preserved PASS");return 0;
 }
 if(!strcmp(fault,"touch-held")){
  touch.down=true;ticks=last_activity+15000;assert(!foreground_idle_due(ticks,false));
  touch.down=false;
 }
 ticks=last_activity+14998;assert(step()&&!sleeps);
 ticks=last_activity+15000;
 bool ok=step();
 if(terminal){
  assert(!ok&&retains==1&&native_custody_retained&&failed);
  assert(!memcmp(&draft,&settings_draft,sizeof(draft))&&!writes&&!kv_writes&&!return_launches);
  assert(diagnostics==diagnostic_before);unsigned resources=grants;
  app_module_fini();assert(grants==resources&&retains==1);
  assert(!step());printf("foreground %s: terminal no-I/O state preservation PASS\n",fault);return 0;
 }
 assert(ok&&!memcmp(&draft,&settings_draft,sizeof(draft)));
 if(work||audio_work||radio_work||wifi_work||surface.frame||service_status==RISC_RUNTIME_CHECKPOINT_BUSY||!timeout_ms||timeout_ms>INT32_MAX||!foreground_idle_configured){
  assert(!sleeps&&grants==before);
  work=audio_work=radio_work=wifi_work=false;service_status=RISC_RUNTIME_CHECKPOINT_OK;timeout_ms=15000;
  foreground_idle_configured=true;
  if(surface.frame)present(false);
  assert(step()&&sleeps==1);
 }else assert(sleeps==1);
 assert(grants==before&&subscriptions==1&&!radio_stops&&!audio_stops);
 hold_wake=true;ticks+=100;assert(step());assert(!return_launches);
 hold_wake=false;assert(step());assert(step());
 ticks=last_activity+15000;assert(step()&&sleeps==2);
 assert(!memcmp(&draft,&settings_draft,sizeof(draft)));
 finish();printf("foreground %s: returning idle draft/work/input/custody PASS\n",fault);return 0;
}
