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
#ifdef ALARM_SERVICE_TAGGED_V2
#include "AlarmServiceV2.h"
#endif
#include <setjmp.h>
#include <time.h>
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#include "PointsUtcSchedule.h"
#endif
static const char *test,*state_path;
static bool terminal,in_main,loaded,pending,panel_off,touch_off,sd_off,dark,promoted,native_valid;
static bool seeded_image,promotion_attempted,loaded_pixels,rtc_live;
static unsigned barriers;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
static unsigned home_reads,home_acquires;
#endif
static unsigned entries,seeds,restores,clears,promotions,starts,kv_reads,rtc_reads,native_reads;
static unsigned battery_acquires,display_acquires,native_live,seed_calls,key_reads,high_water;
static uint32_t epoch=1791331197u,start_ms;static int native_context,promotion_context;
static uint8_t physical[sizeof(pixels)];static risc_retained_wake_record_v1 value;
static risc_display_output_api_v1_power panel;static risc_touch_power_api_v1 touch_power;
static risc_storage_volume_api_v1_sleep sd;static x4_power_deep_v1 power;
static jmp_buf entry;
static bool which(const char *s){return !strcmp(test,s);}
static bool raw_navigation;
static unsigned raw_overlay_frames,raw_dismiss_polls,raw_return_polls;
static uint32_t raw_submitted_at;
static bool raw_async,raw_present_pending,raw_navigation_sent;
static uint8_t raw_clock_pixels[sizeof(pixels)];
/* Inspection scenarios drive only the production raw input capability. They
 * never assign QuickActions state or invoke its close/render functions. */
