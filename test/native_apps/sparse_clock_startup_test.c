/* Actual Clock controller + adapter + X4 local sleep. Providers/native Runtime
 * are strict host doubles: graph ownership still needs separate Runtime tests. */
#define main unused_clock_fixture_main
#define risc_runtime_get_api unused_clock_fixture_runtime
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "paper_clock_test.c"
#pragma GCC diagnostic pop
#undef main
#undef risc_runtime_get_api
#include "PortableDeskClockApp.h"
#include "PortableAppSleep.h"
#include "PortableDeskClockSettings.h"
#include "PortableTimeZonePreference.h"
#include "PortableRtcBasis.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
#include "RiscInputNavigationV1.h"
#include "RiscRealtimeV1.h"
#include "RiscProviderPromotionV1.h"
#include "RiscRetainedWakeV1.h"
#include "RiscDisplayOutputPowerV1.h"
#include "RiscTouchPowerV1.h"
#include "RiscStorageVolumeV1.h"
#include "X4PowerDeepV1.h"
#include <setjmp.h>
#include <time.h>
static const char *test,*state_path;
static bool terminal,in_main,loaded,pending,panel_off,touch_off,sd_off,dark,promoted,native_valid;
static bool seeded_image,promotion_attempted,loaded_pixels,rtc_live;
static unsigned barriers;
static unsigned entries,seeds,restores,clears,promotions,starts,kv_reads,rtc_reads,native_reads;
static unsigned battery_acquires,display_acquires,native_live,seed_calls,key_reads,high_water;
static uint32_t epoch=1791331197u,start_ms;static int native_context,promotion_context;
static uint8_t physical[sizeof(pixels)];static risc_retained_wake_record_v1 value;
static risc_display_output_api_v1_power panel;static risc_touch_power_api_v1 touch_power;
static risc_storage_volume_api_v1_sleep sd;static x4_power_deep_v1 power;
static jmp_buf entry;
static bool which(const char *s){return !strcmp(test,s);}
static void io(void){assert(in_main&&!terminal);}
static void safe(void){io();assert(!panel_off&&!touch_off&&!sd_off);}
static bool healthy(risc_runtime_health_v1*h){io();h->uptime_ms=ms;assert(polls<80);return true;}
static void wait_ms(uint32_t n){safe();ms+=n;}
static bool report(const char*s){safe();return diagnostic(s);}
static bool launch(const char*s){safe();assert(promoted);return launch_app(s);}
static bool read_key(void*c,bool*down){(void)c;io();key_reads++;
 if(which("key-retained")&&presents){terminal=true;return false;}
 *down=which("held") || (which("cancel")&&presents);return true;}
static bool bright(void*c,uint16_t level,uint16_t max){(void)c;io();assert(max==100&&(level==0||level==40));dark=level==0;return true;}
static bool seed_previous(void*c,risc_display_frame_v1 f){(void)c;safe();assert(loaded_pixels&&frames&&f==1&&!subs&&!memcmp(pixels,physical,sizeof(pixels)));seeds++;
 if(which("seed-retained")){terminal=true;return false;}seeded_image=true;return true;}
static bool frame_acquire(void*c,uint32_t f,risc_display_surface_v1*out){safe();seeded_image=false;return acquire_frame(c,f,out);}
static void frame_release(void*c,risc_display_frame_v1 f){safe();seeded_image=false;release_frame(c,f);}
static bool frame_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){(void)c;(void)r;(void)n;safe();assert(frames&&f==1);
 if(!promoted&&loaded_pixels&&value.payload[24])assert(seeds&&seeded_image&&o->intent==RISC_DISPLAY_PRESENT_QUALITY);
 seeded_image=false;frames=0;*out=++presents;return true;}
