/* Production sparse Clock, adapter, X4 sleep client and native-UTC alarm service.
 * Runtime/broker, empty KV, calendar RTC and display are host boundaries. Every
 * process starts with no retained record and native time UNSET unless selected. */
#define main unused_clock_fixture_main
#define risc_runtime_get_api unused_clock_fixture_runtime
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "paper_clock_test.c"
#pragma GCC diagnostic pop
#undef main
#undef risc_runtime_get_api
#include "PortableAppSleep.h"
#include "PortableTimeZonePreference.h"
#include "PortableRtcBasis.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
#include "RiscInputNavigationV1.h"
#include "RiscRealtimeV1.h"
#include "RiscProviderPromotionV1.h"
#include "RiscRetainedWakeV1.h"
#include "RiscDisplayOutputPowerV1.h"
#include "RiscProviderV2.h"
#include "RiscBoundKeyValueV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscPlatformRealtimeV1.h"
#include "AlarmServiceV2.h"
#include <time.h>
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static const char *test;
static bool running,promoted,native_valid,forbid,service_started;
static unsigned native_live,rtc_live,seeds,rtc_reads,native_reads,bound_reads,alarm_native_reads;
static unsigned kv_writes,rtc_writes,ledger_writes,retentions,events,seed_event,first_alarm_event,rtc_release_event,native_release_event;
static uint8_t points_ledger_bytes[64];
static uint32_t points_ledger_size;
static unsigned kv_reads,acquires,releases,provider_starts;
static int native_context;
static int64_t epoch=INT64_C(1791331197),seed_epoch;
static uint32_t seeded_ms;
static const risc_driver_v2 *provider;
static const alarm_service_v1 *service;
static const void *rtc_table;
static uint8_t error_pixels[sizeof(pixels)];
static bool error_rendered;
static bool which(const char *name){return !strcmp(test,name);}
static void safe(void){assert(running&&!forbid);}
static bool fail_policy(void){return which("bad-zone")||which("bad-basis")||which("unreadable-zone")||which("unreadable-basis");}
static bool ambiguous(void){return which("fold")||which("gap")||which("missing-basis-fold")||which("missing-basis-gap");}
static bool retained_case(void){return which("rtc-read-error")||which("rtc-acquire-error")||which("rtc-release-error")||which("seed-context")||which("native-release-error")||which("alarm-native-context")||which("alarm-storage-context");}
static bool expect_seed(void){return !(fail_policy()||ambiguous()||which("invalid-rtc")||which("native-valid")||which("seed-error"));}
/* All unrelated keys are actually missing, including face/time/sleep/radios,
 * alarms, timers and occurrences. No fabricated defaults are persisted. */
static int32_t prefs(const char *key,void *out,uint32_t capacity,uint32_t *size){
 *size=0;
 if(!strcmp(key,PORTABLE_TIMEZONE_KEY)) {
  if(which("unreadable-zone"))return RISC_KEY_VALUE_IO;
  if(which("empty")||which("reset-empty")||which("missing-zone")||which("native-valid")||retained_case()||which("invalid-rtc")||which("seed-error"))return RISC_KEY_VALUE_NOT_FOUND;
  uint8_t bytes[44]={'T','Z',1,0};
  const char *zone=(which("missing-basis")||ambiguous())?"America/Denver":"UTC";
  strcpy((char*)bytes+4,zone);bytes[3]=0xa5;
  for(unsigned i=0;i<sizeof(bytes);i++)if(i!=3)bytes[3]^=bytes[i];
  if(which("bad-zone"))bytes[3]^=1;
  assert(capacity>=sizeof(bytes));memcpy(out,bytes,sizeof(bytes));*size=sizeof(bytes);return RISC_KEY_VALUE_OK;
 }
 if(!strcmp(key,PORTABLE_RTC_BASIS_KEY)) {
  if(which("unreadable-basis"))return RISC_KEY_VALUE_IO;
  if(which("empty")||which("reset-empty")||which("missing-basis")||which("missing-basis-fold")||which("missing-basis-gap")||which("native-valid")||retained_case()||which("invalid-rtc")||which("seed-error"))return RISC_KEY_VALUE_NOT_FOUND;
  uint8_t bytes[12]={0x52,0x54,1,ambiguous()?0:1};
  portable_rtc_basis_put_word(bytes+8,portable_rtc_basis_check(bytes));
  if(which("bad-basis"))bytes[0]=0;
  assert(capacity>=sizeof(bytes));memcpy(out,bytes,sizeof(bytes));*size=sizeof(bytes);return RISC_KEY_VALUE_OK;
 }
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t preferences(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size){(void)c;safe();assert(promoted);kv_reads++;return prefs(key,out,capacity,size);}
static int32_t no_put(void *c,const char *key,const void *data,uint32_t size){(void)c;(void)key;(void)data;(void)size;safe();kv_writes++;assert(!"Clock recovery must never repair/write preferences");return RISC_KEY_VALUE_IO;}
static void alarm_event(void){safe();assert(service_started&&!native_live&&!rtc_live);if(!first_alarm_event)first_alarm_event=++events;}
static int32_t bound_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *size){(void)c;alarm_event();bound_reads++;if(which("alarm-storage-context")){forbid=true;return RISC_BOUND_KEY_VALUE_CONTEXT;}
 if(!strcmp(key,"points_utc_occ")&&points_ledger_size){assert(capacity>=points_ledger_size);memcpy(out,points_ledger_bytes,points_ledger_size);*size=points_ledger_size;return RISC_BOUND_KEY_VALUE_OK;}
 return prefs(key,out,capacity,size);}