static bool raw_lifetime,raw_brightness,raw_dismiss_started,raw_restored;
static bool raw_sheet_seen,raw_sheet_completed,raw_released;
static unsigned raw_sheet_updates,raw_inspection_polls,raw_hardware_brightness=40;
static unsigned raw_brightness_writes,raw_brightness_value;
static uint32_t raw_released_at,raw_brightness_at;
static uint8_t raw_sheet_pixels[sizeof(pixels)];
static void raw_assert_controls(const uint8_t *image){
 /* Native rotation maps logical y to native x. Exclude only the clock header
  * and battery footer; every slider, tile and dismissal row must survive. */
 for(unsigned y=0;y<480;y++)assert(!memcmp(image+y*100+12,raw_sheet_pixels+y*100+12,78));
}
static bool raw_inspection_snapshot(risc_touch_snapshot_v1 *out){
 int x=200,y=300;bool down=false;
 if(!raw_sheet_seen){if(polls==3||polls==4){down=true;y=polls==3?20:120;}}
 else if(!raw_dismiss_started){
  if(!raw_released){raw_released=true;raw_released_at=ms;}
  if(raw_lifetime){
   if(raw_sheet_completed)raw_assert_controls(physical);
   if((uint32_t)(ms-raw_released_at)>=90000u){
    assert(raw_sheet_updates>=3);raw_dismiss_started=true;raw_dismiss_polls=0;
   }
  }else if(raw_sheet_completed){
   unsigned step=++raw_inspection_polls;
   if(step==3){raw_brightness_at=ms;down=true;x=144;y=150;}
   if(step==4||step==5){down=true;x=step==4?310:364;y=150;}
   if(step>6 && (uint32_t)(ms-raw_brightness_at)>=600u && !raw_present_pending){
    if(raw_hardware_brightness!=100||raw_brightness_writes!=1||raw_brightness_value!=100)fprintf(stderr,"Brightness hardware=%u writes=%u saved=%u step=%u ms=%u presents=%u\n",raw_hardware_brightness,raw_brightness_writes,raw_brightness_value,step,ms,presents);
    assert(raw_hardware_brightness==100 && raw_brightness_writes==1 && raw_brightness_value==100);
    const char *path=getenv("RAW_QUICK_FRAME");assert(path);FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(physical,1,sizeof(physical),f)==sizeof(physical));assert(!fclose(f));
    raw_dismiss_started=true;raw_dismiss_polls=0;
   }
  }
 }
 if(raw_dismiss_started){
  unsigned step=++raw_dismiss_polls;
  if((strstr(test,"close")||raw_brightness)&&step==1){down=true;x=240;y=675;}
  if(strstr(test,"center")&&step==1)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
  if(strstr(test,"dismiss-swipe")&&(step==1||step==2)){down=true;x=240;y=step==1?620:500;}
  if(raw_restored&&!raw_present_pending){
   unsigned after=++raw_return_polls;
   if(after==4||after==5){down=true;y=300;x=after==4?200:280;}
  }
 }
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 return true;
}
static bool raw_snapshot(void *c,risc_touch_snapshot_v1 *out){
 (void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(raw_lifetime||raw_brightness)return raw_inspection_snapshot(out);
 unsigned step=polls;
 if(raw_async&&strstr(test,"quick")){
  if(presents>=3)step=raw_present_pending?11:11+(++raw_return_polls);
  else if(presents>=2)step=raw_present_pending?5:5+(++raw_dismiss_polls);
  else if(step>5)step=5;
 }
 bool quick=strstr(test,"quick")!=NULL;int x=200,y=300;bool down=false;
 if(step==1)memcpy(raw_clock_pixels,pixels,sizeof(pixels));
 if(quick){
  if(step==3||step==4){down=true;y=step==3?20:120;}
  if(strstr(test,"close")&&step==7){down=true;x=240;y=675;}
  if(strstr(test,"center")&&step==7)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
  if(strstr(test,"dismiss-swipe")&&(step==7||step==8)){down=true;x=240;y=step==7?620:500;}
  if(step==12){assert(presents>=3);assert(!memcmp(raw_clock_pixels,pixels,sizeof(pixels)));raw_overlay_frames=presents;}
  if(step==14||step==15){down=true;y=300;x=step==14?200:280;}
 }else{
  unsigned start=strstr(test,"held")?8:strstr(test,"center")?4:3;
  if(strstr(test,"held")&&step<5){down=true;x=step<3?200:280;}
  if(strstr(test,"center")&&step==2)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
  if(step==start||step==start+1){
   down=true;x=200;y=strstr(test,"top-replay")?20:300;
   if(step==start+1){if(strstr(test,"left"))x-=80;else if(strstr(test,"up"))y-=100;else if(strstr(test,"down"))y+=100;else x+=80;}
  }
 }
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 return true;
}
static void io(void){assert(in_main&&!terminal);}
static void safe(void){io();assert(!panel_off&&!touch_off&&!sd_off);}
static bool healthy(risc_runtime_health_v1*h){io();h->uptime_ms=ms;assert(polls<(raw_lifetime||raw_brightness?500u:80u));return true;}
static void wait_ms(uint32_t n){safe();if(raw_lifetime&&raw_sheet_completed&&!raw_dismiss_started&&n==8)n=1000;ms+=n;}
static bool report(const char*s){safe();return diagnostic(s);}
static bool launch(const char*s){safe();assert(promoted);return launch_app(s);}
static bool read_key(void*c,bool*down){(void)c;io();key_reads++;
 if(which("key-retained")&&presents){terminal=true;return false;}
 *down=which("held") || (which("cancel")&&presents);return true;}
static bool bright(void*c,uint16_t level,uint16_t max){(void)c;io();assert(max==100&&(raw_brightness||level==0||level==40));raw_hardware_brightness=level;dark=level==0;return true;}
static bool seed_previous(void*c,risc_display_frame_v1 f){(void)c;safe();assert(loaded_pixels&&frames&&f==1&&!subs&&!memcmp(pixels,physical,sizeof(pixels)));seeds++;
 if(which("seed-retained")){terminal=true;return false;}seeded_image=true;return true;}
static bool frame_acquire(void*c,uint32_t f,risc_display_surface_v1*out){safe();seeded_image=false;if(which("cold-acquire")){terminal=true;return false;}return acquire_frame(c,f,out);}
static void frame_release(void*c,risc_display_frame_v1 f){safe();seeded_image=false;release_frame(c,f);}
static bool frame_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){(void)c;(void)r;(void)n;safe();assert(frames&&f==1);
 if(!promoted&&loaded_pixels&&value.payload[24])assert(seeds&&seeded_image&&o->intent==RISC_DISPLAY_PRESENT_QUALITY);
 if(which("cold-submit")){terminal=true;return false;}
 if(raw_lifetime||raw_brightness){
  if(!presents)memcpy(raw_clock_pixels,pixels,sizeof(pixels));
  else if(!raw_dismiss_started){
   if(!raw_sheet_seen){assert(memcmp(pixels,raw_clock_pixels,sizeof(pixels)));memcpy(raw_sheet_pixels,pixels,sizeof(pixels));raw_sheet_seen=true;}
   else if(raw_lifetime)raw_assert_controls(pixels);
   raw_sheet_updates++;
  }else if(!memcmp(pixels,raw_clock_pixels,sizeof(pixels)))raw_restored=true;
  raw_overlay_frames=presents+1;
 }
 seeded_image=false;frames=0;*out=++presents;raw_submitted_at=ms;raw_present_pending=raw_async;return true;}
