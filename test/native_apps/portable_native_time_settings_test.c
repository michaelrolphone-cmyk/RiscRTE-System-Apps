/* Real Apps/settings.c, production adapter/views and checked Set Time helpers.
 * Only the native providers are doubles. Every interaction enters app_main;
 * no test calls editor_save, settings_activate or mutates the editor draft. */
#define PORTABLE_SETTINGS_APP
#define PORTABLE_SETTINGS_NATIVE_TIME
#define PORTABLE_SETTINGS_TIME_ZONE
#define PORTABLE_SETTINGS_X4_DESK_CLOCK
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_DISPLAY_ROTATION 90
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_SETTINGS_VERSION "1.3.7"
#ifdef TEST_NATIVE_SETTINGS_ALARMS
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_ALARM_SETTINGS
#endif
#ifdef TEST_NATIVE_SETTINGS_SHORT
#define NATIVE_WIDTH 600
#define NATIVE_HEIGHT 400
#else
#define NATIVE_WIDTH 800
#define NATIVE_HEIGHT 480
#endif
#define LOGICAL_WIDTH NATIVE_HEIGHT
#define LOGICAL_HEIGHT NATIVE_WIDTH
#define SAVE_X (LOGICAL_WIDTH*3/4)
#define FOOTER_Y (LOGICAL_HEIGHT-68)
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/PortableApps/src/adapter.c"
#include "PortableSetTime.h"