static bool frame_wait(void*c,risc_display_present_token_v1 tkn,uint32_t timeout,risc_display_present_status_v1*out){(void)c;safe();assert(tkn==presents&&timeout);ms+=350;out->state=RISC_DISPLAY_PRESENT_COMPLETE;memcpy(physical,pixels,sizeof(pixels));return true;}
static int32_t panel_prepare(void*c,uint32_t timeout){(void)c;io();assert(!panel_off&&!sd_off);assert(timeout&&!subs&&!frames&&dark&&!native_live);
 assert(promoted?touch_off:(!touch_off&&!sd_off));panel_off=true;
 if(which("panel-retained")){terminal=true;return RISC_DISPLAY_POWER_RETAINED;}
 if(which("slow-prepare"))ms+=65001;
 if(which("cross-minute"))ms+=3000;
 return which("panel-refused")?RISC_DISPLAY_POWER_BUSY:RISC_DISPLAY_POWER_OK;}
static int32_t panel_resume(void*c,uint32_t timeout){(void)c;(void)timeout;io();assert(panel_off&&!sd_off);
 if(which("resume-retained")){terminal=true;return RISC_DISPLAY_POWER_RETAINED;}
 panel_off=false;restores++;return RISC_DISPLAY_POWER_OK;}
static int32_t touch_prepare(void*c,uint32_t timeout){(void)c;safe();assert(promoted&&timeout&&!subs&&!frames);touch_off=true;return RISC_TOUCH_POWER_OK;}
static int32_t touch_resume(void*c,uint32_t timeout){(void)c;(void)timeout;io();assert(!panel_off&&!sd_off);touch_off=false;return RISC_TOUCH_POWER_OK;}
static bool prepare_sd(void*c){(void)c;io();assert(promoted&&!subs&&!frames&&panel_off&&touch_off);return true;}
static bool commit_sd(void*c){(void)c;io();sd_off=true;return true;}
static int32_t resume_sd(void*c){(void)c;io();sd_off=false;return RISC_STORAGE_SLEEP_READY;}
static bool legacy_sd(void*c){(void)c;assert(0);return false;}
static bool disconnect_wifi(void*c){(void)c;safe();assert(promoted);return true;}
static wifi_link_t wifi_status(void*c){(void)c;safe();assert(promoted);return WIFI_LINK_DOWN;}
static bool disable_bt(void*c,bool enabled){(void)c;(void)enabled;safe();assert(promoted);return true;}
static bool bt_status(void*c,uint8_t*out){(void)c;safe();assert(promoted);*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.status=wifi_status,.disconnect_checked=disconnect_wifi};
static const portable_bluetooth_control_v1 bt={.api_version=1,.struct_size=sizeof(bt),.set_enabled=disable_bt,.status=bt_status};
static int32_t step_alarm(void*c){safe();if(promoted&&which("alarm-output")){terminal=true;return ALARM_OUTPUT;}return alarm_step(c);}
static int32_t status_alarm(void*c,alarm_status_v1*out){safe();int32_t result=alarm_status(c,out);
 if(promoted&&which("alarm-uncertain")){out->output_uncertain=1;terminal=true;}return result;}
static int32_t prepare_alarm(void*c,alarm_sleep_v1*out){(void)c;safe();out->rtc_seconds=epoch+(ms-start_ms)/1000u;
 out->deadline=which("alarm-due")?out->rtc_seconds:0;return ALARM_OK;}
static int32_t read_record(void*c,uint32_t type,uint32_t schema,risc_retained_wake_record_v1*out,uint32_t*cause){(void)c;safe();assert(!starts&&!presents&&type==PORTABLE_DESK_CLOCK_RECORD_TYPE&&schema==1);
 *cause=which("gpio")?RISC_BOOT_DEEP_GPIO:RISC_BOOT_DEEP_TIMER;
 if(which("record-context")){terminal=true;return RISC_RETAINED_WAKE_CONTEXT;}
 if(which("cold")||which("manual")||which("foreground")||which("alarm-output")||which("alarm-uncertain")||!strncmp(test,"promotion-",10))return RISC_RETAINED_WAKE_ABSENT;
 *out=value;if(which("invalid-record"))out->payload[0]=0;return RISC_RETAINED_WAKE_OK;}
