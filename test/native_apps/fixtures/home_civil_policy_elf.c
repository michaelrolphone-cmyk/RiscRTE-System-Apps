/* Target link/loader proof of the actual foreground Home civil adapter.
 * Not a product entrypoint and not executed by the build. */
#include "PortableRealtimeClient.h"
#include <string.h>
static const risc_runtime_api_v1 *rt;
static bool desk_promoted,desk_retained;
static portable_realtime_reconcile_state desk_rtc_check;
static volatile int result;
static int desk_fence(void){desk_retained=true;return -2;}
static int desk_phase_guard(void *p){(void)p;return desk_retained?PORTABLE_REALTIME_GUARD_RETAINED:PORTABLE_REALTIME_GUARD_SAFE;}
static bool desk_halted(int s){return s==PORTABLE_REALTIME_CONTEXT||s==PORTABLE_REALTIME_UNCERTAIN||s==PORTABLE_REALTIME_RETAINED;}
static bool portable_app_sleep_retained(void){return desk_retained;}
#include "../../../Apps/paper_home_civil.inc"
__attribute__((visibility("default"))) void app_main(void){
 rt=risc_runtime_get_api(1);desk_promoted=true;
 portable_realtime_client client={0};portable_realtime_recovery_result recovery;
 risc_civil_calendar_v1 calendar={0};char rule[40]={0};
 result=portable_realtime_open(&client,rt,PORTABLE_REALTIME_CONTROL,
     PORTABLE_REALTIME_NORMAL_START,desk_phase_guard,NULL);
 result=desk_civil_reconcile(&client,&recovery);
 result=desk_civil_project(recovery.snapshot.epoch_seconds,recovery.snapshot.nanoseconds,&calendar);
 result=desk_civil_cached_rule(rule);
 result=portable_realtime_close(&client);
}
