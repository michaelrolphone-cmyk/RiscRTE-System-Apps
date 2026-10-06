/* The real Quick adapter must restore calibration before requesting Wi-Fi. */
#define PORTABLE_TAP_SETTINGS
#define PORTABLE_QUICK_FIXTURE_MAIN original_quick_main
#include "quick_adapter_test.c"
static bool sensor_enrolled;static unsigned sensor_restores;
static bool restore_sensor(void*c){(void)c;assert(sensor_enrolled);sensor_enrolled=false;sensor_restores++;return true;}
static const twatch_motion_api_v1 motion_api={.api_version=1,.struct_size=sizeof(motion_api),.resume_wake=restore_sensor};
static bool guarded_launch(const char*path){assert(!sensor_enrolled && !tap_grant.api && !settings_motion_active);return quick_launch(path);}
int main(void){
 setup();quick_runtime.request_launch=guarded_launch;
 sensor_enrolled=true;tap_motion=&motion_api;tap_grant=(risc_runtime_capability_v1){.struct_size=sizeof(tap_grant),.api=&motion_api,.slot=200};grants++;
 tap_cleanup_pending=settings_motion_active=true;tap_calibration.phase=TAP_CAL_PAIR;
 opening(3);tap(80,51,185);t5_app_input_t input={0};
 while(!input.exit_requested)assert(poll(&input,8));
 assert(settings_motion_interrupted && sensor_restores==1 && !sensor_enrolled && !tap_cleanup_pending);
 assert(wifi_launches==1 && !return_launches && quick_launch_pending && !tap_calibration.pairs);
 assert(!pref_writes[0] && !pref_writes[1] && !format_writes);
 cleanup();puts("Actual Quick Wi-Fi launch restores calibration ownership before requestLaunch, with no measured taps or writes PASS");
}