static bool raw_present_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 (void)c;safe();assert(token==presents);out->state=ms-raw_submitted_at<140?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){memcpy(physical,pixels,sizeof(pixels));raw_present_pending=false;if(raw_sheet_seen)raw_sheet_completed=true;}
 return true;
}
static bool raw_display_info(void *c,risc_display_info_v1 *out){get_info(c,out);if(raw_async)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;if(raw_brightness)out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;return true;}
static bool frame_wait(void*c,risc_display_present_token_v1 tkn,uint32_t timeout,risc_display_present_status_v1*out){(void)c;safe();assert(tkn==presents&&timeout);if(which("cold-wait")){terminal=true;return false;}if(presents==1&&getenv("PAPER_BOOT_FRAME")){FILE *capture=fopen(getenv("PAPER_BOOT_FRAME"),"wb");assert(capture);assert(fwrite(pixels,1,sizeof(pixels),capture)==sizeof(pixels));assert(!fclose(capture));}ms+=350;out->state=RISC_DISPLAY_PRESENT_COMPLETE;memcpy(physical,pixels,sizeof(pixels));if(raw_sheet_seen)raw_sheet_completed=true;return true;}
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
 *cause=!strncmp(test,"cold",4)?RISC_BOOT_POWER_ON:!strncmp(test,"reset",5)?RISC_BOOT_RESET:which("other-wake")?RISC_BOOT_DEEP_OTHER:which("gpio")?RISC_BOOT_DEEP_GPIO:RISC_BOOT_DEEP_TIMER;
 if(which("record-context")){terminal=true;return RISC_RETAINED_WAKE_CONTEXT;}
 if(!strncmp(test,"home-",5)||raw_navigation||!strncmp(test,"cold",4)||!strncmp(test,"reset",5)||which("manual")||which("foreground")||which("alarm-output")||which("alarm-uncertain")||!strncmp(test,"promotion-",10))return RISC_RETAINED_WAKE_ABSENT;
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
 unsigned leave=4;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(!strncmp(test,"home-",5))leave=which("home-minute")?65u:12u;
#endif
 if(raw_lifetime||raw_brightness){
  if(raw_dismiss_started&&!raw_navigation_sent&&raw_dismiss_polls==1){raw_navigation_sent=true;if(strstr(test,"back"))out->pressed=out->released=RISC_NAV_BACK;else if(strstr(test,"crown"))out->pressed=out->released=RISC_NAV_HOME;}
 }else if(raw_navigation){
  if(!raw_navigation_sent&&(raw_async?raw_dismiss_polls==2:polls==7)){raw_navigation_sent=true;if(strstr(test,"quick-back"))out->pressed=out->released=RISC_NAV_BACK;else if(strstr(test,"quick-crown"))out->pressed=out->released=RISC_NAV_HOME;}
 }else if(polls==leave)out->pressed=out->released=which("manual")?RISC_NAV_HOME:RISC_NAV_CONFIRM;
 return true;}
