/* Production shared adapter over bounded doubles. No radio, storage or network
 * device is touched; blocked vendor work is represented by an owned operation. */
#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
static bool operation_owned,worker_busy,stop_requested,can_quiesce,timezone_invalid;
static unsigned suspends,closes,service_begins,service_ends,service_depth;
static unsigned alarm_calls,ack_calls,kv_calls,backend_calls,policy_writes,ble_calls;
static alarm_token_v1 acknowledged;
static risc_battery_sample_v1 battery_sample={3900,70,0};
static struct {char key[32];uint8_t bytes[64];uint32_t size;} records[16];
static unsigned records_used;
#ifdef TEST_FILE_SHARING_ADAPTER
static bool sharing_active,retain_on_yield,retain_on_close,retain_on_service_check;
static unsigned quiesce_after_yields,sleep_calls;
bool portable_file_browser_safe(void){return !retained;}
bool portable_file_browser_sharing_active(void){return sharing_active;}
#endif
bool portable_wifi_async_owned(void){return operation_owned;}
bool portable_wifi_stop_pending(void){
#ifdef TEST_FILE_SETUP_ADAPTER
 if(portable_file_browser_cleanup_only())return !retained;
#endif
 return stop_requested&&operation_owned&&!retained;
}
bool portable_wifi_services_safe(void){
#ifdef TEST_FILE_SHARING_ADAPTER
 if(retain_on_service_check){retain_on_service_check=false;portable_adapter_retain_silent();}
#endif
 return !retained;
}
bool portable_wifi_idle_ready(void){return !operation_owned
#ifdef TEST_FILE_SHARING_ADAPTER
 && !sharing_active
#endif
 ;}
