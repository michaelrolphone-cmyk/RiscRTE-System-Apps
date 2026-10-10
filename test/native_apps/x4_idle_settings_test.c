#include <stdbool.h>
static bool capture_audio,capture_radio;
bool portable_audio_capture_active(void){return capture_audio;}
bool portable_radio_capture_active(void){return capture_radio;}
void portable_audio_capture_resume(void){}
#define main original_settings_main
#include "portable_native_time_settings_test.c"
#undef main
static void admission_gates(void) {
 touch.down=false;quick_modal=alarm_modal=quick_launch_pending=handoff_requested=false;
 quick.ui.position_q8=quick.ui.target_q8=0;quick.ui.gesture=PQA_IDLE;quick.ui.neutral_gate=false;
 assert(automatic_idle_ready());
 touch.down=true;assert(!automatic_idle_ready());touch.down=false;
 quick_modal=true;assert(!automatic_idle_ready());quick_modal=false;
 alarm_modal=true;assert(!automatic_idle_ready());alarm_modal=false;
 quick_launch_pending=true;assert(!automatic_idle_ready());quick_launch_pending=false;
 handoff_requested=true;assert(!automatic_idle_ready());handoff_requested=false;
 display_settled=false;assert(!automatic_idle_ready());display_settled=true;
 paper_token=1;assert(!automatic_idle_ready());paper_token=0;
 surface.frame=1;assert(!automatic_idle_ready());surface.frame=0;
 quick.ui.gesture=PQA_TOP_PENDING;assert(!automatic_idle_ready());quick.ui.gesture=PQA_IDLE;
 quick.ui.position_q8=PQA_OPEN_Q8;assert(!automatic_idle_ready());quick.ui.position_q8=0;
 capture_audio=true;assert(!automatic_idle_ready());capture_audio=false;
 capture_radio=true;assert(!automatic_idle_ready());capture_radio=false;
 assert(automatic_idle_ready());
}
static unsigned idle_entries;
static bool aged;
static int idle_result;
static void idle_settings_yield(void) {
 if(!aged && sv_page==SV_VALUE && settings_draft.year==2027) {
  ticks+=60001;aged=true;
 }
}
int portable_app_idle_sleep(const risc_runtime_api_v1 *runtime_api,const risc_display_output_api_v1 *d,
                            const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
 assert(runtime_api==rt && d==display && b==gauge && a==alarms.api);
 assert(!subscriptions&&!surface.frame&&!frames&&!paper_token&&display_settled);
 assert(sv_page==SV_VALUE && settings_draft.year==2027 && !rtc_writes && !seeds);
 assert(settings_store && settings_grant.api);
 ++idle_entries;ticks+=3000;
 if(idle_result==-2)hidden=true;
 return idle_result;
}
int main(int argc,char **argv) {
 assert(argc==2);idle_result=atoi(argv[1]);test_name="value-edit";zone_id="America/Denver";
 native_epoch=1768505696;expected_epoch=1800041696;
 configure_records();plan_inputs();assert(app_module_init()==0);
 in_main=true;app_main();in_main=false;
 assert(aged && idle_entries==1);
 if(idle_result==-2) {
  assert(retained && barriers==1 && !rtc_writes && !seeds && live);
  unsigned before=provider_calls;app_module_fini();assert(provider_calls==before);
 } else {
  assert(!retained && rtc_writes==1 && written.year==2027 && seeds==1);
  assert(!quick.ui.wifi_enabled && !quick.ui.bluetooth_enabled);
  admission_gates();
  in_fini=true;app_module_fini();in_fini=false;assert(!live&&!subscriptions&&!frames);
 }
 printf("X4 idle %d: real nested Settings draft and original return stack preserved PASS\n",idle_result);
 return 0;
}
