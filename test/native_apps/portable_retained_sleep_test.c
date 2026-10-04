#define PORTABLE_APP_SLEEP_LOCAL
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_INPUT_NAVIGATION_LOCAL
#define PORTABLE_RTC_UTC8_DENVER
#define PORTABLE_RETURN_APP "springboard.elf"
#define main original_settings_fixture_main
#include "portable_settings_test.c"
#undef main
static bool retention_returned,jumped;
static unsigned late_rtc,late_kv,late_nav,sleeps;
static bool guarded_rtc(void*c,twatch_rtc_time_v1*out){if(retention_returned)late_rtc++;return rtc_read(c,out);}
static int32_t guarded_kv(void*c,const char*k,void*b,uint32_t cap,uint32_t*size){if(retention_returned)late_kv++;return kv_get(c,k,b,cap,size);}
static bool guarded_nav(void*c){if(retention_returned)late_nav++;return nav_reset(c);}
static void idle_yield(uint32_t ms){test_yield(ms);if(!jumped&&sv_page==SV_SLEEP){ticks+=60000;jumped=true;}}
int portable_app_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){(void)r;(void)d;(void)b;assert(!subscriptions&&!surface.frame);sleeps++;retention_returned=true;return -1;}
int main(void){
 scenario=100;assert(app_module_init()==0);risc_runtime_api_v1 idle_runtime=runtime_api;idle_runtime.yield_ms=idle_yield;rt=&idle_runtime;
 risc_key_value_v1 kv=kv_api;kv.get=guarded_kv;sleep_store=&kv;
 risc_input_navigation_api_v1 nav=nav_api;nav.reset=guarded_nav;navigation=&nav;rtc_api.read=guarded_rtc;
 settings_render(0,0);uint8_t result=settings_activate(0,4);
 assert(result==T5_APP_SETTING_NO_CHANGE&&sleeps==1&&failed&&!return_launches);
 printf("Nested retained unwind: RTC reads=%u KV reads=%u navigation resets=%u after sleep returned -1\n",late_rtc,late_kv,late_nav);
 unsigned frames=displays;
 settings_view_redraw();sv_switch(SV_ROOT); /* Both independently fail closed. */
 assert(!late_rtc&&!late_kv&&!late_nav&&displays==frames);
 /* A real unsafe native sleep also vetoes app_module_fini through Runtime;
  * this focused adapter fixture intentionally does not manually finalize. */
}
