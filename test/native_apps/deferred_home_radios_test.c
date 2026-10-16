/* Real Clock/adapter/Quick radio source, strict native provider doubles.
 * The generated adapter translation unit exposes private gates for isolated
 * lifecycle cases. It does not replace or implement any policy behavior. */
#define SPARSE_FIXTURE_MAIN unused_deferred_sparse_fixture_main
#include "sparse_clock_startup_test.c"
#include "PortableQuickRadios.h"
#include "PortableResidentShell.h"
#include "RiscResidentShellV1.h"
bool portable_paper_quick_open(void);
#include "InstalledAppsV1.h"
#include "RiscAppDataV1.h"

extern bool deferred_test_ready(bool force);
extern bool deferred_test_loaded(void);
extern bool deferred_test_home(void);
extern unsigned deferred_test_pending(void);
extern void deferred_test_busy(unsigned flags);
extern bool deferred_test_submit(void);
extern bool deferred_test_mutable(void);
extern bool deferred_test_low_battery(void);
extern bool deferred_test_queue_token(void);
extern bool deferred_test_quick(void);
extern bool deferred_test_alarm(void);
extern bool deferred_test_launch(void);
void __real_portable_desk_adapter_home_submitted(void);
static const char *deferred_case;
static bool clock_run,force_allowed,policy_active;
static unsigned policy_loads;
static unsigned radio_reads,wifi_acquires,ble_acquires,wifi_off_calls,wifi_status_calls;
static unsigned ble_enable_calls,ble_status_calls,radio_releases,home_markers;
static unsigned completed_frame,home_target,after_load_polls,empty_probes,loading_calls;
static unsigned direct_sleep_calls,guard_launches,cue_steps,nav_samples;
static alarm_service_descriptor_v2 deferred_alarm;
static risc_battery_gauge_api_v1 deferred_battery;
static struct {char key[32];uint8_t bytes[32];uint32_t size;} written[8];
static unsigned written_count;
static risc_resident_callbacks_v1 host_callbacks;
static risc_key_value_v1 deferred_kv;
static wifi_api_v1 deferred_wifi;
static portable_bluetooth_control_v1 deferred_bt;
static bool is_case(const char *name){return !strcmp(deferred_case,name);}
static bool fail_at(const char *name){return !strncmp(deferred_case,"failure-",8)&&!strcmp(deferred_case+8,name);}
static unsigned radio_calls(void) {
 return radio_reads+wifi_acquires+ble_acquires+wifi_off_calls+wifi_status_calls+
  ble_enable_calls+ble_status_calls+radio_releases;
}
static void policy_boundary(void) {
 safe();assert(!portable_desk_adapter_timer_only());
 if(is_case("low-battery-pending"))assert(!deferred_test_mutable());
 else assert(!frames&&!raw_present_pending&&!deferred_test_pending());
 if(clock_run&&!force_allowed)assert(home_markers&&home_target&&completed_frame>=home_target);
}
void __wrap_portable_desk_adapter_home_submitted(void) {
 unsigned before=radio_calls();
 if(!home_markers) {
  unsigned work=deferred_test_pending();
  home_target=(work&1u)?presents:(work? presents+1:presents);
 }
 home_markers++;
 __real_portable_desk_adapter_home_submitted();
 assert(radio_calls()==before); /* Submission notification is memory-only. */
}
bool __real_pqa_radios_load(pqa_radios *,pqa_state *,const risc_runtime_api_v1 *);
bool __wrap_pqa_radios_load(pqa_radios *state,pqa_state *ui,const risc_runtime_api_v1 *rt) {
 policy_boundary();assert(!policy_active);policy_active=true;policy_loads++;
 bool ok=__real_pqa_radios_load(state,ui,rt);policy_active=false;return ok;
}
static int32_t deferred_preferences(void *c,const char *key,void *out,uint32_t cap,uint32_t *used) {
 if(policy_active&&!strcmp(key,"quick_radio")){
  policy_boundary();radio_reads++;
  if(fail_at("kv-context")){*used=0;return RISC_KEY_VALUE_CONTEXT;}
 }
 for(unsigned i=0;i<written_count;i++)if(!strcmp(key,written[i].key)) {
  safe();assert(cap>=written[i].size);memcpy(out,written[i].bytes,written[i].size);*used=written[i].size;return RISC_KEY_VALUE_OK;
 }
 return preferences(c,key,out,cap,used);
}
static int32_t deferred_put(void *c,const char *key,const void *bytes,uint32_t size) {
 if(!is_case("low-battery-pending"))return no_put(c,key,bytes,size);
 safe();assert(!strcmp(key,"battery_edge")||!strcmp(key,"sleep_idle")||!strcmp(key,"sleep_deep")||
  !strcmp(key,"brightness")||!strcmp(key,"quick_radio"));
 assert(strlen(key)<sizeof(written[0].key)&&size<=sizeof(written[0].bytes));
 unsigned i=0;while(i<written_count&&strcmp(key,written[i].key))i++;
 if(i==written_count){assert(written_count<8);written_count++;strcpy(written[i].key,key);}
 memcpy(written[i].bytes,bytes,size);written[i].size=size;return RISC_KEY_VALUE_OK;
}
static bool deferred_battery_read(void *c,risc_battery_sample_v1 *out) {
 bool ok=read_battery(c,out);if(is_case("low-battery-pending"))out->percent=9;return ok;
}
static bool deferred_bright(void *c,uint16_t level,uint16_t max) {
 if(!is_case("low-battery-pending"))return bright(c,level,max);
 (void)c;safe();assert(max==100&&(level==0||level==15||level==40));dark=level==0;return true;
}
static bool deferred_wifi_off(void *c) {
 (void)c;policy_boundary();wifi_off_calls++;return !fail_at("wifi-off");
}
static wifi_link_t deferred_wifi_status(void *c) {
 (void)c;policy_boundary();wifi_status_calls++;
 return fail_at("wifi-status")?WIFI_LINK_JOINING:WIFI_LINK_DOWN;
}
static bool deferred_ble_enable(void *c,bool on) {
 (void)c;policy_boundary();assert(!on);ble_enable_calls++;return !fail_at("ble-enable");
}
static bool deferred_ble_status(void *c,uint8_t *out) {
 (void)c;policy_boundary();ble_status_calls++;
 *out=fail_at("ble-status")?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;
}
static int32_t installed_ok(void *c){(void)c;safe();return INSTALLED_APPS_OK;}
static uint32_t installed_zero(void *c){(void)c;safe();return 0;}
static bool installed_get(void *c,uint32_t i,installed_app_v1 *out){(void)c;(void)i;(void)out;assert(0);return false;}
static int32_t installed_step(void *c,const risc_storage_volume_api_v1 *volume){(void)c;safe();assert(volume);return INSTALLED_APPS_OK;}
static const installed_apps_v1 installed={.api_version=1,.struct_size=sizeof(installed),
 .refresh=installed_ok,.generation=installed_zero,.count=installed_zero,.get=installed_get,
 .step=installed_step,.cancel=installed_ok};