/* The provider's virtual factory schedule can compact expired occurrences.
 * This is distinct from Clock policy repair: only its bound ledger may change,
 * only after native recovery, and its production VERIFY_OCC reads exact bytes. */
static int32_t bound_put(void *c,const char *key,const void *data,uint32_t size){(void)c;alarm_event();assert(native_valid&&!strcmp(key,"points_utc_occ")&&size==sizeof(points_ledger_bytes));assert(!seeds||seed_event<first_alarm_event);memcpy(points_ledger_bytes,data,size);points_ledger_size=size;ledger_writes++;return RISC_BOUND_KEY_VALUE_OK;}
static uint64_t monotonic(void *c){(void)c;safe();return ms;}
static void snapshot_native(risc_realtime_snapshot_v1 *out){
 *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=native_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,
  .epoch_seconds=native_valid?epoch+(ms-seeded_ms)/1000u:0,.nanoseconds=native_valid?((ms-seeded_ms)%1000u)*1000000u:0,
  .monotonic_before_us=(uint64_t)ms*1000u,.monotonic_after_us=(uint64_t)ms*1000u};
}
static int32_t alarm_native(void *c,risc_realtime_snapshot_v1 *out){(void)c;alarm_event();alarm_native_reads++;if(which("alarm-native-context")){forbid=true;return RISC_REALTIME_CONTEXT;}snapshot_native(out);return RISC_REALTIME_OK;}
static const risc_bound_key_value_v1 bound={1,sizeof(bound),NULL,bound_get,bound_put};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,monotonic,NULL};
static const risc_platform_realtime_api_v1 platform_time={1,sizeof(platform_time),NULL,alarm_native};
static const risc_provider_dependency_v1 dependencies[]={{"storage.key-value.bound",1,&bound},{"platform.clock",1,&clock_api},{"platform.realtime",1,&platform_time}};
static bool health_now(risc_runtime_health_v1 *out){safe();assert(polls<80);out->uptime_ms=ms;return true;}
static void wait_ms(uint32_t n){safe();ms+=n;}
static bool report(const char *s){safe();return diagnostic(s);}
static bool launch(const char *s){safe();return launch_app(s);}
/* Every hardware-boundary callback checks the terminal fence, including the
 * reused display/touch fixture operations and finalization callbacks. */
