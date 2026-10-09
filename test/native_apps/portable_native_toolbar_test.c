/* Production adapter and paper/Quick presentation over deterministic providers.
 * The app hook supplies already-local civil time. Its reader is a test double;
 * these checks do not qualify native providers, a product app or hardware. */
#ifdef TEST_NATIVE_TOOLBAR_QUICK
#define PORTABLE_QUICK_ACTIONS
#define PORTABLE_ALARM_CLIENT
#endif
#define PORTABLE_DISPLAY_ROTATION 90
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PortableTime.h"
static unsigned forward_calls;
static bool __attribute__((unused)) fixture_time_forward(const twatch_rtc_time_v1 *in,twatch_rtc_time_v1 *out) {
  ++forward_calls;return portable_time_forward(in,out);
}
#define portable_time_forward fixture_time_forward
#include "../../lib/PortableApps/src/adapter.c"
#undef portable_time_forward

const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned calls,acquires,releases,live,subscriptions,frames,presents,free_calls;
static unsigned ticks,barriers,hook_calls,reader_opens,reader_closes,reader_live;
static unsigned rtc_acquires,rtc_reads,rtc_writes,battery_reads;
static bool retained,hook_available=true,hook_retain,hook_retain_result;
static unsigned callback_calls,callback_frees,callback_presents,callback_batteries;
static uint8_t pixels[800*480/8],retained_pixels[800*480/8];
static twatch_rtc_time_v1 local_time={2026,10,7,3,13,42,56};
#ifdef TEST_NATIVE_TOOLBAR_QUICK
static bool close_quick_on_yield,dnd_saved,dnd_value,retain_refresh,retain_alarm_status,retain_alarm_stop;
static unsigned refreshes,brightness_calls,kv_writes,alarm_stops;
#endif
void __real_free(void *pointer);
void __wrap_free(void *pointer){assert(!retained);++free_calls;__real_free(pointer);}
static void io(void){assert(!retained);++calls;}
static bool fx_health(risc_runtime_health_v1 *out){io();out->uptime_ms=ticks;return true;}
static void fx_yield(uint32_t ms) {
  io();ticks+=ms;
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  if(close_quick_on_yield&&hook_calls)pqa_close(&quick.ui);
#endif
}
static bool fx_diagnostic(const char *s){io();assert(s);return true;}
static bool fx_launch(const char *s){io();assert(s);return true;}
static void checkpoint(void) {
  callback_calls=calls;callback_frees=free_calls;callback_presents=presents;callback_batteries=battery_reads;
  memcpy(retained_pixels,pixels,sizeof(pixels));
}
static bool fx_retain(void){assert(!retained);checkpoint();retained=true;++barriers;return true;}
static bool fx_info(void *context,risc_display_info_v1 *out) {
  (void)context;io();*out=(risc_display_info_v1){.width=800,.height=480,
    .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),
    .flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE};return true;
}
static bool fx_frame(void *context,uint32_t format,risc_display_surface_v1 *out) {
  (void)context;io();assert(!frames&&format==RISC_DISPLAY_FORMAT_MONO1);frames=1;
  *out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=800,.height=480,
    .stride_bytes=100,.size_bytes=sizeof(pixels),.pixel_format=format};return true;
}
static void fx_frame_release(void *context,risc_display_frame_v1 frame) {
  (void)context;io();assert(frames&&frame==1);frames=0;
}
static bool fx_submit(void *context,risc_display_frame_v1 frame,const risc_display_rect_v1 *rect,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
  (void)context;(void)rect;(void)n;(void)options;io();assert(frames&&frame==1);
  frames=0;*token=++presents;return true;
}
static bool fx_present(void *context,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
  (void)context;io();assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
#ifdef TEST_NATIVE_TOOLBAR_QUICK
static bool fx_brightness(void *context,uint16_t value,uint16_t maximum) {
  (void)context;io();assert(value<=100&&maximum==100);++brightness_calls;return true;
}
#endif
static const risc_display_output_api_v1 fx_display={.api_version=1,.struct_size=sizeof(fx_display),
  .get_info=fx_info,.acquire=fx_frame,.release=fx_frame_release,.submit=fx_submit,.present_status=fx_present,
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  .set_brightness=fx_brightness,
#endif
};
static uint64_t fx_subscribe(void *context){(void)context;io();assert(!subscriptions);subscriptions=1;return 1;}
static bool fx_unsubscribe(void *context,uint64_t token){(void)context;io();assert(token==1&&subscriptions);subscriptions=0;return true;}
static bool fx_touch_poll(void *context,size_t n){(void)context;io();assert(n==1);return true;}
static int32_t fx_touch_next(void *context,uint64_t token,risc_touch_event_v1 *out){(void)context;(void)out;io();assert(token==1);return 0;}
static bool fx_snapshot(void *context,risc_touch_snapshot_v1 *out){(void)context;io();*out=(risc_touch_snapshot_v1){.width=480,.height=800};return true;}
static const risc_touch_api_v1 fx_touch={1,sizeof(fx_touch),NULL,fx_subscribe,fx_unsubscribe,fx_touch_poll,fx_touch_next,fx_snapshot};
static bool fx_battery(void *context,risc_battery_sample_v1 *out){(void)context;io();++battery_reads;*out=(risc_battery_sample_v1){3900,70,0};return true;}
static const risc_battery_gauge_api_v1 fx_gauge={1,sizeof(fx_gauge),NULL,fx_battery};
static bool fx_rtc_read(void *context,twatch_rtc_time_v1 *out){(void)context;io();++rtc_reads;*out=local_time;return true;}
static bool fx_rtc_write(void *context,const twatch_rtc_time_v1 *in){(void)context;(void)in;io();++rtc_writes;return true;}
static const twatch_rtc_api_v1 fx_rtc={.api_version=2,.struct_size=sizeof(fx_rtc),.read=fx_rtc_read,.write=fx_rtc_write};
#ifdef TEST_NATIVE_TOOLBAR_QUICK
static int32_t fx_kv_get(void *context,const char *key,void *data,uint32_t capacity,uint32_t *size) {
  (void)context;io();*size=0;
  if(!strcmp(key,PQA_DND_KEY)&&dnd_saved){assert(capacity>=1);*(uint8_t *)data=dnd_value;*size=1;return RISC_KEY_VALUE_OK;}
  return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t fx_kv_put(void *context,const char *key,const void *data,uint32_t size) {
  (void)context;io();assert(!strcmp(key,PQA_DND_KEY)&&size==1);++kv_writes;
  dnd_saved=true;dnd_value=*(const uint8_t *)data!=0;return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 fx_kv={1,sizeof(fx_kv),NULL,fx_kv_get,fx_kv_put};
static int32_t fx_alarm_status(void *context,alarm_status_v1 *out){(void)context;io();*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return retain_alarm_status?ALARM_OUTPUT:ALARM_OK;}
static int32_t fx_alarm_step(void *context){(void)context;io();return ALARM_OK;}
static int32_t fx_alarm_stop(void *context){(void)context;io();++alarm_stops;return retain_alarm_stop?ALARM_OUTPUT:ALARM_OK;}
static int32_t fx_alarm_refresh(void *context){(void)context;io();++refreshes;return retain_refresh?ALARM_OUTPUT:ALARM_OK;}
static int32_t fx_alarm_ack(void *context,const alarm_token_v1 *token){(void)context;(void)token;io();return ALARM_OK;}
static int32_t fx_alarm_prepare(void *context,alarm_sleep_v1 *sleep){(void)context;(void)sleep;io();return ALARM_OK;}
static const alarm_service_v1 fx_alarm={1,sizeof(fx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_refresh,fx_alarm_ack,fx_alarm_prepare,fx_alarm_stop};
#endif
static bool fx_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
  (void)instance;io();const void *api=NULL;assert(out->struct_size==sizeof(*out));
  if(!strcmp(name,"rtc.clock")){assert(version==2);++rtc_acquires;api=&fx_rtc;}
  else {
    assert(version==1);
    if(!strcmp(name,"display.output"))api=&fx_display;
    else if(!strcmp(name,"input.touch.raw"))api=&fx_touch;
    else if(!strcmp(name,"board.battery"))api=&fx_gauge;
#ifdef TEST_NATIVE_TOOLBAR_QUICK
    else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY))api=&fx_kv;
    else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))api=&fx_alarm;
#endif
    else assert(!"unexpected provider acquisition");
  }
  ++acquires;++live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=api};return true;
}
static bool fx_release(risc_runtime_capability_v1 *grant) {
  io();assert(grant->api&&grant->slot&&live);++releases;--live;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
static risc_runtime_api_v1 fx_runtime={.api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,
  .yield_ms=fx_yield,.diagnostic=fx_diagnostic,.request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,.retain_invocation=fx_retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&fx_runtime:NULL;}
#ifndef TEST_NATIVE_TOOLBAR_MISSING_HOOK
bool portable_app_native_local_time(twatch_rtc_time_v1 *out) {
  assert(!retained&&out&&!reader_live);++hook_calls;++reader_opens;++reader_live;
  *out=local_time;
  /* An ordinary per-sample reader is closed before its copied value escapes.
   * A retained reader models loss of custody and pins its unresolved sample. */
  if(hook_retain)portable_adapter_retain();
  else {--reader_live;++reader_closes;}
  if(!retained)checkpoint();
  return hook_retain?hook_retain_result:hook_available;
}
#endif
static void no_legacy(void){assert(!rtc_acquires&&!rtc_reads&&!rtc_writes&&!forward_calls);}
static void check_clock(const paper_presentation *view,bool expected) {
  uint8_t hour=0xa5,minute=0x5a;unsigned before=hook_calls;
  assert(view->clock(&hour,&minute)==expected);assert(hook_calls==before+1);
  if(expected)assert(hour==local_time.hour&&minute==local_time.minute);
  else assert(hour==0xa5&&minute==0x5a);
  no_legacy();
}
static void check_retained(const paper_presentation *view) {
  assert(retained&&barriers==1&&portable_adapter_retained());
  assert(calls==callback_calls&&free_calls==callback_frees&&presents==callback_presents&&battery_reads==callback_batteries);
  unsigned samples=hook_calls;
  uint8_t hour=0xa5,minute=0x5a;
  assert(!view->clock(&hour,&minute)&&hour==0xa5&&minute==0x5a&&hook_calls==samples);
  view->begin();view->text(20,20,300,"STOPPED",1,false,true);view->circle(30,30,10,true);
  fill(0,0,100,100,0);np_pixel(1,1,0,255);present(false);
  t5_app_input_t input={0};assert(!poll(&input,0));input_service();
  t5_battery_state_t battery={0};assert(!read_battery(&battery));
  app_module_fini();app_module_fini();
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  char value[6];quick_time(value);assert(!strcmp(value,"--:--"));
  quick.ui.action_dnd=true;assert(!quick_apply(PQA_DND));
  bool consumed=false;assert(!quick_interrupt()&&!quick_foreground(&consumed));
#endif
  assert(calls==callback_calls&&free_calls==callback_frees&&presents==callback_presents&&battery_reads==callback_batteries);
  assert(!memcmp(retained_pixels,pixels,sizeof(pixels))&&hook_calls==samples&&barriers==1);
  no_legacy();
}
static void malformed(unsigned which) {
  switch(which) {
    case 0:local_time.year=0;break;
    case 1:local_time.month=0;break;
    case 2:local_time.month=13;break;
    case 3:local_time.day=0;break;
    case 4:local_time.day=32;break;
    case 5:local_time=(twatch_rtc_time_v1){2026,2,29,0,13,42,56};break;
    case 6:local_time=(twatch_rtc_time_v1){2028,2,30,3,13,42,56};break;
    case 7:local_time=(twatch_rtc_time_v1){2026,4,31,5,13,42,56};break;
    case 8:local_time.hour=24;break;
    case 9:local_time.minute=60;break;
    case 10:local_time.second=60;break;
    case 11:local_time.weekday=7;break;
    case 12:local_time.weekday=2;break;
    case 13:local_time.year=1599;break;
    case 14:local_time.year=10000;break;
    case 15:local_time=(twatch_rtc_time_v1){1900,2,29,4,13,42,56};break;
    case 16:local_time=(twatch_rtc_time_v1){2100,2,29,1,13,42,56};break;
    default:assert(0);
  }
}
int main(int argc,char **argv) {
  assert(argc>=2);const char *name=argv[1];
  if(!strcmp(name,"runtime-short"))fx_runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
  if(!strcmp(name,"runtime-no-retain"))fx_runtime.retain_invocation=NULL;
  if(!strncmp(name,"runtime-",8)) {
    assert(app_module_init()!=0);assert(!calls&&!live&&!barriers);return 0;
  }
  assert(app_module_init()==0&&!portable_adapter_retained());
  const paper_presentation *view=paper_presentation_get();assert(view&&view->clock);
  assert(!hook_calls);no_legacy();
  if(!strcmp(name,"valid")||!strcmp(name,"unavailable")||!strcmp(name,"malformed")) {
    hook_available=strcmp(name,"unavailable")!=0;
    if(!strcmp(name,"malformed")){assert(argc==3);malformed((unsigned)atoi(argv[2]));}
    bool expected=!strcmp(name,"valid");
    for(unsigned i=0;i<8;++i) {
      unsigned before=acquires;assert(paper_presentation_get()==view&&acquires==before);
      check_clock(view,expected);assert(!reader_live&&reader_opens==reader_closes);
#ifdef TEST_NATIVE_TOOLBAR_QUICK
      char value[6];quick.hour_24=true;quick_time(value);
      assert(!strcmp(value,expected?"13:42":"--:--"));
      quick.hour_24=false;quick_time(value);assert(!strcmp(value,expected?"01:42":"--:--"));
#endif
    }
  } else if(!strcmp(name,"recover-samples")) {
    for(unsigned i=0;i<4;++i) {
      local_time=(twatch_rtc_time_v1){2026,10,7,3,13,42,56};hook_available=i!=1;
      if(i==2)local_time.weekday=4;
      check_clock(view,i==0||i==3);
#ifdef TEST_NATIVE_TOOLBAR_QUICK
      char value[6];quick.hour_24=true;quick_time(value);
      assert(!strcmp(value,i==0||i==3?"13:42":"--:--"));
#endif
    }
  } else if(!strcmp(name,"gregorian")) {
    static const twatch_rtc_time_v1 valid[]={
      {1600,2,29,2,0,0,0},{1969,12,31,3,17,0,0},{2000,1,1,6,0,0,0},
      {2000,2,29,2,0,0,0},{2028,2,29,2,12,0,1},{2026,12,31,4,23,59,59},
      {2400,2,29,2,0,0,0},{9999,12,31,5,23,59,59}};
    for(unsigned i=0;i<sizeof(valid)/sizeof(valid[0]);++i){local_time=valid[i];check_clock(view,true);}
#ifdef TEST_NATIVE_TOOLBAR_QUICK
    char value[6];local_time=(twatch_rtc_time_v1){2026,10,7,3,0,0,0};quick.hour_24=false;quick_time(value);assert(!strcmp(value,"12:00"));
    local_time.hour=12;quick_time(value);assert(!strcmp(value,"12:00"));
#endif
  } else if(!strcmp(name,"retained-false")||!strcmp(name,"retained-true")) {
    view->begin();assert(frames);hook_retain=true;hook_retain_result=!strcmp(name,"retained-true");
    check_clock(view,false);assert(reader_live==1&&frames==1&&!surface.frame);check_retained(view);
  }
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  else if(!strcmp(name,"quick-retained-false")||!strcmp(name,"quick-retained-true")) {
    hook_retain=true;hook_retain_result=!strcmp(name,"quick-retained-true");
    char value[6];quick_time(value);assert(!strcmp(value,"--:--"));
    assert(hook_calls==1&&reader_live==1);check_retained(view);
  }
  else if(!strcmp(name,"quick-modal")||!strcmp(name,"quick-modal-retained")) {
    view->begin();view->text(20,20,300,"FOREGROUND",1,false,true);present(false);assert(alarm_pixels_valid);
    quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.neutral_gate=false;quick.ui.gesture=PQA_IDLE;
    close_quick_on_yield=true;hook_retain=!strcmp(name,"quick-modal-retained");hook_retain_result=true;
    bool consumed=false;unsigned samples=hook_calls;
    assert(quick_foreground(&consumed)==!hook_retain);assert(hook_calls==samples+1);
    if(hook_retain){assert(quick_background&&quick_modal);check_retained(view);}
    else {assert(consumed&&!quick_modal&&!quick_background&&battery_reads);}
  } else if(!strcmp(name,"quick-actions")||!strcmp(name,"quick-action-retained")) {
    char value[6];quick_time(value);assert(!strcmp(value,"01:42"));
    quick.ui.action_dnd=true;retain_refresh=!strcmp(name,"quick-action-retained");
    assert(quick_apply(PQA_DND)==!retain_refresh);assert(kv_writes==1&&refreshes==1&&dnd_value);
    if(retain_refresh)check_retained(view);
    else {quick.ui.action_dnd=false;assert(quick_apply(PQA_DND));assert(kv_writes==2&&refreshes==2&&!dnd_value);}
  } else if(!strcmp(name,"fini-status-retained")||!strcmp(name,"fini-stop-retained")) {
    view->begin();assert(frames==1);
    retain_alarm_status=!strcmp(name,"fini-status-retained");retain_alarm_stop=!retain_alarm_status;
    failed=true;app_module_fini();assert(retained&&alarm_stops==(retain_alarm_stop?1u:0u));
    assert(frames==1&&!surface.frame);check_retained(view);
  }
#endif
  else assert(!"unknown scenario");
  no_legacy();
  if(!retained){assert(reader_opens==reader_closes&&!reader_live);app_module_fini();assert(!live&&!frames&&!subscriptions);}
  printf("{\"case\":\"%s\",\"hook_calls\":%u,\"reader_opens\":%u,\"reader_closes\":%u,\"retained\":%s,\"legacy_rtc_acquires\":%u,\"legacy_rtc_reads\":%u,\"legacy_time_conversions\":%u}\n",
    name,hook_calls,reader_opens,reader_closes,retained?"true":"false",rtc_acquires,rtc_reads,forward_calls);
  return 0;
}