static bool nav_foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;(void)f;(void)n;safe();assert(promoted);return true;}
static bool nav_reset(void*c){(void)c;safe();assert(promoted);return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,nav_poll,nav_foreground,nav_reset};
static int32_t preferences(void*c,const char*k,void*out,uint32_t cap,uint32_t*size){(void)c;safe();assert(promoted);kv_reads++;
 if(raw_brightness&&!strcmp(k,"brightness")&&raw_brightness_writes){assert(cap>=1);*(uint8_t*)out=(uint8_t)raw_brightness_value;*size=1;return RISC_KEY_VALUE_OK;}
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
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
static int32_t home_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*size) {
 (void)c;safe();assert(promoted&&cap>=POINTS_RECORD_SIZE);home_reads++;
 if(which("home-context")){terminal=true;return RISC_KEY_VALUE_CONTEXT;}
 if(which("home-invalid")){memset(out,0,POINTS_RECORD_SIZE);*size=POINTS_RECORD_SIZE;return RISC_KEY_VALUE_OK;}
 if(which("home-storage-error")||which("home-unavailable"))return RISC_KEY_VALUE_IO;
 if(which("home-default")){*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
 if(!strcmp(k,POINTS_META_KEY)) {
  if(which("home-meta-invalid")){memset(out,0,POINTS_RECORD_SIZE);*size=POINTS_RECORD_SIZE;return RISC_KEY_VALUE_OK;}
  points_meta meta={.revision=3,.custom={{.color=6,.name="DRIVE TO WORK"}}};
  points_meta_encode(&meta,out);*size=POINTS_RECORD_SIZE;return RISC_KEY_VALUE_OK;
 }
 assert(!strcmp(k,POINTS_CONFIG_KEY));
 points_config config={.revision=4,.created=1};
 if(!which("home-empty")) {
  config.points[0]=(points_item){POINTS_WORK_START,1,0,127,8,30,0,0,0};
  config.points[1]=(points_item){POINTS_LUNCH,1,0,127,12,0,60,0,1};
  config.points[2]=(points_item){POINTS_BREAK,1,0,127,15,0,15,0,0};
  config.points[3]=(points_item){POINTS_WORK_END,1,0,127,17,0,0,0,0};
  if(which("home-custom"))config.points[1].kind=POINTS_CUSTOM_1;
 }
 points_config_encode(&config,out);*size=POINTS_RECORD_SIZE;return RISC_KEY_VALUE_OK;
}
static bool home_snapshot(void*c,risc_touch_snapshot_v1*out) {
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 bool down=false;int x=240,y=560;
 if(which("home-tap")||which("home-custom-tap")||which("home-swipe")||which("home-cancel")||which("home-retry"))down=polls>=2&&polls<=4;
 if(which("home-retry")&&polls>=7&&polls<=9)down=true;
 if(which("home-swipe")&&polls>=3)x+=60;
 if(which("home-held"))down=polls<5;
 if(which("home-cancel")&&polls==3){out->contact_count=2;return true;}
 if(down){out->contact_count=1;out->contacts[0].id=1;out->contacts[0].x=(uint16_t)x;out->contacts[0].y=(uint16_t)y;}
 return true;
}
static const risc_key_value_v1 home_kv={1,sizeof(home_kv),NULL,home_get,NULL};
#endif
static int32_t no_put(void*c,const char*k,const void*v,uint32_t n){(void)c;
 if(raw_brightness&&!strcmp(k,"brightness")){assert(n==1);raw_brightness_writes++;raw_brightness_value=*(const uint8_t*)v;return RISC_KEY_VALUE_OK;}
 (void)k;(void)v;(void)n;assert(0);return -1;}
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
 promoted=true;return which("promotion-ready")||which("cold-reload")||which("reset-reload")?RISC_PROVIDER_PROMOTION_ALREADY_READY:RISC_PROVIDER_PROMOTION_OK;}
static const risc_provider_promotion_api_v1 promotion={1,sizeof(promotion),&promotion_context,promote};
static bool read_rtc(void*c,twatch_rtc_time_v1*out){(void)c;safe();assert(promoted&&rtc_live);rtc_reads++;
 if(which("rtc-read-retained")){terminal=true;return false;}
 if(which("fold")){*out=(twatch_rtc_time_v1){2026,11,1,0,1,30,0};return true;}
 if(which("gap")){*out=(twatch_rtc_time_v1){2026,3,8,0,2,30,0};return true;}
 time_t stamp=epoch;struct tm*tm=gmtime(&stamp);assert(tm);*out=(twatch_rtc_time_v1){(uint16_t)(tm->tm_year+1900),(uint8_t)(tm->tm_mon+1),(uint8_t)tm->tm_mday,(uint8_t)tm->tm_wday,(uint8_t)tm->tm_hour,(uint8_t)tm->tm_min,(uint8_t)tm->tm_sec};return true;}