static int32_t empty_stat(void *c,const char *name,uint32_t *size,uint64_t *revision) {
 (void)c;safe();assert(!strcmp(name,"crash-spool.bin"));*size=0;*revision=0;return RISC_APP_DATA_NOT_FOUND;
}
static int32_t empty_read(void *c,const char *n,uint64_t r,void *o,uint32_t cap,uint32_t *s,uint64_t *rev) {
 (void)c;(void)n;(void)r;(void)o;(void)cap;(void)s;(void)rev;assert(0);return RISC_APP_DATA_IO;
}
static int32_t empty_replace(void *c,const char *n,uint64_t r,const void *o,uint32_t s) {
 (void)c;(void)n;(void)r;(void)o;(void)s;assert(0);return RISC_APP_DATA_IO;
}
static const risc_app_data_v1 empty_data={1,sizeof(empty_data),NULL,empty_stat,empty_read,empty_replace};
static int32_t deferred_alarm_step(void *c) {
 safe();
 if(is_case("alarm-cue-defers")) {
  assert(!radio_calls());
  if(++cue_steps==2){alarm_state.state=ALARM_STATE_READY;alarm_state.occurrence=(alarm_token_v1){0};}
 }
 return step_alarm(c);
}
#ifdef PORTABLE_SETTINGS_AVAILABILITY
#include "RiscKeyValueAvailabilityV1.h"
static int32_t deferred_availability(void*c,risc_key_value_availability_state_v1*out){
 (void)c;safe();*out=(risc_key_value_availability_state_v1){.struct_size=sizeof(*out),
  .namespace_id=1,.availability=RISC_KEY_VALUE_AVAILABLE,.generation=1};return RISC_KEY_VALUE_AVAILABILITY_OK;
}
static const risc_key_value_availability_v1 available_settings={1,sizeof(available_settings),NULL,deferred_availability};
#endif
static bool deferred_obtain(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g) {
 safe();assert(grants<16&&g->struct_size==sizeof(*g));
 const void *api=NULL;
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
 /* This lifecycle fixture has no atomic writer. Model a clean optional
  * refusal; current Home must never fall back to legacy preference writes. */
 if(!strcmp(name,"storage.key-value.atomic")){assert(version==1&&id==1);return false;}
#endif
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!strcmp(name,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY)) {
  assert(promoted&&version==1&&id==1);g->api=&available_settings;g->slot=++grants;g->generation=1;return true;
 }
