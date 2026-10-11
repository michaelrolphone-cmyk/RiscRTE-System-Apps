#define TEST_PAPER_SETTINGS
#define PORTABLE_DISPLAY_ROTATION 90
#define PORTABLE_SETTINGS_X4_DESK_CLOCK
/* Enter native-retained sleep from each combined Settings editor. */
#define PORTABLE_ALARM_SETTINGS
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_ALARM_FIXTURE_MAIN original_alarm_fixture_main
#include "portable_alarm_test.c"
static unsigned editor_page,late_kv,late_rtc,late_nav,late_service;
static bool jumped;
static int32_t guarded_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*size) {
  if(native_sleep_calls)++late_kv;
  if(!strcmp(k,PORTABLE_DESK_FACE_KEY)){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
  return kv_get(c,k,b,cap,size);
}
static int32_t guarded_put(void*c,const char*k,const void*b,uint32_t size) {
  if(native_sleep_calls)++late_kv;
  return kv_put(c,k,b,size);
}
static bool guarded_rtc(void*c,twatch_rtc_time_v1*out) {
  if(native_sleep_calls)++late_rtc;
  return rtc_read(c,out);
}
static bool guarded_nav(void*c) {
  if(native_sleep_calls)++late_nav;
  return nav_reset(c);
}
static int32_t guarded_step(void*c) {
  if(native_sleep_calls)++late_service;
  return service_step(c);
}
static void idle_editor(uint32_t ms) {
  test_yield(ms);
  if(!jumped && sv_page==editor_page){ticks+=60000;jumped=true;}
}
int main(int argc,char **argv) {
  assert(argc==2);const unsigned pages[]={SV_DESK_FACE,SV_SLEEP,SV_FLIP,SV_LANGUAGE};assert(atoi(argv[1])<4);editor_page=pages[atoi(argv[1])];scenario=100;
  risc_runtime_api_v1 r=runtime_api;r.acquire=acquire_alarm;r.yield_ms=idle_editor;
  rt=&r;dg.struct_size=sizeof(dg);bg.struct_size=sizeof(bg);
  assert(acquire_alarm("display.output",1,0,&dg));display=dg.api;assert(display_info(NULL,&info));surface_format=RISC_DISPLAY_FORMAT_MONO1;paper_rotated=true;
  assert(portable_touch_open(&touch,&r));failed=false;display_settled=true;
  alarm_pixels=malloc(sizeof(framebuffer));assert(alarm_pixels);assert(portable_alarm_open(&alarms,&r));
  alarm_fake.state=ALARM_STATE_READY;
  assert(settings_open());
  risc_key_value_v1 kv=kv_api;kv.get=guarded_get;kv.put=guarded_put;settings_store=alert_store=&kv;
  risc_input_navigation_api_v1 nav=nav_api;nav.reset=guarded_nav;navigation=&nav;
  alarm_service_v1 service=service_api;service.step=guarded_step;alarms.api=&service;
  rtc_api.read=guarded_rtc;
  settings_render(0,0);
  uint8_t result=settings_activate(0,editor_page==SV_DESK_FACE?SETTINGS_FACE_ROW:editor_page==SV_SLEEP?4:editor_page==SV_FLIP?SETTINGS_FLIP_ROW:SETTINGS_LANGUAGE_ROW);
  assert(result==T5_APP_SETTING_NO_CHANGE && jumped && native_sleep_calls==1);
  assert(failed && portable_app_sleep_retained() && !stop_calls && !return_launches);
  unsigned live=grants,shown=displays,steps=service_steps;
  settings_view_redraw();sv_switch(SV_ROOT);app_module_fini();
  assert(live && grants==live && displays==shown && service_steps==steps);
  assert(!late_kv && !late_rtc && !late_nav && !late_service && !stop_calls);
  puts("Desk-clock Settings native-retained editor: no later provider I/O, cleanup or grant release passed");
}