static bool read_battery(void*c,risc_battery_sample_v1*out){safe();assert(promoted);return battery_read(c,out);}
static bool obtain(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){safe();assert(grants<16&&g->struct_size==sizeof(*g));const void*api=NULL;
 if(!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY)){assert(version==1&&!id);api=&wake;}
 else if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)){assert(version==1&&!id);assert(!promotion_attempted||promoted);if(which("absent-native"))return false;if(which("native-acquire-retained")){terminal=true;g->slot=9;return false;}api=&realtime;native_live++;}
 else if(!strcmp(name,RISC_PROVIDER_PROMOTION_CAPABILITY)){assert(version==1&&!id);api=&promotion;}
 else if(!strcmp(name,"display.output")){assert(version==1&&(id==0||id==3));starts++;display_acquires++;api=&panel;}
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){
  #ifdef ALARM_SERVICE_TAGGED_V2
  assert(version==2);static alarm_service_descriptor_v2 service;service=(alarm_service_descriptor_v2){.base=alarm_api,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION};
  service.base.api_version=2;service.base.struct_size=sizeof(service);service.base.prepare_sleep=prepare_alarm;service.base.step=step_alarm;service.base.status=status_alarm;api=&service.base;
#else
  static alarm_service_v1 service;service=alarm_api;service.prepare_sleep=prepare_alarm;service.step=step_alarm;service.status=status_alarm;api=&service;
#endif
 }
 else if(!strcmp(name,X4_POWER_CAPABILITY)){assert(version==1&&id==17);api=&power;}
 else {assert(promoted&&promotion_attempted);
  if(!strcmp(name,"input.touch.raw"))api=&touch_power;
  else if(!strcmp(name,"input.navigation"))api=&navigation;
  else if(!strcmp(name,"board.battery")){static risc_battery_gauge_api_v1 b;b=battery_api;b.read=read_battery;api=&b;battery_acquires++;}
  else if(!strcmp(name,"rtc.clock")){assert(version==2&&!id);if(which("rtc-acquire-retained")){terminal=true;return false;}static twatch_rtc_api_v1 r;r=rtc_api;r.read=read_rtc;api=&r;rtc_live=true;}
  else if(!strcmp(name,"storage.key-value")){
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   if(id==5){assert(version==1);home_acquires++;if(which("home-acquire")){terminal=true;return false;}api=&home_kv;}
   else
#endif
   {assert(version==1&&id==1);static risc_key_value_v1 keyvalue;keyvalue=kv;keyvalue.get=preferences;keyvalue.put=no_put;api=&keyvalue;}}
  else if(!strcmp(name,"storage.volume"))api=&sd;
  else if(!strcmp(name,"net.wifi"))api=&wifi;
  else if(!strcmp(name,"bluetooth.hci"))api=&bt;
  else assert(0);
 }
 g->api=api;g->slot=++grants;g->generation=1;if(grants>high_water)high_water=grants;return true;}
