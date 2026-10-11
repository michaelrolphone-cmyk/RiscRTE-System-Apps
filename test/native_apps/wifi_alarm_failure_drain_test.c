/* Exercise the compatible non-native failure branch with the real controller,
 * adapter and copied async provider. Native terminal-fence coverage lives in
 * wifi_adapter_deferral_test.c. No device or network calls. */
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
static unsigned failure_yields;
static void failure_yield(uint32_t delay){
 assert(!service_live&&!wifi_service_depth&&!alarm_stops);
 assert(delay==1);++failure_yields;assert(failure_yields<200);fake_yield(delay);
}
int main(void){
 workflow_start(true);draft();wifi_connect();automatic_cleanup_at=100;
 runtime_api.yield_ms=failure_yield;
 assert(!alarm_failure());
 assert(failed&&alarm_failed_cleaned&&!wifi_operation&&!wg.api&&!native_active);
 assert(async_polls==100&&async_cancels>=1&&failure_yields==100&&alarm_stops==1);
 unsigned stopped_count=alarm_stops;assert(!alarm_failure()&&alarm_stops==stopped_count);
 runtime_api.yield_ms=fake_yield;finish();
 puts("Wi-Fi alarm failure pending-to-quiescent drain PASS");return 0;
}