static int32_t stage_record(void*c,const risc_retained_wake_record_v1*in){(void)c;safe();assert(!subs&&!frames&&!native_live);value=*in;pending=true;ms+=17;
 if(which("stage-context")){terminal=true;return RISC_RETAINED_WAKE_CONTEXT;}
 return which("stage-refused")?RISC_RETAINED_WAKE_INVALID:RISC_RETAINED_WAKE_OK;}
static int32_t clear_record(void*c){(void)c;safe();clears++;
 if(which("clear-retained")){terminal=true;return RISC_RETAINED_WAKE_CONTEXT;}
 pending=false;return RISC_RETAINED_WAKE_OK;}
static const risc_retained_wake_api_v1 wake={1,sizeof(wake),NULL,read_record,stage_record,clear_record};
static int32_t deep(void*c,uint32_t duration){(void)c;io();assert(pending&&duration&&duration<=60000&&!subs&&!frames&&dark&&panel_off&&!native_live);
 assert(promoted?(touch_off&&sd_off):(!touch_off&&!sd_off));entries++;
 if(which("refused")||which("clear-retained")||which("resume-retained")||which("release-retained")||!strncmp(test,"refused-promotion-",18))return RISC_DEEP_SLEEP_BUSY;
 if(which("retained")){terminal=true;return RISC_DEEP_SLEEP_RETAINED;}
 FILE*out=fopen(state_path,"wb");assert(out);assert(fwrite(&value,1,sizeof(value),out)==sizeof(value));assert(fwrite(physical,1,sizeof(physical),out)==sizeof(physical));assert(!fclose(out));terminal=true;longjmp(entry,1);}
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){(void)c;safe();assert(promoted);*out=(risc_input_navigation_frame_v1){0};
 /* Explicit manual sleep for recovery/timezone scenarios; otherwise leave via
  * a foreground confirm after a neutral first frame. */
 if(polls==4)out->pressed=out->released=which("manual")?RISC_NAV_HOME:RISC_NAV_CONFIRM;
 return true;}