bool portable_wifi_services_begin(void){
 assert(!retained);++service_begins;
 if(worker_busy)return false;
 assert(!service_depth);service_depth=1;return true;
}
bool portable_wifi_services_end(void){assert(!retained&&service_depth==1);service_depth=0;++service_ends;return true;}
#ifdef TEST_FILE_SETUP_ADAPTER
static int fixture_setup_close_step(void);
#endif
static bool fixture_suspend(void){
 assert(!retained);++suspends;
#ifdef TEST_FILE_SETUP_ADAPTER
 int setup_closed=fixture_setup_close_step();if(setup_closed>=0)return setup_closed!=0;
#endif
#ifdef TEST_FILE_SHARING_ADAPTER
 if(retain_on_close){retain_on_close=false;portable_adapter_retain_silent();return false;}
#endif
 if(operation_owned){stop_requested=true;if(!can_quiesce)return false;operation_owned=worker_busy=stop_requested=false;}
#ifdef TEST_FILE_SHARING_ADAPTER
 sharing_active=false;
#endif
 return true;
}
#ifndef TEST_FILE_SHARING_ADAPTER
bool portable_wifi_suspend(void){return fixture_suspend();}
#endif
bool portable_wifi_close(void){++closes;return fixture_suspend();}
void portable_wifi_resume(void){}
static void checked_yield(uint32_t ms){
 assert(!service_depth);fx_yield(ms);if(quick_modal)pqa_close(&quick.ui);
#ifdef TEST_FILE_SHARING_ADAPTER
 if(retain_on_yield){retain_on_yield=false;portable_adapter_retain_silent();return;}
 if(quiesce_after_yields && !--quiesce_after_yields)can_quiesce=true;
#endif
}
#ifdef TEST_FILE_SHARING_ADAPTER
int portable_app_alarm_sleep(const risc_runtime_api_v1 *runtime,const risc_display_output_api_v1 *panel,
 const risc_battery_gauge_api_v1 *battery,const alarm_service_v1 *service){
 (void)runtime;(void)panel;(void)battery;(void)service;
 io();assert(!operation_owned&&!sharing_active);++sleep_calls;return 1;
}
#endif
static bool checked_nav(void *c,risc_input_navigation_frame_v1 *out){(void)c;io();*out=(risc_input_navigation_frame_v1){0};return true;}
static const risc_input_navigation_api_v1 checked_navigation={1,sizeof(checked_navigation),NULL,checked_nav,app_foreground,app_reset};
static int32_t checked_get(void *c,const char *key,void *out,uint32_t size,uint32_t *used){
 io();++kv_calls;*used=0;if(worker_busy)return RISC_KEY_VALUE_BUSY;++backend_calls;
 if(!strcmp(key,PORTABLE_TIMEZONE_KEY)){
  int32_t rc=source_get(c,key,out,size,used);
  if(timezone_invalid&&rc==RISC_KEY_VALUE_OK)((uint8_t*)out)[3]^=1;
  return rc;
 }
 for(unsigned i=0;i<records_used;i++)if(!strcmp(records[i].key,key)){
  assert(size>=records[i].size);memcpy(out,records[i].bytes,records[i].size);*used=records[i].size;return RISC_KEY_VALUE_OK;
 }
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t checked_put(void *c,const char *key,const void *data,uint32_t size){
 (void)c;io();++kv_calls;if(worker_busy)return RISC_KEY_VALUE_BUSY;++backend_calls;
 unsigned i;for(i=0;i<records_used;i++)if(!strcmp(records[i].key,key))break;
 if(i==records_used){assert(records_used<16&&strlen(key)<32);strcpy(records[i].key,key);++records_used;}
 assert(size<=64);memcpy(records[i].bytes,data,size);records[i].size=size;
 if(!strcmp(key,PORTABLE_LOW_BATTERY_KEY))++policy_writes;
 if(!strcmp(key,PQA_DND_KEY))++kv_writes;
 return RISC_KEY_VALUE_OK;
}
static void alarm_call(void){io();assert(!worker_busy&&(!operation_owned||service_depth));++alarm_calls;}
static int32_t checked_alarm_step(void *c){(void)c;alarm_call();return alarm_outcome;}
static int32_t checked_alarm_status(void *c,alarm_status_v1 *out){(void)c;alarm_call();*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t checked_alarm_refresh(void *c){(void)c;alarm_call();++refreshes;return ALARM_OK;}
static int32_t checked_alarm_ack(void *c,const alarm_token_v1 *t){(void)c;alarm_call();acknowledged=*t;++ack_calls;return ALARM_OK;}
static bool checked_battery(void *c,risc_battery_sample_v1 *out){(void)c;io();++battery_reads;*out=battery_sample;return true;}
static const risc_battery_gauge_api_v1 checked_gauge={1,sizeof(checked_gauge),NULL,checked_battery};
static bool checked_ble_set(void *c,bool on){(void)c;io();assert(!operation_owned&&!on);++ble_calls;return true;}
static bool checked_ble_status(void *c,uint8_t *out){(void)c;io();assert(!operation_owned);*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 checked_ble={.api_version=1,.struct_size=sizeof(checked_ble),.set_enabled=checked_ble_set,.status=checked_ble_status};
static bool checked_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!strcmp(name,"bluetooth.hci")){
  assert(version==1&&instance==16);io();++acquires;++live;
  *out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=&checked_ble};return true;
 }
 bool ok=app_acquire(name,version,instance,out);
 if(ok&&out->api==&app_navigation)out->api=&checked_navigation;
 if(ok&&out->api==&fx_gauge)out->api=&checked_gauge;
 return ok;
}
static bool checked_launch(const char *name){io();assert(!operation_owned);assert(!strcmp(name,"default.elf")||!strcmp(name,"wifi_settings.elf")||!strcmp(name,"parent.elf"));++launches;return true;}
static void start_operation(void){assert(portable_broadcast_stop());operation_owned=worker_busy=true;can_quiesce=stop_requested=false;
#ifdef TEST_FILE_SHARING_ADAPTER
 sharing_active=true;
#endif
}
static void paint(void){clear();label(10,20,300,"Wi-Fi fixture");present(false);assert(portable_paper_frame_drain()&&alarm_pixels_valid);}
#ifndef TEST_FILE_SETUP_ADAPTER
int main(int argc,char **argv){
 assert(argc==2);const char *name=argv[1];scenario="valid";
 source_kv.get=checked_get;source_kv.put=checked_put;
 tagged.base.step=checked_alarm_step;tagged.base.status=checked_alarm_status;tagged.base.refresh=checked_alarm_refresh;tagged.base.acknowledge=checked_alarm_ack;
 fx_runtime.acquire=checked_acquire;fx_runtime.release=app_release;fx_runtime.request_launch=checked_launch;fx_runtime.yield_ms=checked_yield;
#ifdef TEST_FILE_SHARING_ADAPTER
 app_display=fx_display;app_display.submit=app_submit;
#endif
 assert(app_module_init()==0);paint();
 if(!strcmp(name,"kv-busy")){
  start_operation();risc_runtime_capability_v1 g={.struct_size=sizeof(g)};assert(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g));
  const risc_key_value_v1 *kv=g.api;uint8_t b=1;uint32_t used=0;unsigned before=backend_calls;
  assert(kv->get(kv->context,"fake",&b,1,&used)==RISC_KEY_VALUE_BUSY);
  assert(kv->put(kv->context,"fake",&b,1)==RISC_KEY_VALUE_BUSY);
  assert(backend_calls==before&&!retained);assert(rt->release(&g));
 }else if(!strcmp(name,"time-cache")||!strcmp(name,"time-cold")||!strcmp(name,"time-invalid-busy")){
  twatch_rtc_time_v1 before={0},after={0};bool cached=strcmp(name,"time-cold")!=0;
  if(cached)assert(portable_app_native_local_time(&before));
  if(!strcmp(name,"time-invalid-busy")) {
   timezone_invalid=true;assert(!portable_app_native_local_time(&after));timezone_invalid=false;cached=false;
  }
  start_operation();unsigned reads=native_reads;
  assert(portable_app_native_local_time(&after)==cached);if(cached)assert(!memcmp(&before,&after,sizeof(before)));
  assert(native_reads==reads&&!retained);operation_owned=worker_busy=false;zone="Asia/Kathmandu";
  assert(portable_app_native_local_time(&after)&&after.hour==0&&after.minute==45);
 }else if(!strcmp(name,"broadcast")){
  start_operation();unsigned steps=fixture_broadcast_steps,pauses=fixture_broadcast_pauses,io_before=backend_calls;
  for(unsigned i=0;i<100;i++){assert(broadcast_tick());assert(portable_broadcast_stop());}
  assert(steps==fixture_broadcast_steps&&pauses==fixture_broadcast_pauses&&backend_calls==io_before&&!retained);
  operation_owned=worker_busy=false;
#ifdef TEST_FILE_SHARING_ADAPTER
  assert(broadcast_tick()&&fixture_broadcast_steps==steps);sharing_active=false;
#endif
  assert(broadcast_tick()&&fixture_broadcast_steps==steps+1);
 }else if(!strcmp(name,"alarm-busy")||!strcmp(name,"alarm-connected")||!strcmp(name,"alarm-retained")){
  start_operation();unsigned before=alarm_calls;
  alarms.status.state=ALARM_STATE_LOADING;
  for(unsigned i=0;i<100;i++){bool consumed=true;assert(alarm_foreground(&consumed)&&!consumed);assert(!service_depth);}
  assert(alarm_calls==before&&!retained&&!suspends);assert(alarm_refresh()&&alarm_refresh_pending);
  alarm_ack_pending=true;alarm_ack_token=(alarm_token_v1){.generation=23};
  assert(alarm_commit_pending()&&alarm_ack_pending&&!ack_calls);
  worker_busy=false;
  if(!strcmp(name,"alarm-retained"))alarm_outcome=ALARM_RETAINED;
  bool ok=alarm_audio_pump();
  if(alarm_outcome==ALARM_RETAINED){assert(!ok&&retained&&service_depth==1);unsigned calls_before=calls;assert(!alarm_audio_pump()&&calls==calls_before);printf("Wi-Fi shared adapter deferral %s PASS\n",name);return 0;}
  assert(ok&&!service_depth&&!alarm_refresh_pending&&!alarm_ack_pending&&ack_calls==1&&acknowledged.generation==23);
  assert(!suspends&&operation_owned);before=alarm_calls;assert(alarm_audio_pump()&&alarm_calls>before&&!suspends);
 }else if(!strcmp(name,"quick-action")){
  assert(pqa_session_load(&quick,rt));quick.ui.action_dnd=true;start_operation();unsigned writes=kv_writes;
  for(unsigned i=0;i<100;i++)assert(quick_apply(i?0:PQA_DND));
  assert(quick_deferred_actions==PQA_DND&&kv_writes==writes&&!retained);
  quick.ui.action_dnd=false; /* A later UI reload must not replace accepted intent. */
  can_quiesce=true;assert(quick_apply(0)&&!quick_deferred_actions&&kv_writes==writes+1&&quick.dnd_enabled);
  assert(quick_apply(0)&&kv_writes==writes+1);
 }else if(!strcmp(name,"quick-open")){
  quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.neutral_gate=false;quick.ui.gesture=PQA_IDLE;
  start_operation();unsigned before=backend_calls;bool consumed=false;
  assert(quick_foreground(&consumed)&&consumed&&!quick_modal&&backend_calls==before&&pqa_visible(&quick.ui));
  can_quiesce=true;assert(quick_foreground(&consumed)&&!operation_owned&&!quick_modal&&!retained);
 }else if(!strcmp(name,"quick-handoff")){
  start_operation();assert(quick_dispatch_destination("wifi_settings.elf")&&!launches&&quick_deferred_destination);
  bool consumed=false;for(unsigned i=0;i<100;i++)assert(quick_foreground(&consumed)&&consumed&&!launches);
  can_quiesce=true;assert(quick_foreground(&consumed)&&launches==1&&!quick_deferred_destination);
 }else if(!strcmp(name,"home")||!strcmp(name,"return")){
  start_operation();t5_app_input_t input;
  if(!strcmp(name,"home"))home_pending=true;else navigation_pending=T5_APP_BUTTON_BACK;
  assert(poll(&input,1)&&!launches&&wifi_deferred_return);
  for(unsigned i=0;i<5;i++)assert(poll(&input,1)&&!launches&&wifi_deferred_return);
  can_quiesce=true;assert(poll(&input,1)&&launches==1&&input.exit_requested&&!wifi_deferred_return);
 }else if(!strcmp(name,"battery-low")||!strcmp(name,"battery-high")||!strcmp(name,"battery-rearm")){
  bool high=!strcmp(name,"battery-high"),rearm=!strcmp(name,"battery-rearm");
  if(rearm){low_battery.observed=low_battery.low=true;const uint8_t b[]={0x42,1,1,0xa4};assert(checked_put(NULL,PORTABLE_LOW_BATTERY_KEY,b,4)==0);}
  battery_sample.percent=high||rearm?70:9;start_operation();unsigned writes=policy_writes,before=backend_calls;
  for(unsigned i=0;i<100;i++)assert(low_battery_poll());
  assert(policy_writes==writes&&backend_calls==before&&!retained);
  assert(low_battery_transition_pending&&low_battery.observed==rearm);
  if(high||rearm)assert(!suspends);else assert(suspends);
  can_quiesce=true;worker_busy=false;battery_sample.percent=high?9:70;assert(low_battery_poll());
  assert(!low_battery_transition_pending&&low_battery.observed&&low_battery.low==(!high&&!rearm));
  assert(policy_writes==writes+(high?0u:1u));
  if(high||rearm)assert(operation_owned&&!suspends);else assert(!operation_owned);
  assert(!service_depth);unsigned after=policy_writes;assert(low_battery_poll()&&policy_writes==after);
#ifdef TEST_FILE_SHARING_ADAPTER
 }else if(!strcmp(name,"sharing-idle")){
  start_operation();unsigned before=sleep_calls;t5_app_input_t input;
  ticks=last_activity+portable_idle_ms()+1;
  for(unsigned i=0;i<10;i++)assert(poll(&input,1));
  assert(sleep_calls==before&&operation_owned&&sharing_active&&!suspends&&!retained);
 }else if(!strcmp(name,"sharing-borrowed-quick")){
  sharing_active=true;assert(!portable_wifi_async_owned());
  assert(pqa_session_load(&quick,rt));quick.ui.action_dnd=true;
  assert(quick_apply(PQA_DND)&&!sharing_active&&closes&&!retained);
 }else if(!strcmp(name,"sharing-sleep")){
  start_operation();crown_pending=true;t5_app_input_t input;
  assert(poll(&input,1)&&wifi_deferred_sleep&&!sleep_calls&&!retained);
  for(unsigned i=0;i<10;i++)assert(poll(&input,1)&&wifi_deferred_sleep&&!sleep_calls&&!retained);
  can_quiesce=true;assert(poll(&input,1)&&!wifi_deferred_sleep&&sleep_calls==1&&!retained);
  assert(poll(&input,1)&&sleep_calls==1);
 }else if(!strcmp(name,"sharing-sleep-retained")||!strcmp(name,"sharing-home-retained")||!strcmp(name,"sharing-quick-retained")){
  start_operation();retain_on_close=true;t5_app_input_t input;
  if(!strcmp(name,"sharing-sleep-retained")){crown_pending=true;assert(!poll(&input,1));}
  else if(!strcmp(name,"sharing-home-retained")){home_pending=true;assert(!poll(&input,1));}
  else assert(!quick_dispatch_destination("wifi_settings.elf"));
  assert(retained&&operation_owned&&!launches&&!sleep_calls);
  assert(calls==callback_calls&&free_calls==callback_frees&&presents==callback_presents);
  app_module_fini();assert(calls==callback_calls);printf("Files shared adapter deferral %s PASS\n",name);return 0;
 }else if(!strcmp(name,"sharing-battery-retained")){
  start_operation();retain_on_service_check=true;assert(!low_battery_poll()&&retained);
  assert(calls==callback_calls&&free_calls==callback_frees&&presents==callback_presents);
  app_module_fini();assert(calls==callback_calls);printf("Files shared adapter deferral %s PASS\n",name);return 0;
 }else if(!strcmp(name,"sharing-fini")||!strcmp(name,"sharing-fini-retained")){
  start_operation();quiesce_after_yields=4;
  retain_on_yield=!strcmp(name,"sharing-fini-retained");
  app_module_fini();
  if(retained){
   assert(operation_owned&&live&&calls==callback_calls&&free_calls==callback_frees&&presents==callback_presents);
   app_module_fini();assert(calls==callback_calls);printf("Files shared adapter deferral %s PASS\n",name);return 0;
  }
  assert(!operation_owned&&!sharing_active&&!live&&!frames&&!subscriptions&&closes>=4);
  printf("Files shared adapter deferral %s PASS\n",name);return 0;
#endif
 }else assert(!"unknown case");
 operation_owned=worker_busy=false;stop_requested=false;
 assert(!retained&&!service_depth);app_module_fini();assert(!live&&!frames&&!subscriptions);
 printf("Wi-Fi shared adapter deferral %s PASS\n",name);return 0;
}

#endif
