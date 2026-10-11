/* Reuse the real 408-byte Clock/adapter/product-sleep fixture unchanged. */
#include "sparse_points_expiry_test.c"
static bool context_timer_resident_forbidden(struct risc_resident_client_v1 *out) {
    (void)out;
    assert(!"Sparse TIMER must not bind the resident foreground host");
    return false;
}
/* The production adapter checks that the SDK getter exists during init, but
 * must never call it on the TIMER path. This host-fixture setup performs no I/O. */
__attribute__((constructor)) static void context_timer_runtime_init(void) {
    runtime.resident_shell=context_timer_resident_forbidden;
}
int __wrap_portable_app_idle_sleep(const risc_runtime_api_v1 *rt,
        const risc_display_output_api_v1 *display,const risc_battery_gauge_api_v1 *gauge,
        const alarm_service_v1 *alarm) {
    (void)rt;(void)display;(void)gauge;(void)alarm;
    assert(!"Sparse TIMER must not enter the foreground idle helper");
    return -2;
}
