/* Reuse deterministic paper/Quick devices, but link the actual source owner
 * and production realtime/timezone helpers as separate translation units. */
#define TEST_NATIVE_TOOLBAR_MISSING_HOOK
#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
#include "PortableRealtimeClient.h"
#include "PortableTimeZonePreference.h"

static const char *scenario;
static const char *zone="America/Denver";
static unsigned zone_live,zone_opens,zone_closes,zone_reads,native_reads;
static bool pending_fence;
static risc_realtime_snapshot_v1 sample={.struct_size=sizeof(sample),.validity=RISC_REALTIME_VALID,
  .epoch_seconds=1791399600,.monotonic_before_us=100,.monotonic_after_us=102};
static bool is(const char *name){return !strcmp(scenario,name);}
static void source_io(void){assert(!pending_fence);io();}
static void encode_zone(void *out,uint32_t capacity,uint32_t *used) {
  assert(capacity==PORTABLE_TIMEZONE_RECORD_BYTES);
  uint8_t *bytes=out;memset(bytes,0,capacity);
  bytes[0]='T';bytes[1]='Z';bytes[2]=1;strcpy((char *)bytes+4,zone);
  bytes[3]=0xa5;for(unsigned i=0;i<capacity;++i)if(i!=3)bytes[3]^=bytes[i];
  *used=capacity;
}
static int32_t source_get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *used) {
  assert(context==&zone_live&&zone_live==1&&!reader_live);
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  if(strcmp(key,PORTABLE_TIMEZONE_KEY)) {
    assert(is("quick-modal")||is("quick-modal-retained"));
    return fx_kv_get(context,key,out,capacity,used);
  }
#else
  assert(!strcmp(key,PORTABLE_TIMEZONE_KEY));
#endif
  source_io();++zone_reads;*used=0;
  if(is("kv-retained")){portable_adapter_retain();return RISC_KEY_VALUE_OK;}
  if(is("kv-context")||is("kv-unknown")){pending_fence=true;return is("kv-context")?RISC_KEY_VALUE_CONTEXT:42;}
  if(is("kv-missing"))return RISC_KEY_VALUE_NOT_FOUND;
  if(is("kv-io"))return RISC_KEY_VALUE_IO;
  if(is("kv-invalid"))return RISC_KEY_VALUE_INVALID;
  if(is("kv-small")){*used=100;return RISC_KEY_VALUE_BUFFER_SMALL;}
  encode_zone(out,capacity,used);
  if(is("kv-corrupt"))((uint8_t *)out)[3]^=1;
  if(is("kv-size"))*used=1;
  return RISC_KEY_VALUE_OK;
}
static int32_t forbidden_put(void *context,const char *key,const void *out,uint32_t size) {
  (void)context;(void)key;(void)out;(void)size;assert(!"time source wrote storage");return RISC_KEY_VALUE_CONTEXT;
}
static risc_key_value_v1 source_kv={1,sizeof(source_kv),&zone_live,source_get,forbidden_put};
static int32_t source_read(void *context,risc_realtime_snapshot_v1 *out) {
  assert(context==&reader_live&&reader_live==1&&!zone_live&&out->struct_size==sizeof(*out));
  source_io();++native_reads;
  if(is("native-retained")||is("quick-modal-retained")){portable_adapter_retain();return RISC_REALTIME_OK;}
  if(is("native-context")||is("native-unknown")){pending_fence=true;return is("native-context")?RISC_REALTIME_CONTEXT:42;}
  if(is("native-io"))return RISC_REALTIME_IO;
  if(is("native-invalid"))return RISC_REALTIME_INVALID;
  *out=sample;
  if(is("native-unset")){out->validity=RISC_REALTIME_UNSET;out->epoch_seconds=0;}
  if(is("native-invalid-validity"))out->validity=2;
  if(is("native-size"))--out->struct_size;
  if(is("native-reserved"))out->reserved=1;
  if(is("native-bracket"))out->monotonic_after_us=0;
  if(is("native-nanos"))out->nanoseconds=1000000000;
  if(is("native-resolution"))out->nanoseconds=1;
  if(is("native-negative"))out->epoch_seconds=-1;
  if(is("native-range"))out->epoch_seconds=INT64_C(2147483648);
  return RISC_REALTIME_OK;
}
static risc_realtime_api_v1 source_native={1,sizeof(source_native),&reader_live,source_read};
static bool source_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
  bool kv=!strcmp(name,RISC_KEY_VALUE_CAPABILITY),native=!strcmp(name,RISC_REALTIME_CAPABILITY);
  assert(strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)&&strcmp(name,TWATCH_RTC_CAPABILITY));
  if(!kv&&!native)return fx_acquire(name,version,instance,out);
  source_io();assert(version==1&&out->struct_size==sizeof(*out)&&!zone_live&&!reader_live);
  assert(instance==(kv?1u:0u));
  if(kv&&is("kv-acquire-empty")){pending_fence=true;return false;}
  if(native&&is("native-missing"))return false;
  ++acquires;++live;
  if(kv){++zone_live;++zone_opens;}else{++reader_live;++reader_opens;}
  *out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,
    .api=kv?(const void *)&source_kv:(const void *)&source_native};
  if((kv&&is("kv-acquire-dirty"))||(native&&is("native-acquire-dirty"))){pending_fence=true;return false;}
  if((kv&&is("kv-grant"))||(native&&is("native-grant"))){out->generation=0;pending_fence=true;}
  if((kv&&is("kv-acquire-retained"))||(native&&is("native-acquire-retained")))portable_adapter_retain();
  return true;
}
static bool source_release(risc_runtime_capability_v1 *grant) {
  bool kv=grant->api==&source_kv,native=grant->api==&source_native;
  if(!kv&&!native)return fx_release(grant);
  source_io();assert(live&&grant->generation&&grant->slot);
  if((kv&&is("kv-release-false"))||(native&&is("native-release-false"))){pending_fence=true;return false;}
  if((kv&&is("kv-release-dirty"))||(native&&is("native-release-dirty"))){pending_fence=true;return true;}
  if((kv&&is("kv-release-retained"))||(native&&is("native-release-retained"))){portable_adapter_retain();return true;}
  if(kv){assert(zone_live&&!reader_live);--zone_live;++zone_closes;}
  else{assert(reader_live&&!zone_live);--reader_live;++reader_closes;}
  ++releases;--live;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
  if((kv&&is("kv-release-size"))||(native&&is("native-release-size"))){--grant->struct_size;pending_fence=true;}
  return true;
}
static void expect_time(const twatch_rtc_time_v1 *want) {
  twatch_rtc_time_v1 actual={0};assert(portable_app_native_local_time(&actual));
  assert(actual.year==want->year&&actual.month==want->month&&actual.day==want->day&&
    actual.weekday==want->weekday&&actual.hour==want->hour&&actual.minute==want->minute&&actual.second==want->second);
  assert(!reader_live&&!zone_live&&reader_opens==reader_closes&&zone_opens==zone_closes);
}
static void unavailable(const paper_presentation *view) {
  twatch_rtc_time_v1 actual;memset(&actual,0xa5,sizeof(actual));
  twatch_rtc_time_v1 before=actual;assert(!portable_app_native_local_time(&actual));
  assert(!memcmp(&actual,&before,sizeof(actual)));
  if(retained)check_retained(view);
  uint8_t hour=0xa5,minute=0x5a;
  assert(!view->clock(&hour,&minute)&&hour==0xa5&&minute==0x5a);
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  char value[6];quick_time(value);assert(!strcmp(value,"--:--"));
#endif
}
#ifdef TEST_NATIVE_TOOLBAR_QUICK
static void source_yield(uint32_t delay) {
  fx_yield(delay);
  if(close_quick_on_yield&&native_reads)pqa_close(&quick.ui);
}
#endif
#ifndef TEST_NATIVE_SOURCE_NO_MAIN
int main(int argc,char **argv) {
  assert(argc==2);scenario=argv[1];
  assert(app_module_init()==0&&!portable_adapter_retained());
  const paper_presentation *view=paper_presentation_get();assert(view&&view->clock);
  fx_runtime.acquire=source_acquire;fx_runtime.release=source_release;
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  fx_runtime.yield_ms=source_yield;
#endif
  const twatch_rtc_time_v1 denver={2026,10,7,3,13,0,0};
  if(is("kv-table-short"))source_kv.struct_size=8;
  if(is("kv-table-version"))source_kv.api_version=2;
  if(is("kv-table-get"))source_kv.get=NULL;
  if(is("native-table-short"))source_native.struct_size=8;
  if(is("native-table-version"))source_native.api_version=2;
  if(is("native-table-read"))source_native.read=NULL;
  if(is("native-table-context"))source_native.context=NULL;
  if(is("valid")) {
    assert(!portable_app_native_local_time(NULL)&&!zone_reads&&!native_reads);
    for(unsigned i=0;i<4;++i) {
      expect_time(&denver);uint8_t hour=0,minute=1;assert(view->clock(&hour,&minute)&&hour==13&&!minute);
#ifdef TEST_NATIVE_TOOLBAR_QUICK
      char value[6];quick.hour_24=true;quick_time(value);assert(!strcmp(value,"13:00"));
      quick.hour_24=false;quick_time(value);assert(!strcmp(value,"01:00"));
#endif
    }
    assert(zone_reads==native_reads&&reader_opens==native_reads);
  } else if(is("zones-boundaries")) {
    expect_time(&denver);
    zone="Asia/Kathmandu";expect_time(&(twatch_rtc_time_v1){2026,10,8,4,0,45,0});
    zone="UTC";sample.epoch_seconds=0;expect_time(&(twatch_rtc_time_v1){1970,1,1,4,0,0,0});
    zone="America/Denver";expect_time(&(twatch_rtc_time_v1){1969,12,31,3,17,0,0});
    sample.epoch_seconds=INT64_C(2147483647);zone="Pacific/Auckland";
    expect_time(&(twatch_rtc_time_v1){2038,1,19,2,16,14,7});
    zone="America/Denver";sample.epoch_seconds=1772960340;
    expect_time(&(twatch_rtc_time_v1){2026,3,8,0,1,59,0});
    sample.epoch_seconds+=60;expect_time(&(twatch_rtc_time_v1){2026,3,8,0,3,0,0});
    sample.epoch_seconds=1793519940;expect_time(&(twatch_rtc_time_v1){2026,11,1,0,1,59,0});
    sample.epoch_seconds+=60;expect_time(&(twatch_rtc_time_v1){2026,11,1,0,1,0,0});
  }
#ifdef TEST_NATIVE_TOOLBAR_QUICK
  else if(is("quick-modal")||is("quick-modal-retained")) {
    view->begin();view->text(20,20,300,"FOREGROUND",1,false,true);present(false);assert(alarm_pixels_valid);
    quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.neutral_gate=false;quick.ui.gesture=PQA_IDLE;
    close_quick_on_yield=true;
    bool consumed=false;assert(quick_foreground(&consumed)==!is("quick-modal-retained"));
    assert(native_reads==1);
    if(is("quick-modal-retained")){assert(quick_background&&quick_modal);check_retained(view);}
    else assert(consumed&&!quick_modal&&!quick_background&&battery_reads&&!reader_live&&!zone_live);
  }
#endif
  else if(is("kv-missing")) {
    expect_time(&(twatch_rtc_time_v1){2026,10,7,3,19,0,0});
    assert(!zone_live&&!reader_live&&!retained);
  }
  else if(is("already-retained")) {
    view->begin();assert(frames==1);
    portable_adapter_retain();unavailable(view);assert(!zone_reads&&!native_reads);
  } else {
    bool must_fence=strstr(scenario,"acquire")||strstr(scenario,"grant")||strstr(scenario,"release")||
      is("kv-context")||is("kv-unknown")||is("native-context")||is("native-unknown")||
      is("kv-retained")||is("native-retained");
    view->begin();assert(frames==1);
    unavailable(view);assert(retained==must_fence);
    if(!retained) {
      assert(!zone_live&&!reader_live);
      /* Ordinary absence, typed error and malformed data can recover next
       * sample. No old native value or selected timezone is cached. */
      scenario="valid";source_kv=(risc_key_value_v1){1,sizeof(source_kv),&zone_live,source_get,forbidden_put};
      source_native=(risc_realtime_api_v1){1,sizeof(source_native),&reader_live,source_read};
      expect_time(&denver);
    }
  }
  no_legacy();
  if(retained){assert(barriers==1);unavailable(view);}
  else {assert(!reader_live&&!zone_live);app_module_fini();assert(!live&&!frames&&!subscriptions);}
  printf("{\"case\":\"%s\",\"zone_reads\":%u,\"native_reads\":%u,\"retained\":%s}\n",
    argv[1],zone_reads,native_reads,retained?"true":"false");
  return 0;
}

#endif
