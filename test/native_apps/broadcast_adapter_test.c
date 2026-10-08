/* Production common adapter and radio/low-battery/sleep/modal call ordering. */
#define PORTABLE_QUICK_FIXTURE_MAIN unused_quick_main
#include "quick_adapter_test.c"
static bool publishing;
static unsigned broadcast_steps,broadcast_pauses,pause_failures;
static bool broadcast_pause_fake(void*c){(void)c;assert(!surface.frame&&display_settled);broadcast_pauses++;if(pause_failures){pause_failures--;return false;}publishing=false;return true;}
static bool broadcast_step_fake(void*c,bool allow,const telemetry_broadcast_policy_v1 *policy){(void)c;assert(!surface.frame&&display_settled);broadcast_steps++;publishing=allow&&policy->settings_valid&&policy->enabled&&policy->radios_allowed;return true;}
static bool broadcast_status_fake(void*c,telemetry_broadcast_status_v1*out){(void)c;*out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out),.enabled=true,.settings_valid=true,.state=publishing?TELEMETRY_BROADCAST_LIVE:TELEMETRY_BROADCAST_PAUSED};return true;}
static int32_t broadcast_enum_fake(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;(void)i;(void)out;return 0;}
static int32_t broadcast_read_fake(void*c,uint32_t i,int32_t*out){(void)c;(void)i;(void)out;return 0;}
static const telemetry_broadcast_v1 broadcast_api={1,sizeof(broadcast_api),NULL,broadcast_step_fake,broadcast_pause_fake,broadcast_status_fake,broadcast_enum_fake,broadcast_read_fake};
static risc_battery_sample_v1 sample={3900,50,0};
static bool sample_read(void*c,risc_battery_sample_v1*out){(void)c;*out=sample;return true;}
static const risc_battery_gauge_api_v1 sample_gauge={1,sizeof(sample_gauge),NULL,sample_read};
int main(int argc,char **argv){
 assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);setup();
 broadcast_client.runtime=rt;broadcast_client.policy.struct_size=sizeof(broadcast_client.policy);broadcast_client.api=&broadcast_api;broadcast_client.grant=(risc_runtime_capability_v1){.struct_size=sizeof(broadcast_client.grant),.api=&broadcast_api};grants++;
 assert(quick_apply(PQA_BLUETOOTH));assert(quick.ui.bluetooth_enabled);assert(broadcast_tick()&&publishing);
 broadcast_steps=broadcast_pauses=0;
 if(test==0){
  t5_app_input_t input;assert(poll(&input,8)&&publishing&&broadcast_steps>0&&!broadcast_pauses);
  assert(pqa_radios_load(&quick_radios,&quick.ui,rt)&&publishing); /* no reset across invocation preference reload */
 }else if(test==1){
  opening(3);tap(80,120,222);drain_to(160);assert(publishing&&broadcast_pauses>0&&broadcast_steps>1);background_unchanged();
 }else if(test==2){
  gauge=&sample_gauge;sample.percent=9;assert(low_battery_poll());assert(!publishing&&!quick.ui.bluetooth_enabled&&low_writes[0]==1);
  assert(quick_apply(PQA_BLUETOOTH)&&broadcast_tick()&&publishing);ticks+=5001;sample.percent=8;assert(low_battery_poll()&&publishing&&quick.ui.bluetooth_enabled&&low_writes[0]==1);
  gauge=NULL;
 }else if(test==3){
  assert(!idle_sleep()&&!publishing&&native_sleep_calls==1&&native_sleep_retained);
  puts("Broadcast pauses before native sleep; retained native result keeps grants PASS");return 0;
 }else if(test==4){
  pause_failures=1;unsigned old=ble_state;assert(!quick_apply(PQA_BLUETOOTH)&&publishing&&ble_state==old&&quick.ui.bluetooth_enabled);
  assert(quick_apply(PQA_BLUETOOTH)&&!publishing&&!ble_state&&!quick.ui.bluetooth_enabled);
 }else if(test==5){
  alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};tap(10,100,165);bool consumed=false;
  assert(alarm_foreground(&consumed)&&consumed&&!publishing&&broadcast_pauses>0);
  assert(broadcast_tick()&&publishing);
 }else if(test==6){
  unsigned before=broadcast_pauses;cleanup();assert(publishing&&broadcast_pauses==before);puts("Normal adapter fini releases only grant; publication remains provider-owned PASS");return 0;
 }else if(test==7){
  pause_failures=1;assert(!alarm_failure()&&!publishing&&broadcast_pauses==2);
  app_module_fini();assert(!grants&&!subscriptions&&!frame_count);puts("Failed publication cleanup retains and retries before abnormal app exit PASS");return 0;
 }else if(test==8){
  alarm_fake.state=ALARM_STATE_CUE;alarm_fake.output_uncertain=1;bool consumed=false;
  assert(alarm_foreground(&consumed)&&consumed&&!publishing&&broadcast_pauses>0);assert(broadcast_tick()&&publishing);
 }else assert(!"unknown scenario");
 cleanup();printf("Broadcast real common adapter lifecycle case%u PASS\n",test);
}
