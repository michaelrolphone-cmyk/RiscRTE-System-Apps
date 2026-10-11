/* Link-only proof, not a product app. A real integrator supplies the phase
 * guard and calls close before device preparation. This harness is not run. */
#include "PortableRealtimeClient.h"
static volatile int result;
static int foreground_phase(void *context) { return context?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_UNCONFIRMED; }
__attribute__((visibility("default"))) void app_main(void) {
 portable_realtime_client client={0};risc_realtime_snapshot_v1 sample;
 portable_realtime_recovery_policy policy={.fold_choice=-1,.basis={true,0}};
 portable_realtime_recovery_result recovery;portable_realtime_estimate estimate;
 int foreground=1;
 result=portable_timezone_parse("UTC0",5,&policy.rule);
 result=portable_realtime_open(&client,risc_runtime_get_api(1),PORTABLE_REALTIME_CONTROL,
     PORTABLE_REALTIME_NORMAL_START,foreground_phase,&foreground);
 result=portable_realtime_recover_rtc(&client,&policy,&recovery);
 result=portable_realtime_read(&client,&sample);
 result=portable_realtime_close(&client);
 result=portable_realtime_project(&sample,0,1000,&estimate);
 portable_realtime_stop(&client,true);
}
