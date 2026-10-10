#define USB_TRANSFER_TEST_MAIN unused_usb_fixture_main
#include "usb_transfer_test.c"
#undef USB_TRANSFER_TEST_MAIN
extern const risc_usb_device_msc_api_v1*bench_provider(unsigned);
extern uint64_t bench_ms(void);extern void bench_yield(uint32_t),bench_cpu(void),bench_token(uint64_t),bench_after_poll(uint32_t),bench_report(void);extern bool bench_quiesce(void);
static const risc_usb_device_msc_api_v1 *actual;
static risc_usb_device_msc_api_v1_diagnostics checked;
static int32_t measured_begin(void*c,uint64_t*t){int32_t n=actual->begin(c,t);session=preparing=n==0;begins++;bench_token(*t);return n;}
static int32_t measured_poll(void*c,uint64_t t,risc_usb_device_msc_status_v1*out){int32_t n=actual->poll(c,t,out);bench_after_poll(out->state);preparing=out->state==RISC_USB_MSC_PREPARING;return n;}
static int32_t measured_end(void*c,uint64_t t,uint32_t reason){int32_t n=actual->end(c,t,reason);if(!n)session=false;return n;}
static bool measured_health(risc_runtime_health_v1*out){bench_cpu();ms=(unsigned)bench_ms();out->uptime_ms=ms;assert(ms<120000);return true;}
static void measured_yield(uint32_t n){bench_yield(n);ms=(unsigned)bench_ms();}
static bool measured_release(risc_runtime_capability_v1*g){if(g->api==selected_api){assert(!session&&bench_quiesce());usb_releases++;}return release(g);}
static bool measured_diagnostic(const char*line){assert(strlen(line)<255);return true;}
int main(int argc,char**argv){
 assert(argc==2);test_case=18;actual=bench_provider((unsigned)atoi(argv[1]));checked=*risc_usb_device_msc_diagnostics(actual);checked.base.base.begin=measured_begin;checked.base.base.poll=measured_poll;checked.base.base.end=measured_end;selected_api=&checked;
 usb_runtime.health=measured_health;usb_runtime.yield_ms=measured_yield;usb_runtime.release=measured_release;usb_runtime.diagnostic=measured_diagnostic;
 assert(!app_module_init());app_main();app_module_fini();assert(!frames&&!grants&&!subs&&!session&&usb_releases==1&&launches==1);bench_report();return 0;
}