#endif
 if(!strcmp(name,"board.battery")) {
  assert(obtain(name,version,id,g));deferred_battery=*(const risc_battery_gauge_api_v1 *)g->api;
  deferred_battery.read=deferred_battery_read;g->api=&deferred_battery;return true;
 }
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)) {
  assert(obtain(name,version,id,g));deferred_alarm=*(const alarm_service_descriptor_v2 *)g->api;
  deferred_alarm.base.step=deferred_alarm_step;g->api=&deferred_alarm;return true;
 }
 if(!strcmp(name,"net.wifi")) {
  policy_boundary();assert(version==1&&id==15);wifi_acquires++;
  if(fail_at("wifi-acquire"))return false;
  api=&deferred_wifi;
 } else if(!strcmp(name,"bluetooth.hci")) {
  policy_boundary();assert(version==1&&id==16);ble_acquires++;
  if(fail_at("ble-acquire"))return false;
  api=&deferred_bt;
 } else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&id==1) {
  assert(promoted&&version==1);
  if(policy_active&&fail_at("kv-acquire"))return false;
  api=&deferred_kv;
 } else if(!strcmp(name,INSTALLED_APPS_CAPABILITY)) {
  assert(promoted&&version==1&&!id);api=&installed;
 } else if(!strcmp(name,RISC_APP_DATA_CAPABILITY)) {
  assert(promoted&&version==1&&id==62);api=&empty_data;
 } else if(!strcmp(name,"contexts.service")) {
  /* RF Contexts is optional; an explicitly empty unavailable response is the
   * production adapter's checked, non-retaining compatibility path. */
  assert(promoted&&version==1&&!id);return false;
 } else {if(getenv("DEFERRED_TRACE"))fprintf(stderr,"obtain %s v=%u id=%llu\n",name,version,(unsigned long long)id);return obtain(name,version,id,g);}
 g->api=api;g->slot=++grants;g->generation=1;
 if(grants>high_water)high_water=grants;
 return true;
}
static bool deferred_drop(risc_runtime_capability_v1 *g) {
 safe();
 bool radio=g->api==&deferred_wifi||g->api==&deferred_bt;
 if(radio)radio_releases++;
 if((g->api==&deferred_wifi&&fail_at("wifi-release")) ||
    (g->api==&deferred_bt&&fail_at("ble-release")) ||
    (g->api==&deferred_kv&&radio_reads&&fail_at("kv-release")))return false;
 return drop(g);
}
static bool deferred_health(risc_runtime_health_v1 *out) {
 safe();assert(ms<30000u);out->uptime_ms=ms;return true;
}
static bool deferred_touch_poll(void *c,size_t count){(void)c;safe();assert(count==1);polls++;return true;}
static bool deferred_touch_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};return true;
}
static bool deferred_nav(void *c,risc_input_navigation_frame_v1 *out) {
 (void)c;safe();*out=(risc_input_navigation_frame_v1){0};
 if(clock_run&&deferred_test_loaded()&&++after_load_polls==25)longjmp(entry,2);
 if(is_case("force-navigation")&&++nav_samples==3)out->pressed=out->released=RISC_NAV_CONFIRM;
 if((is_case("force-quick")||is_case("alarm-modal-defers"))&&presents>=2)out->pressed=out->released=RISC_NAV_BACK;
 return true;
}
static const risc_input_navigation_api_v1 deferred_navigation={1,sizeof(deferred_navigation),NULL,
 deferred_nav,nav_foreground,nav_reset};