static bool nav_foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;(void)f;(void)n;safe();assert(promoted);return true;}
static bool nav_reset(void*c){(void)c;safe();assert(promoted);return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,nav_poll,nav_foreground,nav_reset};
static int32_t preferences(void*c,const char*k,void*out,uint32_t cap,uint32_t*size){(void)c;safe();assert(promoted);kv_reads++;
 if(!strcmp(k,PORTABLE_TIMEZONE_KEY)){
  if(which("missing-zone")){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
  uint8_t b[44]={'T','Z',1,0};const char*zone=getenv("CLOCK_ZONE");strcpy((char*)b+4,zone?zone:"UTC");b[3]=0xa5;
  for(unsigned i=0;i<44;i++)if(i!=3)b[3]^=b[i];
  if(which("bad-zone"))b[3]^=1;
  assert(cap>=sizeof(b));memcpy(out,b,sizeof(b));*size=sizeof(b);return RISC_KEY_VALUE_OK;
 }
 if(!strcmp(k,PORTABLE_RTC_BASIS_KEY)){
  if(which("missing-basis")){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
  uint8_t b[12]={0x52,0x54,1,1};if(which("fold")||which("gap"))b[3]=0;
  portable_rtc_basis_put_word(b+8,portable_rtc_basis_check(b));if(which("bad-basis"))b[0]=0;
  assert(cap>=sizeof(b));memcpy(out,b,sizeof(b));*size=sizeof(b);return RISC_KEY_VALUE_OK;
 }
 uint8_t b[]={0x53,1,1,0xa4};
 if(!strcmp(k,"time_format"))b[0]=0x54;
 else if(!strcmp(k,PORTABLE_DESK_FACE_KEY)){b[0]=0x46;b[2]=(uint8_t)(getenv("CLOCK_FACE")?atoi(getenv("CLOCK_FACE")):0);b[3]=(uint8_t)(b[2]^0xa5);}
 else if(!strcmp(k,"quick_radio")){b[0]=0x51;b[2]=0;b[3]=0xa5;}
 else if(strcmp(k,PORTABLE_SLEEP_KEY)){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
 assert(cap>=4);memcpy(out,b,4);*size=4;return RISC_KEY_VALUE_OK;
}
static int32_t no_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;assert(0);return -1;}
static int32_t read_native(void*c,risc_realtime_snapshot_v1*out){safe();assert(c==&native_context&&native_live&&!rtc_live);native_reads++;
 if(which("native-context")){terminal=true;return RISC_REALTIME_CONTEXT;}
 *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=native_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,
   .epoch_seconds=native_valid?(int64_t)epoch+(ms-start_ms)/1000u:0,.nanoseconds=native_valid?((ms-start_ms)%1000u)*1000000u:0,
   .monotonic_before_us=(uint64_t)ms*1000u,.monotonic_after_us=(uint64_t)ms*1000u};
 if(which("invalid-native"))out->nanoseconds=1000000000u;
 return RISC_REALTIME_OK;}
static int32_t seed_native(void*c,int64_t seconds,uint32_t nano){safe();assert(c==&native_context&&native_live&&promoted&&!rtc_live&&!nano);seed_calls++;
 epoch=(uint32_t)seconds;start_ms=ms;native_valid=true;return RISC_REALTIME_OK;}
static const risc_realtime_control_api_v1 realtime={1,sizeof(realtime),&native_context,read_native,seed_native};
static int32_t promote(void*c){safe();assert(c==&promotion_context);promotion_attempted=true;promotions++;
 if(which("promotion-retained")){terminal=true;return RISC_PROVIDER_PROMOTION_RETAINED;}
 if(which("promotion-failed")||which("refused-promotion-failed")||((which("promotion-partial")||which("refused-promotion-partial"))&&promotions==1))return RISC_PROVIDER_PROMOTION_FAILED;
 promoted=true;return which("promotion-ready")?RISC_PROVIDER_PROMOTION_ALREADY_READY:RISC_PROVIDER_PROMOTION_OK;}
static const risc_provider_promotion_api_v1 promotion={1,sizeof(promotion),&promotion_context,promote};
static bool read_rtc(void*c,twatch_rtc_time_v1*out){(void)c;safe();assert(promoted&&rtc_live);rtc_reads++;
 if(which("fold")){*out=(twatch_rtc_time_v1){2026,11,1,0,1,30,0};return true;}
 if(which("gap")){*out=(twatch_rtc_time_v1){2026,3,8,0,2,30,0};return true;}
 time_t stamp=epoch;struct tm*tm=gmtime(&stamp);assert(tm);*out=(twatch_rtc_time_v1){(uint16_t)(tm->tm_year+1900),(uint8_t)(tm->tm_mon+1),(uint8_t)tm->tm_mday,(uint8_t)tm->tm_wday,(uint8_t)tm->tm_hour,(uint8_t)tm->tm_min,(uint8_t)tm->tm_sec};return true;}
static bool read_battery(void*c,risc_battery_sample_v1*out){safe();assert(promoted);return battery_read(c,out);}
static bool obtain(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){safe();assert(grants<16&&g->struct_size==sizeof(*g));const void*api=NULL;
 if(!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY)){assert(version==1&&!id);api=&wake;}
 else if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)){assert(version==1&&!id);assert(!promotion_attempted||promoted);if(which("absent-native"))return false;if(which("native-acquire-retained")){terminal=true;g->slot=9;return false;}api=&realtime;native_live++;}
 else if(!strcmp(name,RISC_PROVIDER_PROMOTION_CAPABILITY)){assert(version==1&&!id);api=&promotion;}
 else if(!strcmp(name,"display.output")){assert(version==1&&(id==0||id==3));starts++;display_acquires++;api=&panel;}
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){static alarm_service_v1 service;service=alarm_api;service.prepare_sleep=prepare_alarm;service.step=step_alarm;service.status=status_alarm;api=&service;}
 else if(!strcmp(name,X4_POWER_CAPABILITY)){assert(version==1&&id==17);api=&power;}
 else {assert(promoted&&promotion_attempted);
  if(!strcmp(name,"input.touch.raw"))api=&touch_power;
  else if(!strcmp(name,"input.navigation"))api=&navigation;
  else if(!strcmp(name,"board.battery")){static risc_battery_gauge_api_v1 b;b=battery_api;b.read=read_battery;api=&b;battery_acquires++;}
  else if(!strcmp(name,"rtc.clock")){assert(version==2&&!id);if(which("rtc-acquire-retained")){terminal=true;return false;}static twatch_rtc_api_v1 r;r=rtc_api;r.read=read_rtc;api=&r;rtc_live=true;}
  else if(!strcmp(name,"storage.key-value")){assert(version==1&&id==1);static risc_key_value_v1 keyvalue;keyvalue=kv;keyvalue.get=preferences;keyvalue.put=no_put;api=&keyvalue;}
  else if(!strcmp(name,"storage.volume"))api=&sd;
  else if(!strcmp(name,"net.wifi"))api=&wifi;
  else if(!strcmp(name,"bluetooth.hci"))api=&bt;
  else assert(0);
 }
 g->api=api;g->slot=++grants;g->generation=1;if(grants>high_water)high_water=grants;return true;}