static bool drop(risc_runtime_capability_v1*g){safe();assert(grants&&g->api);
 #ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(which("home-release")&&g->api==&home_kv){terminal=true;return false;}
#endif
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
int main(int argc,char**argv){assert(argc==3);test=argv[1];state_path=argv[2];scenario=0;raw_navigation=!strncmp(test,"raw-",4);raw_async=getenv("RAW_ASYNC")!=NULL;raw_lifetime=strstr(test,"inspect")!=NULL;raw_brightness=strstr(test,"brightness")!=NULL;if(raw_brightness)epoch-=epoch%60;
 panel.history.base=d;panel.history.base.get_info=raw_display_info;if(raw_async)panel.history.base.present_status=raw_present_status;panel.history.base.struct_size=sizeof(panel);panel.history.base.acquire=frame_acquire;panel.history.base.release=frame_release;panel.history.base.submit=frame_submit;panel.history.base.wait_present=frame_wait;panel.history.base.set_brightness=bright;panel.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;panel.history.extension_version=1;panel.history.seed_previous=seed_previous;panel.power_tag=RISC_DISPLAY_POWER_TAG;panel.power_version=1;panel.prepare=panel_prepare;panel.resume=panel_resume;
 touch_power.base=t;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(!strncmp(test,"home-",5))touch_power.base.snapshot=home_snapshot;
 if(which("home-retry"))scenario=9;
#endif
 if(raw_navigation)touch_power.base.snapshot=raw_snapshot;touch_power.base.struct_size=sizeof(touch_power);touch_power.power_tag=RISC_TOUCH_POWER_TAG;touch_power.power_version=1;touch_power.prepare=touch_prepare;touch_power.resume=touch_resume;
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
 native_valid=!(which("unset")||which("missing-zone")||which("missing-basis")||which("bad-zone")||which("bad-basis")||which("fold")||which("gap")||which("rtc-acquire-retained")||which("rtc-read-retained"));
 if(which("init-nosuffix")||which("init-missing-barrier")) {
  if(which("init-nosuffix"))runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
  else runtime.retain_invocation=NULL;
  assert(app_module_init()==-1);app_module_fini();assert(!grants&&!starts&&!native_reads&&!barriers);return 0;
 }
 if(getenv("CLOCK_MILLIS"))ms=(uint32_t)strtoul(getenv("CLOCK_MILLIS"),NULL,10);
 assert(app_module_init()==0);assert(!grants&&!starts&&!kv_reads&&!native_reads);in_main=true;start_ms=ms;
 if(setjmp(entry)){assert(terminal&&entries&&grants&&!subs&&!frames&&!native_live);
#ifndef PORTABLE_SLEEP_MANUAL_ONLY
 if(raw_lifetime){assert(raw_dismiss_started&&raw_restored&&raw_sheet_updates>=3&&(uint32_t)(ms-raw_released_at)>=90000u);printf("Paper inspection completed before ordinary idle sleep PASS\n");return 0;}
#endif
 if(!which("manual")){assert(!promoted&&!kv_reads&&!rtc_reads&&!battery_acquires&&!promotions);
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 assert(!home_reads&&!home_acquires);
#endif
 assert(high_water<=6);}
 else assert(promoted&&promotions==1&&battery_acquires==1&&high_water<=16);
 printf("Sparse terminal PASS (%u grants max, %u native reads)\n",high_water,native_reads);return 0;}
 app_main();if(which("cold-acquire")||which("cold-submit")||which("cold-wait"))assert(!rtc_reads&&!native_reads&&!launches);if(portable_app_sleep_retained()){assert(terminal&&(grants||which("native-acquire-retained"))&&barriers==1);unsigned held=grants;app_module_fini();assert(grants==held);printf("Sparse retained %s PASS\n",test);return 0;}
 assert(!terminal);app_module_fini();assert(!grants&&!subs&&!frames&&!pending&&!panel_off&&!touch_off&&!sd_off&&!native_live);
 assert(promotions==(which("promotion-failed")||which("promotion-partial")||which("refused-promotion-failed")||which("refused-promotion-partial")?2u:1u));
 if(!which("promotion-failed")&&!which("refused-promotion-failed")){assert(promoted&&battery_acquires==1&&launches==(which("home-retry")?2u:1u));}
 if(raw_navigation){assert(!entries&&!strcmp(launched,"springboard.elf"));if(strstr(test,"quick"))assert(raw_overlay_frames>=3);if(strstr(test,"held"))assert(polls>=9);}
 if(raw_lifetime||raw_brightness){assert(raw_restored&&raw_dismiss_started);if(raw_lifetime)assert((uint32_t)(ms-raw_released_at)>=90000u);}
 if(which("unset")||which("missing-zone")||which("missing-basis"))assert(seed_calls==1&&rtc_reads==1);
 if(which("bad-zone")||which("bad-basis"))assert(!seed_calls&&!rtc_reads);
 if(which("fold")||which("gap"))assert(!seed_calls&&rtc_reads==1);
 if(which("refused"))assert(entries==1&&restores==1&&clears==1&&display_acquires==2);
 if(which("promotion-failed"))assert(!starts&&!battery_acquires&&!kv_reads&&!entries);
 if(which("refused-promotion-failed"))assert(starts==2&&!battery_acquires&&!kv_reads&&entries==1);
 #ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(!strncmp(test,"home-",5)) {
  bool tapped=which("home-tap")||which("home-custom-tap")||which("home-retry");
  assert(!strcmp(launched,tapped?"points_in_time.elf":"springboard.elf"));
  if(!which("home-unavailable"))assert(home_reads);
  if(which("home-minute"))assert(presents>=2&&home_acquires>=2);
 }
#endif
 const char*capture=getenv("PAPER_FRAME");if(capture){FILE*out=fopen(capture,"wb");assert(out);assert(fwrite(physical,1,sizeof(physical),out)==sizeof(physical));assert(!fclose(out));}
 printf("Sparse foreground %s PASS (%u grants max, %u native reads)\n",test,high_water,native_reads);return 0;
}
