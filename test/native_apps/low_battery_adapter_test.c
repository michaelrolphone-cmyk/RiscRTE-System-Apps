#define PORTABLE_RADIO_SESSION
#define PORTABLE_QUICK_FIXTURE_MAIN original_quick_main
#include "quick_adapter_test.c"
static bool app_radio_owned,app_radio_close_fail,app_radio_uncertain;
static unsigned app_radio_stops;
bool portable_radio_services_safe(void){return !app_radio_uncertain;}
bool portable_radio_suspend(void){
 if(app_radio_close_fail){app_radio_uncertain=true;return false;}
 if(app_radio_owned){app_radio_owned=false;app_radio_stops++;}
 return true;
}
static risc_battery_sample_v1 low_sample={3900,50,0};
static bool sample_error;
static bool sample_read(void *c,risc_battery_sample_v1 *out){(void)c;if(sample_error)return false;*out=low_sample;return true;}
static const risc_battery_gauge_api_v1 low_gauge={1,sizeof(low_gauge),NULL,sample_read};
static void sample_at(unsigned percent,unsigned flags){ticks+=5001;low_sample=(risc_battery_sample_v1){3600,(uint8_t)percent,(uint8_t)flags};assert(low_battery_poll());}
static uint8_t timer_choose(unsigned row,bool add,bool cancel){
 input_count=0;polls=0;
 if(add)tap(3,190,100);
 tap(7,cancel?60:170,210);tap(8,cancel?60:170,210);tap(12,60,210);
 settings_render(0,(int32_t)row+1);return settings_activate(0,row);
}
int main(int argc,char **argv){
 assert(argc>=2);if(argc>2)capture_directory=argv[2];unsigned test=(unsigned)atoi(argv[1]);setup();gauge=&low_gauge;
 assert(quick.idle_ms==60000&&quick.deep_ms==300000&&portable_idle_ms()==60000);
 if(test==0){
  sample_at(10,0);assert(!low_writes[0]);sample_at(9,0);
  assert(quick.brightness==15&&quick.ui.brightness==15&&hardware_brightness==15);
  assert(quick.idle_ms==20000&&quick.deep_ms==60000&&portable_idle_ms()==20000&&quick.deep_ms==60000);
  assert(!quick.ui.wifi_enabled&&!quick.ui.bluetooth_enabled&&!ble_state);
  assert(low_writes[0]==1&&low_writes[1]==1&&low_writes[2]==1&&pref_writes[0]==1);
  quick.ui.action_brightness=80;assert(quick_apply(PQA_BRIGHTNESS_COMMIT));
  assert(quick_apply(PQA_WIFI|PQA_BLUETOOTH)); /* One explicit toggle at a time. */
  assert(quick_apply(PQA_BLUETOOTH));
  assert(portable_sleep_timer_save(&quick_kv_api,false,120000));assert(pqa_session_load(&quick,rt));
  sample_at(8,0);assert(quick.brightness==80&&hardware_brightness==80&&quick.idle_ms==120000);
  assert(quick.ui.wifi_enabled&&quick.ui.bluetooth_enabled&&ble_state==1);
  low_battery=(portable_low_battery){0};sample_at(7,RISC_BATTERY_CHARGING);
  assert(quick.brightness==80&&quick.ui.wifi_enabled&&low_writes[0]==1);
  sample_at(10,0);sample_at(9,0);assert(quick.brightness==15&&quick.idle_ms==20000&&!quick.ui.wifi_enabled);
 }else if(test==1){
  sample_error=true;sample_at(0,0);sample_error=false;sample_at(255,0);sample_at(0,RISC_BATTERY_PROFILE_MISSING);sample_at(0,PORTABLE_POWER_STATUS_VALID);
  assert(!low_battery.observed&&!low_writes[0]&&quick.brightness==40);
  sample_at(9,PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT);assert(quick.brightness==15);
 }else if(test==2){
  sample_at(9,0);
  t5_app_setting_t row;assert(settings_get(0,SETTINGS_TIMER_ROW,&row)&&!strcmp(row.label,"Idle sleep timer")&&!strcmp(row.value,"20 seconds"));
  assert(settings_get(0,SETTINGS_TIMER_ROW+1,&row)&&!strcmp(row.label,"Deep sleep timer")&&!strcmp(row.value,"60 seconds"));
  assert(timer_choose(SETTINGS_TIMER_ROW,true,false)==T5_APP_SETTING_UPDATED);
  assert(quick.idle_ms==25000&&portable_idle_ms()==25000&&quick.deep_ms==60000);
  assert(timer_choose(SETTINGS_TIMER_ROW+1,true,false)==T5_APP_SETTING_UPDATED);
  assert(quick.deep_ms==120000&&quick.deep_ms==120000);
  assert(timer_choose(SETTINGS_TIMER_ROW,true,true)==T5_APP_SETTING_NO_CHANGE&&quick.idle_ms==25000);
  sample_at(8,0);assert(quick.idle_ms==25000&&quick.deep_ms==120000);
  unsigned n=low_writes[1];assert(timer_choose(SETTINGS_TIMER_ROW,false,false)==T5_APP_SETTING_UPDATED&&low_writes[1]==n);
 }else if(test==3){
  sample_at(9,0);low_write_fail=1;
  assert(timer_choose(SETTINGS_TIMER_ROW,true,false)==T5_APP_SETTING_NO_CHANGE);
  assert(quick.idle_ms==20000&&!strcmp(timer_message,"Save unconfirmed - retry"));
  low_write_fail=-1;assert(timer_choose(SETTINGS_TIMER_ROW,true,false)==T5_APP_SETTING_UPDATED&&quick.idle_ms==25000);
 }else if(test==4){
  low_write_fail=1;sample_at(9,0);assert(quick.idle_ms==60000&&quick.deep_ms==60000&&quick.brightness==15);
  low_write_fail=-1;sample_at(8,0);assert(quick.idle_ms==60000&&low_writes[1]==1);
 }else if(test==5){
  /* The real poll, rather than a test-only policy call, applies the edge. */
  low_sample.percent=9;t5_app_input_t input;assert(poll(&input,1));assert(quick.brightness==15);
  last_activity=ticks;native_sleep_calls=0;ticks+=19998;assert(poll(&input,1)&&!native_sleep_calls);
  ticks+=2;assert(!poll(&input,1)&&native_sleep_calls==1&&native_sleep_retained);
  /* Retained invocation deliberately cannot run normal cleanup. */
  puts("Low-battery adapter: exact 20-second idle boundary retains native failure");return 0;
 }else if(test==7){
  app_radio_owned=true;sample_at(9,0);assert(!app_radio_owned&&app_radio_stops==1&&quick.brightness==15);
  quick.ui.action_brightness=80;assert(quick_apply(PQA_BRIGHTNESS_COMMIT));app_radio_owned=true;
  sample_at(8,0);assert(app_radio_owned&&app_radio_stops==1&&quick.brightness==80);
  app_radio_owned=false;
 }else if(test==8){
  app_radio_owned=app_radio_close_fail=true;low_sample.percent=9;
  unsigned calls=brightness_calls;assert(!low_battery_poll());
  assert(app_radio_uncertain&&app_radio_owned&&brightness_calls==calls&&low_writes[0]==1);
  puts("Low battery: unconfirmed owned-radio cleanup stops before hardware refresh");return 0;
 }else if(test==9){
  quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.torch=true;
  low_sample.percent=9;bool consumed=false;assert(quick_foreground(&consumed));
  assert(!quick.ui.torch&&!pqa_visible(&quick.ui)&&quick.brightness==15&&hardware_brightness==15&&low_writes[0]==1);
 }else if(test==10){
  /* No completion/readiness callback is allowed to drive this policy edge. */
  display_settled=false;paper_token=42;assert(!surface.frame);
  low_sample.percent=9;assert(low_battery_poll());
  assert(paper_token==42&&!display_settled&&quick.brightness==15&&hardware_brightness==15&&low_writes[0]==1);
  paper_token=0;display_settled=true;
 }else if(test==6){
  sample_at(9,0);settings_render(0,SETTINGS_TIMER_ROW+1);capture_frame("timer-settings");
  settings_render(0,SETTINGS_TIMER_ROW+2);capture_frame("deep-timer-settings");
  timer_deep=false;timer_choice=20000;sv_switch(SV_TIMER);capture_frame("sleep-timer");
  timer_deep=true;timer_choice=60000;sv_switch(SV_TIMER);capture_frame("deep-sleep-timer");
 }else assert(!"unknown test");
 gauge=NULL;cleanup();puts("Low-battery real adapter/Settings: persisted crossing, actual hardware controls and manual timer parity passed");
}
