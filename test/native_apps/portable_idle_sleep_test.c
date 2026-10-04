/* Generic adapter boundary tests. Reuse the existing provider mocks, but keep
 * all sleep policy/hardware out of this fixture. The fake hook returns through
 * the real Settings call stack, including its nested editor event loops. */
#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_INPUT_NAVIGATION_LOCAL
#define PORTABLE_RTC_UTC8_DENVER
#define PORTABLE_RETURN_APP "springboard.elf"
#define main original_settings_fixture_main
#include "portable_settings_test.c"
#undef main

static unsigned test_case,sleeps,sleep_at,last_resume,nav_at;
static unsigned expected_page,expected_choice,expected_grants;
static unsigned stale_keys_drained,fresh_backs;
static bool jump_done,wake_key;
static int sleep_result=1;
static twatch_rtc_time_v1 expected_draft;
static risc_runtime_api_v1 idle_runtime;

static void assert_draft(void){
 assert(settings_draft.year==expected_draft.year && settings_draft.month==expected_draft.month &&
        settings_draft.day==expected_draft.day && settings_draft.hour==expected_draft.hour &&
        settings_draft.minute==expected_draft.minute && settings_draft.second==expected_draft.second);
}
static bool idle_nav_reset(void *c){
 if(wake_key){stale_keys_drained++;wake_key=false;}
 return nav_reset(c);
}
static bool idle_nav_poll(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(wake_key)out->buttons=out->pressed=RISC_NAV_BACK;
 if(nav_at && polls==nav_at){
  if(test_case==5){assert(sv_page==SV_VALUE);assert_draft();}
  out->pressed=out->released=RISC_NAV_BACK;fresh_backs++;
 }
 return true;
}
static const risc_input_navigation_api_v1 idle_navigation={
 1,sizeof(idle_navigation),NULL,idle_nav_poll,nav_foreground,idle_nav_reset
};
static void idle_yield(uint32_t ms){
 test_yield(ms);
 if((test_case==5 || test_case==6) && !jump_done &&
    ((test_case==5 && sv_page==SV_VALUE && settings_draft.year==2025) ||
     (test_case==6 && sv_page==SV_SLEEP && sleep_choice==PORTABLE_SLEEP_LIGHT))){
  expected_page=sv_page;expected_choice=sleep_choice;expected_draft=settings_draft;
  expected_grants=grants;ticks+=60000;jump_done=true;
 }
}
int portable_app_sleep(const risc_runtime_api_v1 *runtime,const risc_display_output_api_v1 *output,
                       const risc_battery_gauge_api_v1 *battery){
 assert(runtime==&idle_runtime && output==&display_api && !battery);
 assert(!surface.frame && !frame_count && !subscriptions && !touch.subscription && !touch.grant.api);
 assert(rtc_grant.api==&rtc_api && settings_grant.api==&kv_api && settings_store==&kv_api);
 assert(!writes && !kv_writes && !return_launches);
 if(expected_grants)assert(grants==expected_grants-1); /* Only touch was released. */
 sleep_at=ticks;sleeps++;
 if(test_case==5 || test_case==6){
  assert(sv_page==expected_page);assert_draft();assert(sleep_choice==expected_choice);
  /* Wake over a real actionable control and hold it through two polls. The
   * reopened touch stream must require release/neutral before accepting it. */
  tap(polls+1,test_case==5?196:100,test_case==5?80:116);
  tap(polls+2,test_case==5?196:100,test_case==5?80:116);
  wake_key=true; /* Post-wake navigation reset must discard this completed key. */
  if(test_case==5){nav_at=polls+5;tap(polls+7,60,213);}
  else tap(polls+5,60,213);
 }
 ticks+=3100;last_resume=ticks;
 return sleep_result;
}
static void begin(void){
 scenario=100+test_case;assert(app_module_init()==0);
 idle_runtime=runtime_api;idle_runtime.yield_ms=idle_yield;rt=&idle_runtime;
 navigation=&idle_navigation;settings_render(0,0);
 expected_grants=grants;
}
static t5_app_input_t step(void){
 t5_app_input_t input;assert(poll(&input,20));return input;
}
static void quiet_step(void){
 t5_app_input_t input=step();assert(!input.exit_requested && !input.buttons && !input.tapped);
}
static void finish(void){
 assert(!failed);app_module_fini();assert(!grants && !subscriptions && !frame_count);
 assert(!writes && !kv_writes);
}
static void idle_after_drag(void){
 begin();tap(2,100,160);tap(3,100,90);
 quiet_step();quiet_step();quiet_step();quiet_step();
 assert(touch.moved && !touch.down);unsigned released_at=last_activity;
 quiet_step();assert(input_sample.moved && !input_sample.down && !input_sample.released);
 assert(last_activity==released_at); /* Historical moved bit is not activity. */
 ticks=released_at+59998;quiet_step();assert(!sleeps);
 ticks=released_at+60000;quiet_step();assert(sleeps==1 && sleep_at>=released_at+60000);
 assert(last_activity==last_resume && grants==expected_grants && subscriptions==1);
 assert(sv_page==SV_ROOT && sv_scroll[0]>0);finish();
}
static void repeated_or_refused(void){
 begin();if(test_case==2)sleep_result=0;
 quiet_step();
 for(unsigned n=1;n<=3;n++){
  unsigned prior=last_activity;ticks=prior+59998;quiet_step();assert(sleeps==n-1);
  ticks=prior+60000;quiet_step();assert(sleeps==n);
  assert(last_activity==last_resume && subscriptions==1 && grants==expected_grants);
  quiet_step();assert(sleeps==n); /* Neither refusal nor wake immediately retries. */
 }
 finish();
}
static void held_contact(void){
 begin();for(unsigned p=1;p<=8;p++)tap(p,100,110);
 for(unsigned p=1;p<=8;p++){
  ticks+=61000;quiet_step();assert(!sleeps && touch.down && last_activity==ticks);
 }
 quiet_step();assert(!sleeps && !touch.down);unsigned released=last_activity;
 ticks=released+60000;quiet_step();assert(sleeps==1);finish();
}
static void pending_frame(void){
 begin();quiet_step();clear();assert(surface.frame && frame_count);
 ticks=last_activity+60000;quiet_step();assert(!sleeps && frame_count);
 present(false);quiet_step();assert(sleeps==1 && !frame_count);finish();
}
static void nested_settings(void){
 begin();
 if(test_case==5){tap(2,80,80);tap(5,196,80);} /* Year field, then increment. */
 else tap(2,100,80); /* Change Hybrid draft to Light, but never Save. */
 uint8_t result=settings_activate(0,test_case==5?0:4);
 assert(jump_done && sleeps==1 && result==T5_APP_SETTING_NO_CHANGE);
 assert(stale_keys_drained==1 && !wake_key && sv_page==SV_ROOT);
 assert_draft();assert(sleep_choice==expected_choice);
 assert(!writes && !kv_writes && !return_launches && grants==expected_grants);
 if(test_case==5)assert(fresh_backs==1 && settings_draft.year==2025);
 else assert(sleep_choice==PORTABLE_SLEEP_LIGHT);
 /* A deliberately fresh root Back still returns once, after nested Cancel
  * returned locally and the waking Back was discarded. */
 nav_at=0;quiet_step();test_case=7;nav_at=polls+1;
 t5_app_input_t back=step();assert(back.exit_requested && (back.buttons&T5_APP_BUTTON_BACK));
 assert(return_launches==1);finish();
}
static void wraparound(void){
 begin();ticks=UINT32_MAX-30000u;last_activity=ticks;last_poll_at=ticks;
 quiet_step();ticks+=59970;quiet_step();assert(!sleeps);
 ticks+=40;quiet_step();assert(sleeps==1);finish();
}
int main(int argc,char **argv){
 assert(argc==2);test_case=(unsigned)atoi(argv[1]);
 if(test_case==0)idle_after_drag();
 else if(test_case==1 || test_case==2)repeated_or_refused();
 else if(test_case==3)held_contact();
 else if(test_case==4)pending_frame();
 else if(test_case==5 || test_case==6)nested_settings();
 else if(test_case==7)wraparound();
 else assert(!"Unknown idle scenario");
 printf("Portable idle scenario %s: %u sleep entries, nested state/input and clean ownership passed\n",argv[1],sleeps);
 return 0;
}
