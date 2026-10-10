/* Actual app + adapter bridged to the production async provider supplied by
 * the Runtime concurrency fixture. Only outer display/KV/time are synthetic. */
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
void app_workflow_start(const wifi_async_v1 *api) {
    assert(api&&api->base.struct_size>=sizeof(*api));
    workflow_start(false);assert(portable_wifi_suspend());
    radios=*api;use_async=true;assert(wifi_acquire());draft();
    assert(wifi_async&&wifi_async->begin==api->begin);
}
void app_workflow_scan(void) {
    wifi_scan_start();assert(wifi_operation&&scanning&&page==WP_SCAN);
    wifi_make_view();portable_wifi_render(&view);assert(!surface.frame);
}
void app_workflow_unavailable(void) {
    assert(wifi_async&&!wifi_operation);
    wifi_scan_start();
    assert(!wifi_operation&&!wifi_next_request.kind&&!scanning&&!joining);
    assert(wifi_async&&!cleanup_pending&&!portable_text_adapter_retained());
    wifi_connect();
    assert(!wifi_operation&&!wifi_next_request.kind&&!scanning&&!joining);
    assert(wifi_async&&!cleanup_pending&&!portable_text_adapter_retained());
    assert(wifi_back()&&!opened&&!wg.api);
    finish();assert(!grant_count&&!sub_count&&!frame_count);
}
void app_workflow_blocked(void) {
    unsigned grants=grant_count;assert(wifi_operation&&wg.api);
    assert(!wifi_back()&&wifi_scan_back&&portable_wifi_stop_pending());
    for(unsigned i=0;i<100;i++){
        ticks+=5;t5_app_input_t input={0};assert(wa->poll(&input,1));
        assert(portable_wifi_services_safe());
        assert(!portable_wifi_services_begin()&&!wifi_service_depth);
        assert(opened&&wg.api&&grant_count==grants&&!cleanup_pending&&!portable_text_adapter_retained());
    }
    assert(!portable_wifi_suspend()&&portable_wifi_stop_pending()&&wg.api);
}
void app_workflow_cleaned(void) {
    tick(250);assert(!wifi_operation&&!wifi_scan_back&&page==WP_ROOT&&!cleanup_pending);
    assert(wifi_back()&&!opened&&!wg.api&&!wifi_service_depth&&!wifi_service_lease);
    finish();assert(!grant_count&&!sub_count&&!frame_count);
}