static bool deferred_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g) {
 if(!strcmp(name,"input.navigation")) {
  safe();assert(version==1&&!id&&grants<16);g->api=&deferred_navigation;
  g->slot=++grants;g->generation=1;if(grants>high_water)high_water=grants;return true;
 }
 return deferred_obtain(name,version,id,g);
}
static bool deferred_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token) {
 if(clock_run&&!home_markers)assert(!radio_calls());
 if(loading_calls)assert(deferred_test_loaded());
 return frame_submit(c,f,r,n,o,token);
}
static bool deferred_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
 if(is_case("failed-pending-frame")){safe();return false;}
 bool ok=raw_present_status(c,token,out);
 if(ok&&out->state==RISC_DISPLAY_PRESENT_COMPLETE)completed_frame=(unsigned)token;
 return ok;
}
static bool deferred_wait(void *c,risc_display_present_token_v1 token,uint32_t timeout,risc_display_present_status_v1 *out) {
 bool ok=frame_wait(c,token,timeout,out);
 if(ok&&out->state==RISC_DISPLAY_PRESENT_COMPLETE){completed_frame=(unsigned)token;raw_present_pending=false;}
 return ok;
}
static int32_t register_host(uint64_t invocation,const risc_resident_callbacks_v1 *callbacks) {
 safe();assert(invocation==1&&callbacks&&callbacks->struct_size>=RISC_RESIDENT_CALLBACKS_LOADING_V1_SIZE);
 assert(callbacks->loading);host_callbacks=*callbacks;return RISC_RESIDENT_OK;
}
static int32_t run_child(uint64_t invocation,const char *path,risc_resident_result_v1 *out) {
 safe();assert(invocation==1&&out->struct_size==sizeof(*out));
 if(!path&&!is_case("force-continuation")) {
  empty_probes++;assert(!radio_calls()&&!deferred_test_loaded());
  out->status=RISC_RESIDENT_NO_PENDING;return RISC_RESIDENT_NO_PENDING;
 }
 assert(!radio_calls()&&!deferred_test_loaded()&&!home_markers);
 loading_calls++;
 int status=host_callbacks.loading(host_callbacks.context,path?path:"settings.elf");
 assert(status==RISC_RESIDENT_OK&&deferred_test_loaded()&&radio_reads==1&&completed_frame);
 out->status=RISC_RESIDENT_HANDOFF;return RISC_RESIDENT_HANDOFF;
}
static int32_t request_exit(uint64_t invocation){(void)invocation;assert(0);return RISC_RESIDENT_FAILED;}
static bool get_resident(risc_resident_client_v1 *out) {
 safe();assert(out->struct_size==sizeof(*out));
 *out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=1,
  .role=RISC_RESIDENT_ROLE_HOST,.register_shell=register_host,.run_foreground=run_child,
  .request_foreground_exit=request_exit};return true;
}
static bool deferred_launch(const char *path) {
 safe();assert(!strcmp(path,"settings.elf")&&deferred_test_loaded()&&radio_reads==1);
 guard_launches++;return true;
}
int __real_portable_app_alarm_sleep(const risc_runtime_api_v1 *,const risc_display_output_api_v1 *,
 const risc_battery_gauge_api_v1 *,const alarm_service_v1 *);