static bool display_info(void *c,risc_display_info_v1 *out){safe();return get_info(c,out);}
static bool display_acquire(void *c,uint32_t format,risc_display_surface_v1 *out){safe();return acquire_frame(c,format,out);}
static void display_release(void *c,risc_display_frame_v1 frame){safe();release_frame(c,frame);}
static bool display_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){safe();return present_status(c,token,out);}
static uint64_t touch_subscribe(void *c){safe();return subscribe(c);}
static bool touch_unsubscribe(void *c,uint64_t id){safe();return unsubscribe(c,id);}
static bool touch_poll(void *c,size_t n){safe();return poll_touch(c,n);}
static int32_t touch_next(void *c,uint64_t id,risc_touch_event_v1 *out){safe();return next_touch(c,id,out);}
static bool touch_snapshot(void *c,risc_touch_snapshot_v1 *out){safe();return snapshot(c,out);}
static bool battery_now(void *c,risc_battery_sample_v1 *out){safe();return battery_read(c,out);}
static bool wait_frame(void *c,risc_display_present_token_v1 token,uint32_t timeout,risc_display_present_status_v1 *out){(void)c;safe();assert(token==presents&&timeout);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool submit_frame(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rects,size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 safe();alarm_status_v1 state={.struct_size=sizeof(state)};assert(service->status(service->context,&state)==ALARM_OK);
 if(state.state==ALARM_STATE_BLOCKED){memcpy(error_pixels,pixels,sizeof(pixels));error_rendered=true;}
 (void)c;assert(frame==1&&frames&&options&&token);assert(count<=8);
 assert(options->intent==RISC_DISPLAY_PRESENT_QUALITY||options->intent==RISC_DISPLAY_PRESENT_CLEAN);
 for(size_t i=0;i<count;i++){const risc_display_rect_v1 *r=&rects[i];assert(rects&&r->x>=0&&r->y>=0&&r->width&&r->height);assert((unsigned)r->x+r->width<=PANEL_WIDTH&&(unsigned)r->y+r->height<=PANEL_HEIGHT);assert(r->x%8==0&&r->width%8==0);}
 frames=0;*token=++presents;return true;
}
static int32_t read_record(void *c,uint32_t type,uint32_t schema,risc_retained_wake_record_v1 *out,uint32_t *cause){(void)c;(void)type;(void)schema;(void)out;safe();assert(!provider_starts&&!promoted&&!presents);*cause=which("reset-empty")?RISC_BOOT_RESET:RISC_BOOT_POWER_ON;return RISC_RETAINED_WAKE_ABSENT;}
static int32_t stage_record(void *c,const risc_retained_wake_record_v1 *value){(void)c;(void)value;assert(!"Cold foreground test must not stage deep sleep");return RISC_RETAINED_WAKE_INVALID;}
static int32_t clear_record(void *c){(void)c;safe();return RISC_RETAINED_WAKE_OK;}
static const risc_retained_wake_api_v1 wake={1,sizeof(wake),NULL,read_record,stage_record,clear_record};
static int32_t promote(void *c){(void)c;safe();assert(!promoted&&!service_started);provider=t5_driver_get(2);assert(provider&&provider->capability_api==2);service=provider->capability;
 assert(alarm_service_descriptor(service));assert(provider->start(dependencies,3));service_started=true;provider_starts++;assert(!bound_reads&&!alarm_native_reads&&!seeds);promoted=true;return RISC_PROVIDER_PROMOTION_OK;}
static const risc_provider_promotion_api_v1 promotion={1,sizeof(promotion),&native_context,promote};
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *out){(void)c;safe();*out=(risc_input_navigation_frame_v1){0};
 /* Leave a visible typed error with Back, then exit the restored Clock. */
 if(polls==4)out->pressed=out->released=RISC_NAV_BACK;
 if(polls==8)out->pressed=out->released=RISC_NAV_CONFIRM;
 return true;}
static bool nav_foreground(void *c,const risc_input_foreground_v1 *value,size_t n){(void)c;(void)value;(void)n;safe();return true;}
static bool nav_reset(void *c){(void)c;safe();return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,nav_poll,nav_foreground,nav_reset};
static int32_t native_read(void *c,risc_realtime_snapshot_v1 *out){safe();assert(c==&native_context&&native_live&&!rtc_live);native_reads++;snapshot_native(out);return RISC_REALTIME_OK;}
static int32_t native_seed(void *c,int64_t seconds,uint32_t nanos){safe();assert(c==&native_context&&native_live&&promoted&&!rtc_live&&!nanos&&!first_alarm_event);seeds++;seed_event=++events;assert(rtc_release_event&&rtc_release_event<seed_event);seed_epoch=seconds;
 if(which("seed-context")){forbid=true;return RISC_REALTIME_CONTEXT;}
 if(which("seed-error"))return RISC_REALTIME_IO;
 epoch=seconds;seeded_ms=ms;native_valid=true;return RISC_REALTIME_OK;}