void app_main(void);
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
enum { K_DISPLAY=1,K_TOUCH,K_NAV,K_KV,K_NATIVE,K_CONTROL,K_RTC,K_BATTERY,K_ALARM };
typedef struct {unsigned kind,generation;} lease;
static lease leases[32];
static unsigned generation,live,high_water,provider_calls,acquires,releases;
static unsigned ticks,polls,subscriptions,frames,presents,launches,barriers;
static unsigned native_reads,seeds,rtc_reads,rtc_writes,kv_reads,basis_puts,zone_puts,other_puts;
static unsigned nav_buttons[2048],next_at=5,last_at,contact_count;
typedef struct {unsigned at,x,y,id;} scripted_contact;
static scripted_contact contacts[256];
static uint8_t pixels[NATIVE_WIDTH*NATIVE_HEIGHT/8],zone_record[44],basis_record[12],flip_record[4];
static uint32_t zone_size=44,basis_size=12,flip_size=4;
static bool in_main,in_fini,hidden,retained,flipped,native_valid=true;
static bool initial_basis_missing,initial_basis_invalid,initial_basis_unavailable;
static bool initial_zone_missing,initial_zone_invalid,initial_zone_unavailable;
static bool saw_fields,saw_fold,saw_default_basis,saw_snapshot_unset;
static unsigned gap_poll,fail_poll,replaced_poll,capture_index;
static int64_t native_epoch,seed_epoch,expected_epoch;
static const char *test_name,*zone_id;
static twatch_rtc_time_v1 calendar,written,first_draft;
static char first_basis[64],final_status[128];
static bool which(const char *s){return !strcmp(test_name,s);}
void __real_free(void *pointer);
void __wrap_free(void *pointer){assert(!hidden&&!retained);__real_free(pointer);}
static void io(void) {
  assert((in_main||in_fini)&&!hidden&&!retained);
  ++provider_calls;
}
static bool kind_live(unsigned kind) {
  for(unsigned i=1;i<32;++i)if(leases[i].kind==kind)return true;
  return false;
}
static void capture(void) {
  const char *dir=getenv("NATIVE_SETTINGS_FRAMES");if(!dir)return;
  char path[1024];snprintf(path,sizeof(path),"%s/%s-%03u-page-%u.pbm",dir,test_name,capture_index++,sv_page);
  FILE *f=fopen(path,"wb");assert(f);assert(fprintf(f,"P4\n%d %d\n",NATIVE_WIDTH,NATIVE_HEIGHT)>0);
  assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));assert(!fclose(f));
}
static bool health(risc_runtime_health_v1 *out) {
  io();assert(polls<1900);out->uptime_ms=ticks;return true;
}
static void yielding(uint32_t ms){io();ticks+=ms;}
static bool diagnostic(const char *s){io();assert(s);return true;}
static bool request_launch(const char *path){io();assert(!strcmp(path,PORTABLE_HOME_APP));++launches;return true;}
static bool retain(void) {
  assert((in_main||in_fini)&&!retained);retained=true;++barriers;return true;
}
static bool get_info(void *ctx,risc_display_info_v1 *out) {
  (void)ctx;io();*out=(risc_display_info_v1){.width=NATIVE_WIDTH,.height=NATIVE_HEIGHT,
    .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),
    .flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT,
    .damage_x_alignment=8,.damage_width_alignment=8};return true;
}
static bool frame_get(void *ctx,uint32_t format,risc_display_surface_v1 *out) {
  (void)ctx;io();assert(!frames&&format==RISC_DISPLAY_FORMAT_MONO1);frames=1;
  *out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=NATIVE_WIDTH,.height=NATIVE_HEIGHT,
    .stride_bytes=NATIVE_WIDTH/8,.size_bytes=sizeof(pixels),.pixel_format=format};return true;
}
static void frame_drop(void *ctx,risc_display_frame_v1 frame){(void)ctx;io();assert(frame==1&&frames);frames=0;}
static bool frame_show(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,
 size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
  (void)ctx;(void)damage;(void)count;(void)options;io();assert(frame==1&&frames);
  frames=0;*token=++presents;
  if(sv_page==SV_FIELDS&&!saw_fields){first_draft=settings_draft;saw_fields=true;snprintf(first_basis,sizeof(first_basis),"%s",snt_basis_label());}
  if(sv_page==SV_FOLD)saw_fold=true;
  if(snt_basis_status==PORTABLE_RTC_BASIS_MISSING)saw_default_basis=true;
  if(snt_snapshot_status==PORTABLE_REALTIME_UNSET)saw_snapshot_unset=true;
  capture();return true;
}
static bool frame_state(void *ctx,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
  (void)ctx;io();assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool battery_read(void *ctx,risc_battery_sample_v1 *out){(void)ctx;io();*out=(risc_battery_sample_v1){3800,80,0};return true;}
static uint64_t subscribe(void *ctx){(void)ctx;io();assert(!subscriptions);subscriptions=1;return 1;}
static bool unsubscribe(void *ctx,uint64_t token){(void)ctx;io();assert(token==1&&subscriptions);subscriptions=0;return true;}
static bool touch_poll(void *ctx,size_t limit){(void)ctx;io();assert(limit==1);++polls;return polls!=fail_poll;}
static int32_t touch_next(void *ctx,uint64_t token,risc_touch_event_v1 *out){(void)ctx;(void)token;(void)out;io();return polls==gap_poll?-1:0;}
static bool touch_snapshot(void *ctx,risc_touch_snapshot_v1 *out) {
  (void)ctx;io();*out=(risc_touch_snapshot_v1){.width=LOGICAL_WIDTH,.height=LOGICAL_HEIGHT};
  for(unsigned i=0;i<contact_count;++i)if(contacts[i].at==polls) {
    unsigned x=contacts[i].x,y=contacts[i].y;
    if(flipped){x=LOGICAL_WIDTH-1-x;y=LOGICAL_HEIGHT-1-y;}
    out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=polls==replaced_poll?2:contacts[i].id,.x=x,.y=y};break;
  }
  return true;
}
static bool navigation_poll(void *ctx,risc_input_navigation_frame_v1 *out) {
  (void)ctx;io();assert(polls<2048);*out=(risc_input_navigation_frame_v1){0};
  if(getenv("NATIVE_SETTINGS_TRACE")&&nav_buttons[polls])fprintf(stderr,"poll %u nav %u page %u field %u selected %u\n",polls,nav_buttons[polls],sv_page,editor_field,sv_selected);
  out->buttons=out->pressed=nav_buttons[polls];return true;
}
static bool navigation_reset(void *ctx){(void)ctx;io();return true;}
static bool navigation_foreground(void *ctx,const risc_input_foreground_v1 *list,size_t n) {
  (void)ctx;io();if(n)assert(n==1&&list&&!strcmp(list[0].capability,"input.touch.raw"));else assert(!list);return true;
}
static int32_t native_read(void *ctx,risc_realtime_snapshot_v1 *out) {
  io();assert(ctx==leases&&(kind_live(K_NATIVE)||kind_live(K_CONTROL)));++native_reads;
  if(which("native-context")&&!seeds){hidden=true;return RISC_REALTIME_CONTEXT;}
  if(which("native-read-io")&&!seeds)return RISC_REALTIME_IO;
  if(which("native-readback-io")&&seeds)return RISC_REALTIME_IO;
  *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=native_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET,
    .epoch_seconds=native_valid?native_epoch:0,.monotonic_before_us=(uint64_t)ticks*1000+native_reads*100,
    .monotonic_after_us=(uint64_t)ticks*1000+native_reads*100+10};
  if(which("native-readback-mismatch")&&seeds)out->epoch_seconds+=20;
  return RISC_REALTIME_OK;
}
static int32_t native_seed(void *ctx,int64_t epoch,uint32_t nanoseconds) {
  io();assert(ctx==leases&&kind_live(K_CONTROL)&&!kind_live(K_RTC)&&!nanoseconds);++seeds;seed_epoch=epoch;
  if(which("native-seed-context")){hidden=true;return RISC_REALTIME_CONTEXT;}
  if(which("native-seed-io")||which("native-retry")) {
    if(seeds==1){native_valid=false;return RISC_REALTIME_IO;}
  }
  native_epoch=epoch;native_valid=true;return RISC_REALTIME_OK;
}
static bool rtc_read(void *ctx,twatch_rtc_time_v1 *out) {
  io();assert(ctx==leases&&kind_live(K_RTC)&&rtc_writes);++rtc_reads;*out=calendar;
  if(which("rtc-read-false")){hidden=true;return false;}
  if(which("rtc-mismatch"))out->minute=(uint8_t)((out->minute+3)%60);
  return true;
}
static bool rtc_write(void *ctx,const twatch_rtc_time_v1 *in) {
  io();assert(ctx==leases&&kind_live(K_RTC)&&kind_live(K_CONTROL));
  assert(in->year>=2000&&in->year<=2099&&in->month>=1&&in->month<=12&&in->day>=1&&
    in->day<=calendar_days(in->year,in->month)&&in->hour<24&&in->minute<60&&in->second<60&&
    in->weekday==calendar_weekday(in));++rtc_writes;written=calendar=*in;
  if(which("rtc-write-false")){hidden=true;return false;}return true;
}
static int32_t kv_get(void *ctx,const char *key,void *out,uint32_t capacity,uint32_t *size) {
  io();assert(ctx==leases&&kind_live(K_KV));++kv_reads;
  const uint8_t *data=NULL;uint32_t length=0;*size=0;
  if(!strcmp(key,PORTABLE_RTC_BASIS_KEY)) {
    if(basis_puts&&which("metadata-read-io")){hidden=true;return RISC_KEY_VALUE_IO;}
    if(!basis_puts&&initial_basis_unavailable)return RISC_KEY_VALUE_IO;
    data=basis_record;length=basis_size;
  } else if(!strcmp(key,PORTABLE_TIMEZONE_KEY)) {
    if(initial_zone_unavailable)return RISC_KEY_VALUE_IO;
    data=zone_record;length=zone_size;
  } else if(!strcmp(key,PORTABLE_READER_FLIP_KEY)){data=flip_record;length=flip_size;}
  else if(!strcmp(key,PORTABLE_READER_LANGUAGE_KEY)||!strcmp(key,PORTABLE_TIME_FORMAT_KEY)||
          !strcmp(key,PORTABLE_SLEEP_KEY)||!strcmp(key,PORTABLE_DESK_FACE_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#ifdef TEST_NATIVE_SETTINGS_ALARMS
  else if(!strcmp(key,PORTABLE_ALERT_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#endif
  else assert(!"unexpected preference read");
  if(!length)return RISC_KEY_VALUE_NOT_FOUND;
  *size=length;if(capacity<length)return RISC_KEY_VALUE_BUFFER_SMALL;
  memcpy(out,data,length);
  if(basis_puts&&which("metadata-mismatch")&&!strcmp(key,PORTABLE_RTC_BASIS_KEY))((uint8_t *)out)[11]^=1;
  return RISC_KEY_VALUE_OK;
}
static int32_t kv_put(void *ctx,const char *key,const void *data,uint32_t size) {
  io();assert(ctx==leases&&kind_live(K_KV));
  if(!strcmp(key,PORTABLE_RTC_BASIS_KEY)) {
    assert(!kind_live(K_RTC)&&seeds&&size==12);++basis_puts;
    memcpy(basis_record,data,size);basis_size=size;
    if(which("metadata-write-io")){hidden=true;return RISC_KEY_VALUE_IO;}
  } else if(!strcmp(key,PORTABLE_TIMEZONE_KEY)) {
    assert(size==44);++zone_puts;memcpy(zone_record,data,size);zone_size=size;
  } else if(!strcmp(key,PORTABLE_READER_FLIP_KEY)) {
    assert(size==4);++other_puts;memcpy(flip_record,data,size);flipped=((const uint8_t *)data)[2]!=0;
  } else {++other_puts;assert(!"unexpected preference write");}
  return RISC_KEY_VALUE_OK;
}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),
 .get_info=get_info,.acquire=frame_get,.release=frame_drop,.submit=frame_show,.present_status=frame_state};
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,subscribe,unsubscribe,touch_poll,touch_next,touch_snapshot};
static const risc_input_navigation_api_v1 navigation_api={1,sizeof(navigation_api),NULL,navigation_poll,navigation_foreground,navigation_reset};
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),leases,kv_get,kv_put};
static const risc_realtime_api_v1 native_api={1,sizeof(native_api),leases,native_read};
static const risc_realtime_control_api_v1 control_api={1,sizeof(control_api),leases,native_read,native_seed};
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.context=leases,.read=rtc_read,.write=rtc_write};
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,battery_read};
#ifdef TEST_NATIVE_SETTINGS_ALARMS
static unsigned alarm_steps,alarm_statuses,alarm_refreshes,alarm_acks,alarm_stops;
static alarm_status_v1 alarm_state={.api_version=1,.struct_size=sizeof(alarm_state),.state=ALARM_STATE_READY};
static bool alarm_fault_ready(void){return saw_fields&&sv_page==SV_FIELDS;}
static int32_t alarm_status(void *ctx,alarm_status_v1 *out) {
  (void)ctx;io();++alarm_statuses;
  if(which("alarm-status-retained")&&alarm_fault_ready()){hidden=true;return -9;}
  if(which("alarm-stop-retained")&&in_fini) {
    alarm_state.state=ALARM_STATE_ALERT;alarm_state.occurrence=(alarm_token_v1){1,2,3,4};
  }
  *out=alarm_state;return ALARM_OK;
}
static int32_t alarm_step(void *ctx) {
  (void)ctx;io();assert(display_settled&&!surface.frame&&!frames);++alarm_steps;
  if(which("alarm-step-retained")&&alarm_fault_ready()){hidden=true;return -9;}
  if(which("alarm-refresh-retained")&&alarm_fault_ready()) {
    alarm_state.state=ALARM_STATE_BLOCKED;alarm_state.error=ALARM_STORAGE;alarm_state.snapshot=2;
  }
  if(which("alarm-ack-retained")&&alarm_fault_ready()) {
    alarm_state.state=ALARM_STATE_ALERT;alarm_state.occurrence=(alarm_token_v1){1,2,3,4};
  }
  return ALARM_OK;
}
static int32_t alarm_refresh(void *ctx){(void)ctx;io();assert(which("alarm-refresh-retained"));++alarm_refreshes;hidden=true;return -9;}
static int32_t alarm_ack(void *ctx,const alarm_token_v1 *token) {
  (void)ctx;io();assert(which("alarm-ack-retained")&&!memcmp(token,&alarm_state.occurrence,sizeof(*token)));
  ++alarm_acks;hidden=true;return -9;
}
static int32_t alarm_prepare(void *ctx,alarm_sleep_v1 *out){(void)ctx;(void)out;io();assert(!"unexpected sleep preparation");return ALARM_INVALID;}
static int32_t alarm_stop(void *ctx){(void)ctx;io();assert(which("alarm-stop-retained"));++alarm_stops;hidden=true;return -9;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_prepare,alarm_stop};
#endif
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
  io();assert(in_main&&grant->struct_size==sizeof(*grant));++acquires;
  if(getenv("NATIVE_SETTINGS_TRACE"))fprintf(stderr,"acquire %s\n",name);
  unsigned kind=0;const void *api=NULL;
  if(!strcmp(name,"display.output")){kind=K_DISPLAY;api=&display_api;}
  else if(!strcmp(name,"input.touch.raw")){kind=K_TOUCH;api=&touch_api;}
  else if(!strcmp(name,"input.navigation")){kind=K_NAV;api=&navigation_api;}
  else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);kind=K_KV;api=&kv_api;}
  else if(!strcmp(name,RISC_REALTIME_CAPABILITY)){kind=K_NATIVE;api=&native_api;}
  else if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)){kind=K_CONTROL;api=&control_api;}
  else if(!strcmp(name,TWATCH_RTC_CAPABILITY)){kind=K_RTC;api=&rtc_api;}
