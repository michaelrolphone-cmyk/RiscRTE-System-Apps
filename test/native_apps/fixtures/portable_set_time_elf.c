/* Link-only proof, not a deployable app or controller retention implementation. */
#include "PortableSetTime.h"
static volatile int result;
static int guard(void *p){return p?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_UNCONFIRMED;}
__attribute__((visibility("default"))) void app_main(void) {
 portable_set_time_client c={0};portable_set_time_plan plan;portable_set_time_result outcome;
 portable_set_time_request p={.local={2026,1,1,12,0,0,0},.basis={true,0},.fold_choice=-1,
                            .native_readback_budget_us=1000000};
 int foreground=1;result=portable_timezone_parse("UTC0",5,&p.rule);
 result=portable_set_time_prepare(&p,&plan);
 result=portable_set_time_open(&c,risc_runtime_get_api(1),guard,&foreground);
 result=portable_set_time_apply_confirmed(&c,&p,&outcome);
 result=portable_set_time_close(&c);portable_set_time_stop(&c,true);
}