static const risc_realtime_control_api_v1 native_api={1,sizeof(native_api),&native_context,native_read,native_seed};
static bool rtc_read_now(void *c,twatch_rtc_time_v1 *out){(void)c;safe();assert(promoted&&rtc_live);rtc_reads++;
 if(which("rtc-read-error")){forbid=true;return false;}
 if(which("fold")||which("missing-basis-fold")){*out=(twatch_rtc_time_v1){2026,11,1,0,1,30,0};return true;}
 if(which("gap")||which("missing-basis-gap")){*out=(twatch_rtc_time_v1){2026,3,8,0,2,30,0};return true;}
 time_t stamp=epoch;struct tm *value=gmtime(&stamp);assert(value);*out=(twatch_rtc_time_v1){(uint16_t)(value->tm_year+1900),(uint8_t)(value->tm_mon+1),(uint8_t)value->tm_mday,(uint8_t)value->tm_wday,(uint8_t)value->tm_hour,(uint8_t)value->tm_min,(uint8_t)value->tm_sec};
 if(which("invalid-rtc"))out->month=13;
 return true;
}
static bool rtc_write_now(void *c,const twatch_rtc_time_v1 *value){(void)c;(void)value;safe();rtc_writes++;assert(!"Cold recovery cannot write the calendar RTC");return false;}
static bool wifi_disconnect(void *c){(void)c;safe();return true;}
static wifi_link_t wifi_status(void *c){(void)c;safe();return WIFI_LINK_DOWN;}
static bool bt_set(void *c,bool enabled){(void)c;safe();assert(!enabled);return true;}
static bool bt_status(void *c,uint8_t *out){(void)c;safe();*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.status=wifi_status,.disconnect_checked=wifi_disconnect};
static const portable_bluetooth_control_v1 bt={.api_version=1,.struct_size=sizeof(bt),.set_enabled=bt_set,.status=bt_status};
static bool obtain(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant){safe();assert(grant->struct_size==sizeof(*grant));const void *api=NULL;acquires++;
 if(!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY))api=&wake;
 else if(!strcmp(name,RISC_PROVIDER_PROMOTION_CAPABILITY))api=&promotion;
 else {assert(promoted);
  if(!strcmp(name,"display.output")){static risc_display_output_api_v1 display;display=d;display.get_info=display_info;display.acquire=display_acquire;display.release=display_release;display.present_status=display_status;display.wait_present=wait_frame;display.submit=submit_frame;api=&display;}
  else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==2&&!id);api=service;}
  else if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 touch;touch=t;touch.subscribe=touch_subscribe;touch.unsubscribe=touch_unsubscribe;touch.poll=touch_poll;touch.next=touch_next;touch.snapshot=touch_snapshot;api=&touch;}
  else if(!strcmp(name,"input.navigation"))api=&navigation;
  else if(!strcmp(name,"net.wifi"))api=&wifi;
  else if(!strcmp(name,"bluetooth.hci"))api=&bt;
  else if(!strcmp(name,"board.battery")){static risc_battery_gauge_api_v1 battery;battery=battery_api;battery.read=battery_now;api=&battery;}
  else if(!strcmp(name,"storage.key-value")){assert(id==1||id==5);static risc_key_value_v1 keyvalue;keyvalue=kv;keyvalue.get=preferences;keyvalue.put=no_put;api=&keyvalue;}
  else if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)){assert(version==1&&!id);api=&native_api;native_live++;}
  else if(!strcmp(name,"rtc.clock")){assert(version==2&&!id);if(which("rtc-acquire-error")){forbid=true;return false;}static twatch_rtc_api_v1 calendar;calendar=rtc_api;calendar.read=rtc_read_now;calendar.write=rtc_write_now;api=rtc_table=&calendar;rtc_live++;}
  else {fprintf(stderr,"Unexpected capability %s\n",name);assert(0);}
 }
 grant->api=api;grant->slot=++grants;grant->generation=1;return true;
}
static bool drop(risc_runtime_capability_v1 *grant){safe();assert(grants&&grant->api);releases++;
 if(grant->api==rtc_table){if(which("rtc-release-error")){forbid=true;return false;}assert(rtc_live==1);rtc_live--;rtc_release_event=++events;}
 if(grant->api==&native_api){if(which("native-release-error")){forbid=true;return false;}assert(native_live==1);native_live--;native_release_event=++events;}
 grants--;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
static bool retain(void){assert(running);retentions++;forbid=true;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health_now,.yield_ms=wait_ms,.diagnostic=report,.request_launch=launch,.acquire=obtain,.release=drop,.retain_invocation=retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
int main(int argc,char **argv){assert(argc==2);test=argv[1];scenario=0;native_valid=which("native-valid");
 assert(app_module_init()==0);assert(!grants&&!acquires&&!kv_reads&&!native_reads&&!service_started);running=true;app_main();
 if(retained_case()) {assert(portable_app_sleep_retained()&&retentions==1&&grants&&forbid);unsigned held=grants;app_module_fini();assert(grants==held&&!kv_writes&&!rtc_writes&&!ledger_writes&&!launches);
  if(which("alarm-native-context")||which("alarm-storage-context"))assert(seeds==1&&rtc_reads==1&&seed_event<first_alarm_event);
  if(which("rtc-read-error")||which("rtc-release-error"))assert(rtc_reads==1&&!seeds);
  if(which("rtc-acquire-error"))assert(!rtc_reads&&!seeds);
  if(!which("alarm-native-context")&&!which("alarm-storage-context"))assert(!first_alarm_event);
  printf("Cold actual alarm %s: retained with no post-fault I/O\n",test);return 0;}
 assert(!forbid&&!retentions&&!portable_app_sleep_retained());alarm_status_v1 state={.struct_size=sizeof(state)};assert(service->status(service->context,&state)==ALARM_OK);
 bool baseline=getenv("EXPECT_DELIVERED_FAILURE")!=NULL;
 bool error_expected=baseline||fail_policy()||ambiguous()||which("invalid-rtc")||which("seed-error");
 if(error_expected){assert(state.state==ALARM_STATE_BLOCKED);assert(state.error==(which("bad-zone")||which("unreadable-zone")?ALARM_STORAGE:ALARM_RTC));assert(error_rendered&&presents>=3);}
 else {assert(state.state==ALARM_STATE_READY&&state.error==ALARM_OK&&!state.output_uncertain&&!error_rendered&&presents==2);assert(alarm_native_reads);}
 if(baseline){assert(!native_valid&&!seeds&&!rtc_reads&&!ledger_writes);}
 else if(expect_seed()){assert(seeds==1&&rtc_reads==1&&native_valid);assert(seed_epoch==INT64_C(1791331197)+(which("missing-basis")?21600:0));assert(seed_event<first_alarm_event&&native_release_event>seed_event);}
 else if(which("seed-error")){assert(seeds==1&&rtc_reads==1&&!native_valid);}
 else {assert(!seeds);assert(rtc_reads==((ambiguous()||which("invalid-rtc"))?1u:0u));}
 assert(first_alarm_event&&bound_reads&&!kv_writes&&!rtc_writes&&provider_starts==1&&launches==1);
 app_module_fini();assert(!grants&&!subs&&!frames&&!native_live&&!rtc_live&&acquires==releases);
 unsigned before=bound_reads+alarm_native_reads;assert(provider->quiesce());assert(before==bound_reads+alarm_native_reads&&!kv_writes&&!rtc_writes);
 const char *capture=getenv("ALARM_ERROR_FRAME");if(capture){assert(error_rendered);FILE *out=fopen(capture,"wb");assert(out&&fwrite(error_pixels,1,sizeof(error_pixels),out)==sizeof(error_pixels)&&!fclose(out));}
 printf("Cold actual alarm %s: state=%u error=%d seeds=%u RTC reads=%u Clock KV/RTC writes=0 provider ledger writes=%u seed_event=%u first_alarm_event=%u cleanup=complete\n",test,state.state,state.error,seeds,rtc_reads,ledger_writes,seed_event,first_alarm_event);return 0;
}
