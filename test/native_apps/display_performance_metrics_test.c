#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
static risc_display_present_metrics_v1 supplied;
static unsigned gets,events;
static uint32_t values[32];
static bool accept=true;
static bool metrics_get(void *context,risc_display_present_metrics_v1 *out) {
 (void)context;assert(out->api_version==1 && out->struct_size==sizeof(*out));++gets;
 if(!accept)return false;
 *out=supplied;return true;
}
static uint32_t metrics_trace(uint32_t id,uint32_t phase,uint32_t value) {
 assert(id==7&&phase==RISC_PERF_COUNTER);++events;values[value>>24]=value&0xffffffu;return 7;
}
static bool seed(void *context,risc_display_frame_v1 frame){(void)context;(void)frame;assert(false);return false;}
static int32_t power(void *context,uint32_t ms){(void)context;(void)ms;assert(false);return -1;}
int main(void) {
 risc_display_output_api_v1_metrics provider={0};
 provider.power.history.base=fx_display;provider.power.history.base.struct_size=sizeof(provider);
 provider.power.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;provider.power.history.extension_version=1;provider.power.history.seed_previous=seed;
 provider.power.power_tag=RISC_DISPLAY_POWER_TAG;provider.power.power_version=1;provider.power.prepare=power;provider.power.resume=power;
 provider.metrics_tag=RISC_DISPLAY_METRICS_TAG;provider.metrics_version=1;provider.snapshot=metrics_get;
 fx_runtime.trace=metrics_trace;perf_runtime=&fx_runtime;perf_id=perf_frame_id=7;
 display=&provider.power.history.base;
 supplied=(risc_display_present_metrics_v1){.api_version=1,.struct_size=sizeof(supplied),.token=9,.state=RISC_DISPLAY_PRESENT_COMPLETE,
  .mode=RISC_DISPLAY_METRICS_PARTIAL,.valid_times=63,.bytes_sent=12800,.gpio_write_calls=102400,
  .queued_ms=100,.transfer_start_ms=104,.transfer_end_ms=120,.refresh_ms=121,.busy_assert_ms=123,.busy_done_ms=300,
  .effective_update={0,0,800,80}};
 perf_metrics(9);assert(gets==1&&events==10);
 assert(values[16]==12800&&values[17]==102400&&values[18]==16&&values[19]==200&&values[20]==64000&&values[21]==1);
 assert(values[22]==177&&values[23]==2&&values[24]==4&&values[25]==63);
 supplied.valid_times=0;events=0;perf_metrics(9);assert(events==5); /* Unknown intervals omitted, never invented. */
 events=0;perf_metrics(8);assert(!events); /* Different token cannot supply frame timing. */
 accept=false;perf_metrics(9);assert(!events);accept=true;
 unsigned before=gets;perf_frame_id=6;perf_metrics(9);assert(gets==before&&!events);perf_frame_id=7;
 provider.metrics_tag=0;perf_metrics(9);assert(gets==before);provider.metrics_tag=RISC_DISPLAY_METRICS_TAG;
 display=&fx_display;perf_metrics(9);assert(gets==before); /* Exact old prefix. */
 assert(!calls&&!acquires&&!releases&&!presents&&!ticks); /* Getter path has no provider authority or clock I/O. */
 puts("Display metrics: per-token values, valid timestamps, old suffix, mismatch and stale completion pass");return 0;
}