int __wrap_portable_app_alarm_sleep(const risc_runtime_api_v1 *rt,
 const risc_display_output_api_v1 *display,const risc_battery_gauge_api_v1 *gauge,const alarm_service_v1 *alarm) {
 if(is_case("force-sleep")) {
  safe();assert(deferred_test_loaded()&&radio_reads==1&&!home_markers);
  direct_sleep_calls++;longjmp(entry,3);
 }
 return __real_portable_app_alarm_sleep(rt,display,gauge,alarm);
}
int __wrap_portable_app_idle_sleep(const risc_runtime_api_v1 *rt,
 const risc_display_output_api_v1 *display,const risc_battery_gauge_api_v1 *gauge,const alarm_service_v1 *alarm) {
 return __wrap_portable_app_alarm_sleep(rt,display,gauge,alarm);
}
static void configure(void) {
 test=clock_run?"cold":"foreground";state_path="/unused-deferred-radio-fixture";
 native_valid=true;in_main=true;start_ms=ms;
 runtime.acquire=deferred_acquire;runtime.release=deferred_drop;runtime.health=deferred_health;
 runtime.resident_shell=get_resident;runtime.request_launch=deferred_launch;
 deferred_kv=kv;deferred_kv.get=deferred_preferences;deferred_kv.put=deferred_put;
 deferred_wifi=wifi;deferred_wifi.disconnect_checked=deferred_wifi_off;deferred_wifi.status=deferred_wifi_status;
 deferred_bt=bt;deferred_bt.set_enabled=deferred_ble_enable;deferred_bt.status=deferred_ble_status;
 panel.history.base=d;panel.history.base.struct_size=sizeof(panel);
 panel.history.base.get_info=raw_display_info;panel.history.base.acquire=frame_acquire;
 panel.history.base.release=frame_release;panel.history.base.submit=deferred_submit;
 panel.history.base.present_status=deferred_status;panel.history.base.wait_present=deferred_wait;
 panel.history.base.set_brightness=deferred_bright;panel.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;
 panel.history.extension_version=1;panel.history.seed_previous=seed_previous;
 panel.power_tag=RISC_DISPLAY_POWER_TAG;panel.power_version=1;panel.prepare=panel_prepare;panel.resume=panel_resume;
 touch_power.base=t;touch_power.base.poll=deferred_touch_poll;touch_power.base.snapshot=deferred_touch_snapshot;
 touch_power.base.struct_size=sizeof(touch_power);touch_power.power_tag=RISC_TOUCH_POWER_TAG;
 touch_power.power_version=1;touch_power.prepare=touch_prepare;touch_power.resume=touch_resume;
 sd.terminal.power.volume.base=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(sd)};
 sd.terminal.extension_tag=RISC_STORAGE_POWER_COMMIT_TAG;sd.terminal.extension_version=1;sd.terminal.commit_power_down=legacy_sd;
 sd.sleep_tag=RISC_STORAGE_SLEEP_TAG;sd.sleep_version=1;sd.prepare_sleep=prepare_sd;sd.commit_sleep=commit_sd;sd.resume_sleep=resume_sd;
 power=(x4_power_deep_v1){{1,sizeof(power),NULL,read_key,NULL},X4_POWER_DEEP_TAG,1,deep};
 assert(app_module_init()==0);assert(!grants&&!starts&&!radio_calls());
}
static void once_loaded(void) {
 assert(deferred_test_loaded()&&policy_loads==1&&radio_reads==1&&wifi_acquires==1&&ble_acquires==1);
 assert(wifi_off_calls==1&&wifi_status_calls==1&&ble_enable_calls==1&&ble_status_calls==1&&radio_releases==2);
 unsigned before=radio_calls();
 for(unsigned i=0;i<10;i++){assert(deferred_test_ready(false));assert(deferred_test_ready(true));}
 assert(radio_calls()==before&&!terminal&&!barriers);
}
#ifndef DEFERRED_FIXTURE_MAIN
#define DEFERRED_FIXTURE_MAIN main
#endif
int DEFERRED_FIXTURE_MAIN(int argc,char **argv) {
 assert(argc==2);deferred_case=argv[1];clock_run=!strncmp(deferred_case,"clock-",6);
 raw_async=is_case("low-battery-pending")||is_case("clock-async")||is_case("force-pending-frame")||is_case("failed-pending-frame");force_allowed=!clock_run;configure();
 if(clock_run) {
  int escaped=setjmp(entry);
  if(!escaped){app_main();assert(!"Clock must reach settled Home with deferred radio policy");}
  assert(escaped==2&&after_load_polls==25&&home_markers&&completed_frame>=home_target&&empty_probes==1);
  once_loaded();printf("%s: actual Clock Home completion precedes one policy load; 25 later neutral polls PASS\n",deferred_case);return 0;
 }
 promoted=promotion_attempted=true;
 bool timer=is_case("timer-never");
 assert(portable_desk_adapter_start(timer?PORTABLE_DESK_START_TIMER:PORTABLE_DESK_START_FOREGROUND)==1);
 assert(!radio_calls()&&!deferred_test_loaded());
 if(timer) {
  for(unsigned i=0;i<10;i++){assert(deferred_test_ready(false));assert(deferred_test_ready(true));portable_desk_adapter_home_submitted();}
  assert(!radio_calls()&&!deferred_test_loaded()&&!deferred_test_home());
 } else if(is_case("settled-once")||is_case("blocked-frames")) {
  for(unsigned i=0;i<5;i++)assert(deferred_test_ready(false));
  assert(!radio_calls());
  portable_desk_adapter_home_submitted();assert(deferred_test_home()&&!radio_calls());
  if(is_case("blocked-frames")) {
   for(unsigned bit=1;bit<=16;bit<<=1){deferred_test_busy(bit);assert(deferred_test_ready(false)==(bit!=4));assert(!radio_calls());}
   deferred_test_busy(0);
  }
  assert(deferred_test_ready(false));once_loaded();
 } else if(is_case("force-pending-frame")) {
  assert(deferred_test_submit()&&deferred_test_pending()&&!radio_calls());
  portable_desk_adapter_home_submitted();assert(deferred_test_ready(false)&&!radio_calls());
  assert(deferred_test_ready(true));assert(completed_frame==1&&!raw_present_pending&&!deferred_test_pending());once_loaded();
 } else if(is_case("alarm-cue-defers")||is_case("alarm-modal-defers")) {
  if(is_case("alarm-modal-defers")){portable_desk_adapter_begin();assert(portable_desk_adapter_present(true));}
  alarm_state.state=is_case("alarm-cue-defers")?ALARM_STATE_CUE:ALARM_STATE_ALERT;
  alarm_state.occurrence=(alarm_token_v1){1,1,1,1};
  assert(deferred_test_alarm());assert(!home_markers&&!radio_calls()&&alarm_state.state==ALARM_STATE_READY);
  assert(deferred_test_ready(false)&&!radio_calls());
  portable_desk_adapter_home_submitted();assert(deferred_test_ready(false));once_loaded();
  if(is_case("alarm-cue-defers"))assert(cue_steps==2&&!presents);else assert(alarm_acks==1&&presents>=3);
 } else if(!strncmp(deferred_case,"failure-",8)||is_case("retained-before-ready")||is_case("failed-pending-frame")) {
  if(is_case("retained-before-ready"))portable_desk_adapter_retain();
  if(is_case("failed-pending-frame"))assert(deferred_test_submit()&&deferred_test_pending());
  assert(!deferred_test_ready(true));assert(portable_app_sleep_retained()&&terminal&&barriers==1&&!deferred_test_loaded());
  unsigned before=radio_calls(),held=grants;
  assert(!deferred_test_ready(true));assert(!deferred_test_ready(false));
  portable_desk_adapter_home_submitted();assert(!portable_desk_adapter_foreground());
  assert(!portable_desk_adapter_sleep());assert(!portable_paper_quick_open());
  app_module_fini();assert(grants==held&&radio_calls()==before&&barriers==1);
  printf("%s: retained once, live grants pinned, no later provider/cleanup calls PASS\n",deferred_case);return 0;
 } else if(is_case("force-quick")) {
  portable_desk_adapter_begin();assert(portable_desk_adapter_present(true));
  assert(portable_paper_quick_open()&&!radio_calls());
  assert(deferred_test_quick());once_loaded();assert(presents>=2&&!home_markers);
 } else if(is_case("low-battery-pending")) {
  assert(deferred_test_submit()&&deferred_test_queue_token());
  assert(raw_present_pending&&!completed_frame&&!home_markers&&!radio_calls());
  assert(deferred_test_low_battery());
  assert(raw_present_pending&&!completed_frame&&!home_markers);
#ifdef PORTABLE_HOME_AUTOMATIC_PREFERENCES
  /* A cleanly unavailable atomic writer neither falls back nor applies radios. */
  assert(!written_count&&!radio_calls()&&!deferred_test_loaded());
#else
  once_loaded();assert(written_count>=4);
#endif
  unsigned before=radio_calls();assert(deferred_test_low_battery()&&radio_calls()==before);
 } else if(is_case("force-navigation")) {
  const t5_app_api_v1 *app=t5_app_get_api(1);assert(app);t5_app_input_t input={0};
  for(unsigned i=0;i<10&&!input.buttons;i++)assert(app->poll(&input,20));
  assert(input.buttons&T5_APP_BUTTON_CONFIRM);once_loaded();assert(!home_markers);
 } else if(is_case("force-launch")) {
  assert(deferred_test_launch()&&guard_launches==1);once_loaded();
 } else if(is_case("force-sleep")) {
  int escaped=setjmp(entry);
  if(!escaped){(void)portable_desk_adapter_sleep();assert(!"Sleep must reach the checked product boundary");}
  assert(escaped==3&&direct_sleep_calls==1);assert(deferred_test_loaded()&&radio_reads==1);
  printf("%s: policy loaded before product sleep boundary PASS\n",deferred_case);return 0;
 } else if(is_case("force-resident")||is_case("force-continuation")||is_case("empty-continuation")) {
  int result=portable_resident_run_foreground(is_case("force-resident")?"settings.elf":NULL);
  if(is_case("empty-continuation"))assert(result==PORTABLE_RESIDENT_NO_PENDING&&empty_probes==1&&!radio_calls());
  else {assert(result==PORTABLE_RESIDENT_HANDOFF&&loading_calls==1);once_loaded();}
 } else assert(!"Unknown test case");
 assert(!terminal&&!barriers);app_module_fini();
 assert(!grants&&!subs&&!frames&&!native_live);
 printf("%s: actual adapter lifecycle and clean release PASS\n",deferred_case);return 0;
}
