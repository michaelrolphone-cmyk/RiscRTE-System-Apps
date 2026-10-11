/* Real shared Settings controller: visual-only service never writes a mode. */
#define PORTABLE_ALARM_SETTINGS
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_ALARM_FIXTURE_MAIN original_alarm_fixture_main
#include "portable_alarm_test.c"
int main(void) {
 scenario=100;
 risc_runtime_api_v1 r=runtime_api;r.acquire=acquire_alarm;r.yield_ms=yield_retained;
 rt=&r;dg.struct_size=sizeof(dg);bg.struct_size=sizeof(bg);
 assert(acquire_alarm("display.output",1,0,&dg));display=dg.api;assert(display_info(NULL,&info));
 assert(portable_touch_open(&touch,&r));failed=false;display_settled=true;
 alarm_pixels=malloc(sizeof(framebuffer));assert(alarm_pixels);assert(portable_alarm_open(&alarms,&r));
 alarm_service_outputs_v1 visual={.service=service_api,.output_modes=ALARM_MODE_VISUAL};
 visual.service.struct_size=sizeof(visual);alarms.api=&visual.service;
 alarm_fake.state=ALARM_STATE_READY;alarm_fake.mode=ALARM_MODE_VISUAL;
 assert(settings_open());settings_render(0,0);
 assert(settings_visual_alerts());t5_app_setting_t row;
 assert(settings_get(0,SETTINGS_ALERT_ROW,&row)&&!strcmp(row.value,"VISUAL ONLY"));
 /* Physical-mode tap and Save remain inert; root Back still works normally. */
 tap(3,100,110);tap(7,175,210);tap(11,60,210);
 assert(settings_activate(0,SETTINGS_ALERT_ROW)==T5_APP_SETTING_NO_CHANGE);
 assert(!kv_writes&&!format_writes&&!writes);
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
 puts("Visual-only Settings preserves stored modes and grants; unsupported choices do not write");
 return 0;
}