#ifdef TEST_NATIVE_SETTINGS_ALARMS
  else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){kind=K_ALARM;api=&alarm_api;}
#endif
  else {assert(!strcmp(name,"board.battery"));kind=K_BATTERY;api=&battery_api;}
  assert(version==(kind==K_RTC?2u:1u));assert(kind==K_KV||!instance);
  if((which("native-absent")&&(kind==K_NATIVE||kind==K_CONTROL))||
     (which("native-control-absent")&&kind==K_CONTROL))return false;
  if(which("rtc-acquire-false")&&kind==K_RTC){hidden=true;return false;}
  if(which("metadata-acquire-false")&&kind==K_KV&&seeds){hidden=true;return false;}
  unsigned slot=1;while(slot<32&&leases[slot].kind)++slot;assert(slot<32);
  leases[slot]=(lease){kind,++generation};*grant=(risc_runtime_capability_v1){sizeof(*grant),slot,generation,api};
  ++live;if(live>high_water)high_water=live;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
  io();assert(grant->slot&&grant->slot<32&&grant->api&&live);
  lease *entry=&leases[grant->slot];assert(entry->kind&&entry->generation==grant->generation);++releases;
  if((which("rtc-release-false")&&entry->kind==K_RTC)||
     (which("metadata-release-false")&&entry->kind==K_KV&&basis_puts)||
     (which("native-release-false")&&entry->kind==K_CONTROL&&basis_puts)) {
    hidden=true;return false;
  }
  entry->kind=0;--live;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.yield_ms=yielding,
 .diagnostic=diagnostic,.request_launch=request_launch,.acquire=acquire,.release=release,.retain_invocation=retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