static bool drop(risc_runtime_capability_v1*g){safe();assert(grants&&g->api);
 if(which("native-release")&&g->api==&realtime){terminal=true;return false;}
 if(which("release-retained")&&g->api==&wake&&starts){terminal=true;return false;}
 if(g->api==&realtime){assert(native_live);native_live--;}
 if(g->api!=&realtime&&rtc_live){/* Recovery RTC is the only transient provider then. */assert(promoted);rtc_live=false;}
 grants--;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool retain_invocation(void){assert(in_main);barriers++;terminal=true;return true;}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=healthy,
 .yield_ms=wait_ms,.diagnostic=report,.request_launch=launch,.acquire=obtain,.release=drop,
 .retain_invocation=retain_invocation};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
int main(int argc,char**argv){assert(argc==3);test=argv[1];state_path=argv[2];scenario=0;
 panel.history.base=d;panel.history.base.struct_size=sizeof(panel);panel.history.base.acquire=frame_acquire;panel.history.base.release=frame_release;panel.history.base.submit=frame_submit;panel.history.base.wait_present=frame_wait;panel.history.base.set_brightness=bright;panel.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;panel.history.extension_version=1;panel.history.seed_previous=seed_previous;panel.power_tag=RISC_DISPLAY_POWER_TAG;panel.power_version=1;panel.prepare=panel_prepare;panel.resume=panel_resume;
 touch_power.base=t;touch_power.base.struct_size=sizeof(touch_power);touch_power.power_tag=RISC_TOUCH_POWER_TAG;touch_power.power_version=1;touch_power.prepare=touch_prepare;touch_power.resume=touch_resume;
 sd.terminal.power.volume.base=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(sd)};sd.terminal.extension_tag=RISC_STORAGE_POWER_COMMIT_TAG;sd.terminal.extension_version=1;sd.terminal.commit_power_down=legacy_sd;sd.sleep_tag=RISC_STORAGE_SLEEP_TAG;sd.sleep_version=1;sd.prepare_sleep=prepare_sd;sd.commit_sleep=commit_sd;sd.resume_sleep=resume_sd;
 power=(x4_power_deep_v1){{1,sizeof(power),NULL,read_key,NULL},X4_POWER_DEEP_TAG,1,deep};
 portable_desk_record record={.config={.face=0,.time_format=1,.rtc_stores_utc=1},.displayed_minute=1791331140,.has_image=true};
 const char*zone=getenv("CLOCK_ZONE");strcpy(record.config.time_zone,zone?zone:"UTC");
 if(getenv("CLOCK_FACE"))record.config.face=(uint8_t)atoi(getenv("CLOCK_FACE"));
 if(getenv("CLOCK_FLIP"))record.config.flip_ui=(uint8_t)atoi(getenv("CLOCK_FLIP"));
 if(getenv("CLOCK_LANG"))record.config.language=(uint8_t)atoi(getenv("CLOCK_LANG"));
 if(getenv("CLOCK_EPOCH")){epoch=(uint32_t)strtoul(getenv("CLOCK_EPOCH"),NULL,10);record.displayed_minute=(int64_t)(epoch-epoch%60);}
 value=(risc_retained_wake_record_v1){.struct_size=sizeof(value),.type=PORTABLE_DESK_CLOCK_RECORD_TYPE,.schema_version=1,.size=PORTABLE_DESK_CLOCK_RECORD_BYTES};
 assert(portable_desk_encode(&record,value.payload,value.size));loaded=true;
 FILE*in=fopen(state_path,"rb");if(in){assert(fread(&value,1,sizeof(value),in)==sizeof(value));assert(fread(physical,1,sizeof(physical),in)==sizeof(physical));assert(!fclose(in));assert(portable_desk_decode(value.payload,value.size,&record));epoch=(uint32_t)record.displayed_minute+60u;loaded_pixels=true;}
 native_valid=!(which("unset")||which("missing-zone")||which("missing-basis")||which("bad-zone")||which("bad-basis")||which("fold")||which("gap")||which("rtc-acquire-retained"));
 if(which("init-nosuffix")||which("init-missing-barrier")) {
  if(which("init-nosuffix"))runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
  else runtime.retain_invocation=NULL;
  assert(app_module_init()==-1);app_module_fini();assert(!grants&&!starts&&!native_reads&&!barriers);return 0;
 }
 if(getenv("CLOCK_MILLIS"))ms=(uint32_t)strtoul(getenv("CLOCK_MILLIS"),NULL,10);
 assert(app_module_init()==0);assert(!grants&&!starts&&!kv_reads&&!native_reads);in_main=true;start_ms=ms;
 if(setjmp(entry)){assert(terminal&&entries&&grants&&!subs&&!frames&&!native_live);
 if(!which("manual")){assert(!promoted&&!kv_reads&&!rtc_reads&&!battery_acquires&&!promotions);assert(high_water<=6);}
 else assert(promoted&&promotions==1&&battery_acquires==1&&high_water<=16);
 printf("Sparse terminal PASS (%u grants max, %u native reads)\n",high_water,native_reads);return 0;}
 app_main();if(portable_app_sleep_retained()){assert(terminal&&(grants||which("native-acquire-retained"))&&barriers==1);unsigned held=grants;app_module_fini();assert(grants==held);printf("Sparse retained %s PASS\n",test);return 0;}
 assert(!terminal);app_module_fini();assert(!grants&&!subs&&!frames&&!pending&&!panel_off&&!touch_off&&!sd_off&&!native_live);
 assert(promotions==(which("promotion-failed")||which("promotion-partial")||which("refused-promotion-failed")||which("refused-promotion-partial")?2u:1u));
 if(!which("promotion-failed")&&!which("refused-promotion-failed")){assert(promoted&&battery_acquires==1&&launches==1);}
 if(which("unset"))assert(seed_calls==1&&rtc_reads==1);
 if(which("missing-zone")||which("missing-basis")||which("bad-zone")||which("bad-basis"))assert(!seed_calls&&!rtc_reads);
 if(which("fold")||which("gap"))assert(!seed_calls&&rtc_reads==1);
 if(which("refused"))assert(entries==1&&restores==1&&clears==1&&display_acquires==2);
 if(which("promotion-failed"))assert(!starts&&!battery_acquires&&!kv_reads&&!entries);
 if(which("refused-promotion-failed"))assert(starts==2&&!battery_acquires&&!kv_reads&&entries==1);
 const char*capture=getenv("PAPER_FRAME");if(capture){FILE*out=fopen(capture,"wb");assert(out);assert(fwrite(physical,1,sizeof(physical),out)==sizeof(physical));assert(!fclose(out));}
 printf("Sparse foreground %s PASS (%u grants max, %u native reads)\n",test,high_water,native_reads);return 0;
}
