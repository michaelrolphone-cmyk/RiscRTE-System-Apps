#define main original_sparse_main
#include "sparse_clock_adapter_test.c"
#undef main
int portable_app_idle_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,
                            const risc_battery_gauge_api_v1 *b,const alarm_service_v1 *a) {
 io();assert(r==rt&&d==display&&b==gauge&&a==alarms.api);
 assert(desk_phase==DESK_FOREGROUND && !subscriptions && !surface.frame && !paper_token);
 sleep_calls++;return 0;
}
int main(int argc,char **argv) {
 assert(argc==2);if(atoi(argv[1])<900)return original_sparse_main(argc,argv);
 assert(app_module_init()==0 && portable_desk_adapter_start(2)==1);
 portable_desk_adapter_begin();assert(portable_desk_adapter_present(true));
 t5_app_input_t input={0};for(unsigned i=0;i<3;i++)assert(poll(&input,20));
 unsigned before_mode=mode_calls;now+=60001;assert(poll(&input,20));
 assert(sleep_calls==1 && !mode_calls && before_mode==0 && !input.buttons && !input.tapped);
 assert(desk_phase==DESK_FOREGROUND && subscriptions==1 && !native_sleep_retained);
 app_module_fini();assert(!live&&!subscriptions&&!frame_count);
 puts("Actual sparse adapter: automatic Light bypasses manual Deep preference; timer-only and held input regressions PASS");
 return 0;
}