static void nav(unsigned button){assert(next_at<2000);nav_buttons[next_at]=button;last_at=next_at;next_at+=4;}
static void point(unsigned at,unsigned x,unsigned y){assert(contact_count<256);contacts[contact_count++]=(scripted_contact){at,x,y,1};}
static void tap(unsigned x,unsigned y){point(next_at,x,y);last_at=next_at;next_at+=5;}
static void open_time(bool touch_mode){if(touch_mode)tap(120,150);else {nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);}}
static void save_time(bool touch_mode){if(touch_mode)tap(SAVE_X,FOOTER_Y);else {nav(RISC_NAV_UP);nav(RISC_NAV_UP);nav(RISC_NAV_CONFIRM);}}
static void end_app(void){nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);}
static void configure_records(void) {
  memset(zone_record,0,sizeof(zone_record));zone_record[0]='T';zone_record[1]='Z';zone_record[2]=1;
  assert(strlen(zone_id)<40);strcpy((char *)zone_record+4,zone_id);zone_record[3]=0xa5;
  for(unsigned i=0;i<44;++i)if(i!=3)zone_record[3]^=zone_record[i];
  bool utc=which("save-utc")||which("fold-second-utc")||which("unset-utc");
  basis_record[0]='R';basis_record[1]='T';basis_record[2]=1;basis_record[3]=utc;
  portable_rtc_basis_put_word(basis_record+4,1767225600);
  portable_rtc_basis_put_word(basis_record+8,portable_rtc_basis_check(basis_record));
  if(initial_basis_missing)basis_size=0;
  if(initial_basis_invalid)basis_record[11]^=1;
  if(initial_zone_missing)zone_size=0;
  if(initial_zone_invalid)zone_record[3]^=1;
  memcpy(flip_record,(uint8_t[]){0x52,1,(uint8_t)flipped,(uint8_t)((unsigned)flipped^0xa5)},4);
}
static void plan_inputs(void) {
  if(which("open-only")||which("native-context")||which("native-read-io")||which("native-absent")){end_app();return;}
  if(which("timezone")) {
    nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);
    /* UTC starts at the UTC region. Move to Africa, then its first city. */
    nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);end_app();return;
  }
  if(which("timezone-then-save")) {
    nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);
    unsigned city=0;int target=portable_timezone_find("America/Denver",sizeof("America/Denver"));assert(target>=0);
    const portable_timezone_entry *entry=portable_timezone_get((unsigned)target);
    for(unsigned i=0;i<entry->region;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);
    while(portable_timezone_city_index(entry->region,city)!=target){assert(city<419);++city;}
    for(unsigned i=0;i<city;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_UP);nav(RISC_NAV_CONFIRM);
    save_time(false);end_app();return;
  }
  if(which("flip-editor")) {
    for(unsigned i=0;i<=SETTINGS_FLIP_ROW;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_RIGHT);nav(RISC_NAV_CONFIRM);end_app();return;
  }
  bool touch_mode=which("save-touch")||which("save-touch-flip")||which("cancel-touch")||
    which("held-touch")||which("drag-save")||which("touch-gap")||which("touch-failed-poll")||which("touch-replaced");
  open_time(touch_mode);
  if(!strncmp(test_name,"alarm-",6)) {
    nav(which("alarm-refresh-retained")?RISC_NAV_CONFIRM:RISC_NAV_BACK);end_app();return;
  }
  if(which("cancel-touch")){tap(100,FOOTER_Y);end_app();return;}
  if(which("back")){nav(RISC_NAV_BACK);end_app();return;}
  if(which("home")){nav(RISC_NAV_HOME);return;}
  if(which("value-back")||which("value-home")||which("value-edit")) {
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_RIGHT);
    if(which("value-home")){nav(RISC_NAV_HOME);return;}
    nav(RISC_NAV_BACK);
    if(which("value-back")){nav(RISC_NAV_BACK);end_app();return;}
  }
  if(which("gap")) { /* 01:30 immediately before Denver spring-forward -> 02:30. */
    for(unsigned i=0;i<3;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_RIGHT);nav(RISC_NAV_BACK);
    for(unsigned i=0;i<3;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_BACK);end_app();return;
  }
  if(which("range")) {
    for(unsigned i=0;i<5;++i)nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_RIGHT);nav(RISC_NAV_BACK);nav(RISC_NAV_DOWN);
    nav(RISC_NAV_CONFIRM);nav(RISC_NAV_BACK);end_app();return;
  }
  if(which("held-entry")) {
    for(unsigned i=1;i<12;++i)nav_buttons[last_at+i]=RISC_NAV_CONFIRM;
    next_at=last_at+16;nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);end_app();return;
  }
  if(which("drag-save")||which("touch-gap")||which("touch-failed-poll")||which("touch-replaced")) {
    point(next_at,SAVE_X,150);point(next_at+1,SAVE_X,FOOTER_Y);point(next_at+2,SAVE_X,FOOTER_Y);
    if(which("touch-gap"))gap_poll=next_at+1;
    if(which("touch-failed-poll"))fail_poll=next_at+1;
    if(which("touch-replaced"))replaced_poll=next_at+1;
    next_at+=8;nav(RISC_NAV_BACK);end_app();return;
  }
  if(which("held-touch")) {
    for(unsigned i=0;i<12;++i)point(next_at+i,SAVE_X,FOOTER_Y);
    next_at+=16;end_app();return;
  }
  save_time(touch_mode);
  if(which("held-save")) {
    for(unsigned i=1;i<12;++i)nav_buttons[last_at+i]=RISC_NAV_CONFIRM;
    next_at=last_at+16;
  }
  if(which("fold-first")||which("fold-second")||which("fold-second-utc")||which("fold-held")||
     which("fold-back")||which("fold-home")) {
    if(which("fold-held")) {
      for(unsigned i=1;i<12;++i)nav_buttons[last_at+i]=RISC_NAV_CONFIRM;
      next_at=last_at+16;nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);
    } else if(which("fold-back")){nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);}
    else if(which("fold-home")){nav(RISC_NAV_HOME);return;}
    else {if(which("fold-second")||which("fold-second-utc"))nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);}
  }
  if(which("native-retry"))nav(RISC_NAV_CONFIRM);
  end_app();
}
static bool no_save_case(void) {
  return !strncmp(test_name,"alarm-",6)||which("open-only")||which("cancel-touch")||which("back")||which("home")||which("value-back")||
   which("value-home")||which("held-entry")||which("fold-held")||which("fold-back")||which("fold-home")||
   which("gap")||which("range")||which("drag-save")||which("touch-gap")||which("touch-failed-poll")||
   which("touch-replaced")||which("bad-basis")||which("unavailable-basis")||which("native-context")||
   which("native-read-io")||which("native-absent")||which("native-control-absent")||which("timezone")||
   which("flip-editor")||which("rtc-acquire-false");
}
int main(int argc,char **argv) {
  assert(argc==2);test_name=argv[1];zone_id=getenv("NATIVE_SETTINGS_ZONE");if(!zone_id)zone_id="America/Denver";
  native_epoch=strtoll(getenv("NATIVE_SETTINGS_EPOCH"),NULL,10);
  expected_epoch=strtoll(getenv("NATIVE_SETTINGS_EXPECTED_EPOCH"),NULL,10);
  initial_basis_missing=which("missing-basis");initial_basis_invalid=which("bad-basis");
  initial_basis_unavailable=which("unavailable-basis");initial_zone_missing=which("missing-zone");
  initial_zone_invalid=which("bad-zone");initial_zone_unavailable=which("unavailable-zone");
  flipped=which("save-touch-flip");native_valid=!which("unset-local")&&!which("unset-utc");
  configure_records();plan_inputs();
  if(which("init-short")||which("init-no-retain")) {
    if(which("init-short"))runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
    else runtime.retain_invocation=NULL;
    assert(app_module_init()==-1);app_module_fini();assert(!provider_calls&&!live&&!barriers);
    printf("{\"case\":\"%s\",\"loader_refused\":true,\"provider_calls\":0}\n",test_name);return 0;
  }
  assert(app_module_init()==0);assert(!provider_calls&&!live&&!seeds&&!rtc_reads&&!kv_reads);
  assert(!t5_app_get_api(0)&&!provider_calls);
  if(which("init-only")) {
    app_module_fini();assert(!provider_calls&&!live&&!barriers);
    printf("{\"case\":\"%s\",\"loader_only\":true,\"provider_calls\":0}\n",test_name);return 0;
  }
  in_main=true;app_main();in_main=false;
  if(getenv("NATIVE_SETTINGS_TRACE"))fprintf(stderr,"done polls %u calls %u failed %d retained %d writes %u seeds %u fields %d status %s editor %s\n",polls,provider_calls,failed,retained,rtc_writes,seeds,saw_fields,snt_status_text,editor_message);
  snprintf(final_status,sizeof(final_status),"%s",snt_status_text);
  if(!retained){assert(!hidden);in_fini=true;app_module_fini();in_fini=false;}
  if(retained) {
    unsigned count=provider_calls,held=live,shown=presents;
    assert(barriers==1&&native_custody_retained);
    settings_view_redraw();sv_switch(SV_ROOT);app_module_fini();
    assert(provider_calls==count&&live==held&&presents==shown&&!launches);
  } else {
    assert(!live&&!subscriptions&&!frames&&!barriers);
  }
  if(no_save_case())assert(!rtc_writes&&!seeds&&!basis_puts);
  else if(which("bad-zone")||which("unavailable-zone"))assert(!rtc_writes&&!seeds&&!basis_puts);
  else assert(rtc_writes==(which("native-retry")?2u:1u));
  if(which("save-local")||which("save-utc")||which("save-touch")||which("save-touch-flip")||
     which("held-save")||which("held-touch")||which("value-edit")||which("fold-first")||
     which("fold-second")||which("fold-second-utc")||which("missing-zone")||which("native-retry")||which("timezone-then-save"))
    assert(!retained&&basis_puts==1&&seeds==(which("native-retry")?2u:1u)&&
      snt_last_result.stage==PORTABLE_SET_TIME_DONE&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED);
  if(basis_puts) {
    assert(seeds&&snt_last_result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED);
    assert(snt_last_result.native.verified&&seed_epoch==expected_epoch);
    assert(portable_rtc_basis_word(basis_record+4)==(uint32_t)expected_epoch);
    bool utc=which("save-utc")||which("fold-second-utc")||which("unset-utc");
    assert((basis_record[3]!=0)==utc);
    portable_timezone_civil actual={written.year,written.month,written.day,written.hour,written.minute,written.second,written.weekday};
    int64_t calendar_epoch;assert(!portable_timezone_civil_to_epoch(&actual,&calendar_epoch));
    const char *expected_calendar=getenv("NATIVE_SETTINGS_EXPECTED_CALENDAR");
    assert(expected_calendar&&calendar_epoch==strtoll(expected_calendar,NULL,10));
  }
  if(which("rtc-write-false")||which("rtc-read-false"))assert(retained&&snt_last_result.rtc_outcome==PORTABLE_SET_TIME_UNCONFIRMED&&!seeds&&!basis_puts);
  if(which("rtc-mismatch"))assert(!retained&&snt_last_result.rtc_outcome==PORTABLE_SET_TIME_UNCONFIRMED&&!seeds&&!basis_puts);
  if(which("native-seed-io")||which("native-readback-io")||which("native-readback-mismatch"))
    assert(!retained&&snt_last_result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED&&seeds==1&&!basis_puts&&!snt_last_result.native.verified);
  if(which("metadata-write-io")||which("metadata-read-io"))assert(retained&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED);
  if(which("metadata-mismatch"))assert(!retained&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED);
  if(which("rtc-release-false"))assert(snt_last_result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED&&!seeds&&!basis_puts);
  if(which("metadata-acquire-false"))assert(snt_last_result.native.verified&&!basis_puts);
  if(which("metadata-release-false")||which("native-release-false"))assert(snt_last_result.native.verified&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED);
  if(which("rtc-acquire-false")||which("rtc-release-false")||which("metadata-acquire-false")||
     which("metadata-release-false")||which("native-release-false")||which("native-context")||which("native-seed-context"))assert(retained);
  if(which("missing-basis"))assert(saw_default_basis&&strstr(first_basis,"default")&&basis_puts==1&&!basis_record[3]);
  if(which("bad-basis"))assert(saw_fields&&strstr(first_basis,"blocked"));
  if(which("unavailable-basis")||which("unavailable-zone"))assert(retained);
  if(which("unset-local")||which("unset-utc"))assert(saw_snapshot_unset&&saw_fields&&first_draft.year==2000&&basis_puts==1);
  if(which("fold-first")||which("fold-second")||which("fold-second-utc")||which("fold-held")||which("fold-back")||which("fold-home"))assert(saw_fold);
  if(which("home")||which("value-home")||which("fold-home"))assert(launches==1);
  else assert(!launches);
  if(which("timezone-then-save"))assert(!strcmp((char *)zone_record+4,"America/Denver")&&first_draft.hour==12&&first_draft.minute==34);
  assert(zone_puts==((which("timezone")||which("timezone-then-save"))?1u:0u));assert(other_puts==(which("flip-editor")?1u:0u));
  assert(!rtc_reads||rtc_writes); /* Native UNSET and opening never bootstrap RTC. */
#ifdef TEST_NATIVE_SETTINGS_ALARMS
  if(!strncmp(test_name,"alarm-",6)) {
    assert(retained&&barriers==1&&native_sleep_retained);
    assert(alarm_acks==(which("alarm-ack-retained")?1u:0u));
    assert(alarm_refreshes==(which("alarm-refresh-retained")?1u:0u));
    assert(alarm_stops==(which("alarm-stop-retained")?1u:0u));
  } else assert(!alarm_acks&&!alarm_refreshes&&!alarm_stops);
#endif
  printf("{\"case\":\"%s\",\"rtc_writes\":%u,\"native_seeds\":%u,\"basis_puts\":%u,\"zone_puts\":%u,\"retained\":%s,\"native_reads\":%u,\"frames\":%u,\"grant_high_water\":%u,\"status\":\"%s\"}\n",
    test_name,rtc_writes,seeds,basis_puts,zone_puts,retained?"true":"false",native_reads,presents,high_water,final_status);
  return 0;
}
