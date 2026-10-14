#define RGB_ATOMIC_FIXTURE
#define PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL
#define LOW_BATTERY_FIXTURE_MAIN inherited_low_main
#include "low_battery_adapter_test.c"
int main(int argc,char**argv){
 assert(argc==2);unsigned mode=(unsigned)atoi(argv[1]);setup();gauge=&low_gauge;
 assert(quick.idle_ms==60000&&quick.deep_ms==300000);
 sample_at(9,0);assert(rgb_automatic==5&&!rgb_user&&quick.brightness==15&&quick.idle_ms==20000&&quick.deep_ms==60000);
 if(mode==0){
  assert(timer_choose(SETTINGS_TIMER_ROW,true,false)==T5_APP_SETTING_UPDATED);
  assert(quick.idle_ms==25000&&rgb_user==1);
  assert(timer_choose(SETTINGS_TIMER_ROW+1,true,false)==T5_APP_SETTING_UPDATED);
  assert(quick.deep_ms==120000&&rgb_user==2);
  assert(timer_choose(SETTINGS_TIMER_ROW,true,true)==T5_APP_SETTING_NO_CHANGE&&rgb_user==2);
  assert(timer_choose(SETTINGS_TIMER_ROW,false,false)==T5_APP_SETTING_UPDATED&&rgb_user==3);
  sample_at(8,0);assert(rgb_automatic==5&&quick.idle_ms==25000&&quick.deep_ms==120000);
 }else if(mode==1){
  low_write_fail=1;assert(timer_choose(SETTINGS_TIMER_ROW,true,false)==T5_APP_SETTING_NO_CHANGE);
  assert(quick.idle_ms==20000&&rgb_user==1);
  low_write_fail=-1;assert(timer_choose(SETTINGS_TIMER_ROW,true,false)==T5_APP_SETTING_UPDATED&&quick.idle_ms==25000&&rgb_user==2);
 }else if(mode==2||mode==3){
  if(mode==3){tap(polls+1,182,62);tap(polls+2,182,62);}
  assert(quick_atomic_modal_begin());++quick_atomic_contact;quick.ui.action_brightness=80;
  assert(quick_apply(PQA_BRIGHTNESS_COMMIT));assert(quick.brightness==80&&rgb_user==1);
  if(mode==3){assert(touch.down&&quick.ui.neutral_gate);
   (void)pqa_input(&quick.ui,input_sampled_at+1,true,1,1,182,62,false);
   assert(quick.ui.neutral_gate&&!pqa_take_action(&quick.ui));}
  assert(quick_atomic_close());
  sample_at(8,0);assert(quick.brightness==80&&rgb_automatic==5);
 }else assert(!"unknown mode");
 gauge=NULL;cleanup();assert(!rgb_token);
 printf("Deployed RGB preferences %u: real timer Save/cancel/reentry, explicit same-value intent, one-shot automatic policy, legacy write fence PASS\n",mode);return 0;
}
