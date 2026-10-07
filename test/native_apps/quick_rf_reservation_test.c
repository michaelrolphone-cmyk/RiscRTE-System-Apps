/* Real shared input/controller path with a foreground radio owner. */
#define PORTABLE_RADIO_SESSION
#define PORTABLE_APP_LAUNCH_GUARD
#define PORTABLE_QUICK_FIXTURE_MAIN original_quick_fixture_main
#include "quick_adapter_test.c"

static bool receiver_active = true;
static unsigned receiver_stops;
static bool allow_handoff=true;
static unsigned handoff_checks;
bool portable_app_before_launch(const char *destination) {
 assert(!strcmp(destination,"wifi_settings.elf")||!strcmp(destination,"springboard.elf"));
 ++handoff_checks;
 return allow_handoff;
}
bool portable_radio_services_safe(void) { return true; }
bool portable_radio_suspend(void) {
 receiver_active = false;
 ++receiver_stops;
 return true;
}

int main(void) {
 setup();
 unsigned initial_grants = grants;
 /* A normal app-owned top-edge tap must replay without taking RF custody. */
 tap(3,100,20);
 t5_app_input_t input={0};
 for(unsigned n=0;n<12&&!input.tapped;n++)assert(poll(&input,8));
 assert(input.tapped&&input.touch_x==100&&input.touch_y==20);
 assert(!input.exit_requested&&!input.buttons&&!pqa_visible(&quick.ui));
 assert(receiver_active&&!receiver_stops&&grants==initial_grants);
 assert(!quick_modal&&!quick_background&&!pref_writes[0]&&!pref_writes[1]);
 /* A committed pull still pauses RF, and dismissal never resumes it. */
 opening(20);tap(100,120,222);drain_to(180);background_unchanged();
 assert(!receiver_active&&receiver_stops>0);
 /* A pending app edit vetoes Wi-Fi handoff without poisoning the session. */
 allow_handoff=false;opening(200);tap(280,51,185);drain_to(360);
 assert(handoff_checks==1&&!failed&&!wifi_launches&&!quick_launch_pending);
 allow_handoff=true;opening(380);tap(460,51,185);input=(t5_app_input_t){0};
 while(!input.exit_requested)assert(poll(&input,8));
 assert(handoff_checks==2&&wifi_launches==1&&!return_launches&&!failed);
 cleanup();
 puts("RF tap replay, committed modal pause and guarded Wi-Fi refusal/retry PASS");
}
