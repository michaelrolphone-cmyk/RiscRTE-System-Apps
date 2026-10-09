/* Native entry around unchanged Apps/settings.c, production adapter/views and
 * checked Set Time helpers.
 * Only the native providers are doubles. Settings interactions enter app_main;
 * no test calls editor_save, settings_activate or mutates the editor draft.
 * Optional Quick clock/action checks run directly before invocation fini;
 * these checks do not qualify the Quick gesture controller. */
#define PORTABLE_SETTINGS_APP
#define PORTABLE_SETTINGS_NATIVE_TIME
#define PORTABLE_SETTINGS_TIME_ZONE
#define PORTABLE_SETTINGS_X4_DESK_CLOCK
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#endif
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_INPUT_NAVIGATION
#define PORTABLE_DISPLAY_ROTATION 90
#define PORTABLE_HOME_APP "default.elf"
#define PORTABLE_SETTINGS_VERSION "1.3.7"
#ifdef TEST_NATIVE_SETTINGS_QUICK
#define TEST_NATIVE_SETTINGS_ALARMS
#define PORTABLE_QUICK_ACTIONS
#define PORTABLE_QUICK_RADIOS
#endif
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
enum { K_DISPLAY=1,K_TOUCH,K_NAV,K_KV,K_NATIVE,K_CONTROL,K_RTC,K_BATTERY,K_ALARM,K_WIFI,K_BLE };
typedef struct {unsigned kind,generation;} lease;
static lease leases[32];
static unsigned generation,live,high_water,provider_calls,acquires,releases;
static unsigned ticks,polls,subscriptions,frames,presents,launches,barriers;
static bool async_pending;
static unsigned async_submitted_at,async_edits,async_value_frames,async_completions,async_observed_year;
static uint8_t async_pixels[NATIVE_WIDTH*NATIVE_HEIGHT/8];
/* Milliseconds below are a deterministic provider clock, not wall time or a
 * hardware claim. Each transfer deliberately stays busy for 1000 ms; the
 * touch timeline starts with the first region image submission. */
typedef struct {
  unsigned down_ms,up_ms,detected_ms,dispatch_ms,visible_ms,first;
  bool moving,detected,dispatched,visible;
} timezone_tap;
static timezone_tap timezone_taps[]={
  {100,220,0,0,0,0,false,false,false,false},
  {300,340,0,0,0,0,false,false,false,false},
  {420,460,0,0,0,0,false,false,false,false},
  {540,580,0,0,0,0,false,false,false,false},
  {700,860,0,0,0,0,true,false,false,false},
  {2200,2320,0,0,0,0,false,false,false,false},
  {3600,3720,0,0,0,0,false,false,false,false},
  {5000,5120,0,0,0,0,false,false,false,false},
  {6500,6720,0,0,0,0,false,false,false,false},
  {6900,7060,0,0,0,0,true,false,false,false}
};
static bool timezone_started,timezone_exit;
static unsigned timezone_start,timezone_seen_first,timezone_rendered_first;
static unsigned timezone_rasters,timezone_presents,timezone_dispatches,timezone_busy_dispatches;
static unsigned timezone_samples,timezone_motion_samples,timezone_burst_first,timezone_visible_at;
static unsigned timezone_final_rasters,timezone_final_presents;
static risc_display_rect_v1 timezone_damage;
static uint8_t timezone_prior_pixels[NATIVE_WIDTH*NATIVE_HEIGHT/8];
static unsigned native_reads,seeds,rtc_reads,rtc_writes,kv_reads,basis_puts,zone_puts,other_puts;
static unsigned metadata_failure_calls;
static unsigned nav_buttons[2048],next_at=5,last_at,contact_count;
typedef struct {unsigned at,x,y,id;} scripted_contact;
static scripted_contact contacts[256];
static uint8_t pixels[NATIVE_WIDTH*NATIVE_HEIGHT/8],zone_record[44],basis_record[12],flip_record[4];
static uint32_t zone_size=44,basis_size=12,flip_size=4;
static bool in_main,in_fini,hidden,retained,flipped,native_valid=true;
static bool initial_basis_missing,initial_basis_invalid,initial_basis_unavailable;
static bool initial_zone_missing,initial_zone_invalid,initial_zone_unavailable;
static bool saw_fields,saw_fold,saw_default_basis,saw_snapshot_unset;
static bool saw_zone_unconfirmed;
static unsigned gap_poll,fail_poll,replaced_poll,capture_index;
static int64_t native_epoch,seed_epoch,expected_epoch;
static const char *test_name,*zone_id;
static twatch_rtc_time_v1 calendar,written,first_draft;
static char first_basis[64],final_status[128];
#ifdef TEST_NATIVE_SETTINGS_QUICK
static bool quick_exercising;
static unsigned wifi_disconnects,wifi_statuses,ble_sets,ble_statuses,radio_puts,quick_puts;
static uint8_t ble_value,radio_record[4]={0x51,1,0,0xa5},dnd_value;
#endif
static bool which(const char *s){return !strcmp(test_name,s);}
static bool async_case(void){return !strncmp(test_name,"async-",6);}
static bool timezone_page_case(void){return which("async-timezone-next")||which("sync-timezone-next");}
static bool delayed_present_case(void){return async_case()||(timezone_page_case()&&sv_page==SV_TIMEZONE_REGIONS);}
static void timezone_observe(void) {
  if(!timezone_page_case()||!timezone_started||sv_page!=SV_TIMEZONE_REGIONS)return;
  if(timezone_seen_first!=stz_first) {
    assert(stz_first>timezone_seen_first);
    timezone_seen_first=stz_first;++timezone_dispatches;
    if(async_pending)++timezone_busy_dispatches;
    for(unsigned n=sizeof(timezone_taps)/sizeof(*timezone_taps);n>0;--n) {
      timezone_tap *tap=&timezone_taps[n-1];
      if(!tap->detected||tap->moving)continue;
      assert(!tap->dispatched);tap->dispatched=true;tap->dispatch_ms=ticks-timezone_start;tap->first=stz_first;
      break;
    }
  }
  if(ticks-timezone_start<1000)timezone_burst_first=stz_first;
}
void __real_free(void *pointer);
void __wrap_free(void *pointer){assert(!hidden&&!retained);__real_free(pointer);}
static void io(void) {
  assert((in_main||in_fini)&&!hidden&&!retained);
  ++provider_calls;timezone_observe();
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
    .damage_x_alignment=8,.damage_width_alignment=8};
  if(async_case())out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
  return true;
}
static bool frame_get(void *ctx,uint32_t format,risc_display_surface_v1 *out) {
  (void)ctx;io();assert(!frames&&!async_pending&&format==RISC_DISPLAY_FORMAT_MONO1);frames=1;
  if(timezone_page_case()&&sv_page==SV_TIMEZONE_REGIONS) {
    ++timezone_rasters;
    if(timezone_started)for(unsigned n=0;n<sizeof(timezone_taps)/sizeof(*timezone_taps);++n) {
      unsigned elapsed=ticks-timezone_start;
      assert(elapsed<timezone_taps[n].down_ms||elapsed>=timezone_taps[n].up_ms);
    }
  }
  *out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=NATIVE_WIDTH,.height=NATIVE_HEIGHT,
    .stride_bytes=NATIVE_WIDTH/8,.size_bytes=sizeof(pixels),.pixel_format=format};return true;
}
static void frame_drop(void *ctx,risc_display_frame_v1 frame){(void)ctx;io();assert(frame==1&&frames);frames=0;}
static bool frame_show(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,
 size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
  (void)ctx;(void)damage;(void)count;(void)options;io();assert(frame==1&&frames);
  frames=0;*token=++presents;
  if(delayed_present_case()) {
    assert(!async_pending);
    if(!timezone_page_case()) {
      if(presents==2)assert(sv_page==SV_VALUE&&options->intent==RISC_DISPLAY_PRESENT_CLEAN);
      if(presents==3)assert(sv_page==SV_ROOT&&options->intent==RISC_DISPLAY_PRESENT_CLEAN);
      if(sv_page==SV_VALUE) {
        assert(settings_draft.year==2029&&async_edits==5&&!nav_buttons[polls]);
        ++async_value_frames;
      }
    }
    async_pending=true;async_submitted_at=ticks;
    memcpy(async_pixels,pixels,sizeof(pixels));
  }
#if defined(ALARM_SERVICE_TAGGED_V2) && defined(TEST_NATIVE_SETTINGS_ALARMS)
  if(alarms.api)assert(settings_visual_alerts());
#endif
  if(sv_page==SV_FIELDS&&!saw_fields){first_draft=settings_draft;saw_fields=true;snprintf(first_basis,sizeof(first_basis),"%s",snt_basis_label());}
  if(sv_page==SV_FOLD)saw_fold=true;
  if(snt_basis_status==PORTABLE_RTC_BASIS_MISSING)saw_default_basis=true;
  if(snt_snapshot_status==PORTABLE_REALTIME_UNSET)saw_snapshot_unset=true;
  if(stz_unconfirmed)saw_zone_unconfirmed=true;
  if(timezone_page_case()&&sv_page==SV_TIMEZONE_REGIONS) {
    if(!timezone_started){timezone_started=true;timezone_start=ticks;timezone_seen_first=stz_first;}
    else {
      assert(count==1&&options->intent==RISC_DISPLAY_PRESENT_QUALITY);
      timezone_damage=damage[0];
      assert(damage[0].x>=0&&damage[0].y>=0&&damage[0].width&&damage[0].height);
      assert(damage[0].x+(int)damage[0].width<=NATIVE_WIDTH&&damage[0].y+(int)damage[0].height<=NATIVE_HEIGHT);
      assert(damage[0].width*damage[0].height<NATIVE_WIDTH*NATIVE_HEIGHT);
      for(unsigned y=0;y<NATIVE_HEIGHT;++y)for(unsigned x=0;x<NATIVE_WIDTH;++x) {
        unsigned at=y*(NATIVE_WIDTH/8)+x/8;
        if((pixels[at]^timezone_prior_pixels[at])&(0x80u>>(x%8)))
          assert(x>=(unsigned)damage[0].x&&x<(unsigned)damage[0].x+damage[0].width&&
                 y>=(unsigned)damage[0].y&&y<(unsigned)damage[0].y+damage[0].height);
      }
    }
    memcpy(timezone_prior_pixels,pixels,sizeof(pixels));
    timezone_rendered_first=stz_first;++timezone_presents;
  }
  capture();return true;
}
static bool frame_state(void *ctx,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
  (void)ctx;io();assert(token);
  if(async_pending) {
    assert(token==presents&&!memcmp(async_pixels,pixels,sizeof(pixels)));
    unsigned elapsed=ticks-async_submitted_at;
    if(elapsed<1000) {
      out->state=elapsed<300?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_ACTIVE;
      return true;
    }
    async_pending=false;++async_completions;
    if(timezone_page_case()&&sv_page==SV_TIMEZONE_REGIONS) {
      if(timezone_rendered_first&&!timezone_visible_at)timezone_visible_at=ticks-timezone_start;
      for(unsigned n=0;n<sizeof(timezone_taps)/sizeof(*timezone_taps);++n) {
        timezone_tap *tap=&timezone_taps[n];
        if(tap->dispatched&&!tap->visible&&tap->first==timezone_rendered_first) {
          tap->visible=true;tap->visible_ms=ticks-timezone_start;
        }
      }
    }
  }
  out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool battery_read(void *ctx,risc_battery_sample_v1 *out){(void)ctx;io();*out=(risc_battery_sample_v1){3800,80,0};return true;}
static uint64_t subscribe(void *ctx){(void)ctx;io();assert(!subscriptions);subscriptions=1;return 1;}
static bool unsubscribe(void *ctx,uint64_t token){(void)ctx;io();assert(token==1&&subscriptions);subscriptions=0;return true;}
static bool touch_poll(void *ctx,size_t limit){
  (void)ctx;io();assert(limit==1);++polls;
  if(async_case()&&sv_page==SV_VALUE&&async_pending) {
    if(async_observed_year&&async_observed_year!=settings_draft.year) {
      assert(presents==1&&sp_dirty);++async_edits;
    }
    async_observed_year=settings_draft.year;
  }
  return polls!=fail_poll;
}
static int32_t touch_next(void *ctx,uint64_t token,risc_touch_event_v1 *out){(void)ctx;(void)token;(void)out;io();return polls==gap_poll?-1:0;}
static bool touch_snapshot(void *ctx,risc_touch_snapshot_v1 *out) {
  (void)ctx;io();*out=(risc_touch_snapshot_v1){.width=LOGICAL_WIDTH,.height=LOGICAL_HEIGHT};
  if(timezone_page_case()&&timezone_started) {
    unsigned elapsed=ticks-timezone_start;
    for(unsigned n=0;n<sizeof(timezone_taps)/sizeof(*timezone_taps);++n) {
      timezone_tap *tap=&timezone_taps[n];
      if(elapsed>=tap->up_ms&&!tap->detected){tap->detected=true;tap->detected_ms=elapsed;}
      if(elapsed<tap->down_ms||elapsed>=tap->up_ms)continue;
      unsigned x=LOGICAL_WIDTH-80,y=LOGICAL_HEIGHT-164;
      if(tap->moving&&elapsed>=tap->down_ms+40){if(n==9)y-=60;else x-=20;}
      out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};
      ++timezone_samples;if(tap->moving)++timezone_motion_samples;
    }
    return true;
  }
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
  out->buttons=out->pressed=nav_buttons[polls];
  if(timezone_page_case()&&timezone_started) {
    out->buttons=out->pressed=0;
    if(ticks-timezone_start>=7500&&!timezone_exit) {
      timezone_exit=true;timezone_final_rasters=timezone_rasters;timezone_final_presents=timezone_presents;
      out->buttons=out->pressed=RISC_NAV_HOME;
    }
  }
  return true;
}
static bool navigation_reset(void *ctx){(void)ctx;io();return true;}
static bool navigation_foreground(void *ctx,const risc_input_foreground_v1 *list,size_t n) {
  (void)ctx;io();if(n)assert(n==1&&list&&!strcmp(list[0].capability,"input.touch.raw"));else assert(!list);return true;
}
static int32_t native_read(void *ctx,risc_realtime_snapshot_v1 *out) {
  io();assert(ctx==leases&&(kind_live(K_NATIVE)||kind_live(K_CONTROL)));++native_reads;
  if(which("native-context")&&!seeds){hidden=true;return RISC_REALTIME_CONTEXT;}
  if(which("async-retained")&&async_pending){hidden=true;return RISC_REALTIME_CONTEXT;}
#ifdef TEST_NATIVE_SETTINGS_QUICK
  if(which("quick-time-context")&&quick_exercising){hidden=true;return RISC_REALTIME_CONTEXT;}
#endif
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
    if(basis_puts&&(which("metadata-read-io")||(which("metadata-read-retry")&&basis_puts==1))) {
      metadata_failure_calls=provider_calls;return RISC_KEY_VALUE_IO;
    }
    if(basis_puts&&which("metadata-read-context")){hidden=true;return RISC_KEY_VALUE_CONTEXT;}
    if(which("basis-context")){hidden=true;return RISC_KEY_VALUE_CONTEXT;}
    if(!basis_puts&&initial_basis_unavailable)return RISC_KEY_VALUE_IO;
    data=basis_record;length=basis_size;
  } else if(!strcmp(key,PORTABLE_TIMEZONE_KEY)) {
    if(which("zone-context")){hidden=true;return RISC_KEY_VALUE_CONTEXT;}
    if(initial_zone_unavailable)return RISC_KEY_VALUE_IO;
    data=zone_record;length=zone_size;
  } else if(!strcmp(key,PORTABLE_READER_FLIP_KEY)){data=flip_record;length=flip_size;}
  else if(!strcmp(key,PORTABLE_READER_LANGUAGE_KEY)||!strcmp(key,PORTABLE_TIME_FORMAT_KEY)||
          !strcmp(key,PORTABLE_SLEEP_KEY)||!strcmp(key,PORTABLE_DESK_FACE_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#ifdef TEST_NATIVE_SETTINGS_ALARMS
  else if(!strcmp(key,PORTABLE_ALERT_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#endif
#ifdef TEST_NATIVE_SETTINGS_QUICK
  else if(!strcmp(key,PORTABLE_RADIO_KEY)){data=radio_record;length=4;}
  else if(!strcmp(key,PQA_DND_KEY)&&quick_puts){data=&dnd_value;length=1;}
  else if(!strcmp(key,PQA_BRIGHTNESS_KEY)||!strcmp(key,PQA_VOLUME_KEY)||
          !strcmp(key,PQA_RESTORE_VOLUME_KEY)||!strcmp(key,PQA_DND_KEY))return RISC_KEY_VALUE_NOT_FOUND;
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
    if(which("metadata-write-context")){hidden=true;return RISC_KEY_VALUE_CONTEXT;}
    if(which("metadata-before-io")||(which("metadata-retry-before")&&basis_puts==1)) {
      metadata_failure_calls=provider_calls;return RISC_KEY_VALUE_IO;
    }
    memcpy(basis_record,data,size);basis_size=size;
    if(which("metadata-write-io")||which("metadata-held-save")||(which("metadata-retry-after")&&basis_puts==1)) {
      metadata_failure_calls=provider_calls;return RISC_KEY_VALUE_IO;
    }
  } else if(!strcmp(key,PORTABLE_TIMEZONE_KEY)) {
    assert(size==44);++zone_puts;
    bool before=which("timezone-io-before")||which("timezone-retry-before");
    bool after=which("timezone-io-after")||which("timezone-retry-after");
    if(before&&zone_puts==1)return RISC_KEY_VALUE_IO;
    memcpy(zone_record,data,size);zone_size=size;
    if(after&&zone_puts==1)return RISC_KEY_VALUE_IO;
  } else if(!strcmp(key,PORTABLE_READER_FLIP_KEY)) {
    assert(size==4);++other_puts;memcpy(flip_record,data,size);flipped=((const uint8_t *)data)[2]!=0;
#ifdef TEST_NATIVE_SETTINGS_QUICK
  } else if(!strcmp(key,PORTABLE_RADIO_KEY)) {
    assert(size==4);++radio_puts;memcpy(radio_record,data,size);
  } else if(!strcmp(key,PQA_DND_KEY)) {
    assert(size==1);++quick_puts;dnd_value=*(const uint8_t *)data;
#endif
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
static int32_t fixture_alarm_refresh(void *ctx){(void)ctx;io();assert(which("alarm-refresh-retained")||which("quick-refresh-retained"));++alarm_refreshes;hidden=true;return -9;}
static int32_t alarm_ack(void *ctx,const alarm_token_v1 *token) {
  (void)ctx;io();assert(which("alarm-ack-retained")&&!memcmp(token,&alarm_state.occurrence,sizeof(*token)));
  ++alarm_acks;hidden=true;return -9;
}
static int32_t alarm_prepare(void *ctx,alarm_sleep_v1 *out){(void)ctx;(void)out;io();assert(!"unexpected sleep preparation");return ALARM_INVALID;}
static int32_t alarm_stop(void *ctx){(void)ctx;io();assert(which("alarm-stop-retained"));++alarm_stops;hidden=true;return -9;}
#ifdef ALARM_SERVICE_TAGGED_V2
static const alarm_service_descriptor_v2 alarm_api={
  {ALARM_SERVICE_API_V2,sizeof(alarm_api),NULL,alarm_status,alarm_step,fixture_alarm_refresh,alarm_ack,alarm_prepare,alarm_stop},
  ALARM_SERVICE_DESCRIPTOR_TAG,ALARM_SERVICE_DESCRIPTOR_VERSION,ALARM_MODE_VISUAL,0,NULL};
#else
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,alarm_status,alarm_step,fixture_alarm_refresh,alarm_ack,alarm_prepare,alarm_stop};
#endif
#endif
#ifdef TEST_NATIVE_SETTINGS_QUICK
static bool wifi_disconnect(void *ctx) {
  (void)ctx;io();++wifi_disconnects;
  if(quick_exercising&&which("quick-wifi-disconnect")){hidden=true;return false;}return true;
}
static wifi_link_t wifi_status(void *ctx) {
  (void)ctx;io();++wifi_statuses;
  if(quick_exercising&&which("quick-wifi-status")){hidden=true;return WIFI_LINK_JOINING;}return WIFI_LINK_DOWN;
}
static bool ble_set(void *ctx,bool enabled) {
  (void)ctx;io();++ble_sets;
  if(quick_exercising&&which("quick-ble-set")){hidden=true;return false;}
  ble_value=enabled?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;
}
static bool ble_status(void *ctx,uint8_t *out) {
  (void)ctx;io();++ble_statuses;
  if(quick_exercising&&which("quick-ble-status")){hidden=true;*out=PORTABLE_BLUETOOTH_RETAINED;return false;}
  *out=ble_value;return true;
}
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=wifi_status,.disconnect_checked=wifi_disconnect};
static const portable_bluetooth_control_v1 ble_api={.api_version=1,.struct_size=sizeof(ble_api),.set_enabled=ble_set,.status=ble_status};
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
#ifdef TEST_NATIVE_SETTINGS_QUICK
  else if(!strcmp(name,"net.wifi")){kind=K_WIFI;api=&wifi_api;assert(instance==15);}
  else if(!strcmp(name,"bluetooth.hci")){kind=K_BLE;api=&ble_api;assert(instance==16);}
#endif
  else {assert(!strcmp(name,"board.battery"));kind=K_BATTERY;api=&battery_api;}
#ifdef ALARM_SERVICE_TAGGED_V2
  assert(version==((kind==K_RTC||kind==K_ALARM)?2u:1u));
#else
  assert(version==(kind==K_RTC?2u:1u));
#endif
assert(kind==K_KV||kind==K_WIFI||kind==K_BLE||!instance);
#ifdef TEST_NATIVE_SETTINGS_QUICK
  if(quick_exercising&&which("quick-later-acquire")&&kind==K_BLE){hidden=true;return false;}
#endif
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
#ifdef TEST_NATIVE_SETTINGS_QUICK
  if(quick_exercising&&which("quick-release-false")&&entry->kind==K_KV){hidden=true;return false;}
#endif
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
  if(timezone_page_case()){nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);return;}
  if(async_case()) {
    if(which("async-exit")){nav(RISC_NAV_BACK);return;}
    if(which("async-retained")){open_time(false);return;}
    bool touch_mode=which("async-touch");open_time(touch_mode);
    if(touch_mode) {
      tap(120,150);tap(LOGICAL_WIDTH-80,280);tap(LOGICAL_WIDTH-80,280);
      tap(80,280);tap(LOGICAL_WIDTH-80,280);tap(LOGICAL_WIDTH-80,280);
    } else {
      nav(RISC_NAV_CONFIRM);nav(RISC_NAV_RIGHT);nav(RISC_NAV_RIGHT);
      nav(RISC_NAV_LEFT);nav(RISC_NAV_RIGHT);nav(RISC_NAV_RIGHT);
    }
    /* Keep the editor neutral across completion, then change pages during
     * its transfer and let a second neutral interval show the latest root. */
    next_at=65;nav(RISC_NAV_BACK);nav(RISC_NAV_BACK);
    next_at=115;end_app();return;
  }
  if(!strncmp(test_name,"quick-",6)){end_app();return;}
  if(which("open-only")||which("native-context")||which("native-read-io")||which("native-absent")){end_app();return;}
  if(which("timezone")||which("timezone-io-before")||which("timezone-io-after")||
     which("timezone-retry-before")||which("timezone-retry-after")) {
    nav(RISC_NAV_DOWN);nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);
    /* UTC starts at the UTC region. Move to Africa, then its first city. */
    nav(RISC_NAV_DOWN);nav(RISC_NAV_CONFIRM);nav(RISC_NAV_CONFIRM);
    if(which("timezone-retry-before")||which("timezone-retry-after"))nav(RISC_NAV_CONFIRM);
    end_app();nav(RISC_NAV_BACK);return;
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
  if(which("held-save")||which("metadata-held-save")) {
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
  if(which("native-retry")||which("metadata-retry-before")||which("metadata-retry-after")||which("metadata-read-retry"))nav(RISC_NAV_CONFIRM);
  end_app();
}
static bool no_save_case(void) {
  return timezone_page_case()||async_case()||!strncmp(test_name,"alarm-",6)||!strncmp(test_name,"quick-",6)||which("open-only")||which("cancel-touch")||which("back")||which("home")||which("value-back")||
   which("value-home")||which("held-entry")||which("fold-held")||which("fold-back")||which("fold-home")||
   which("gap")||which("range")||which("drag-save")||which("touch-gap")||which("touch-failed-poll")||
   which("touch-replaced")||which("bad-basis")||which("unavailable-basis")||which("native-context")||
   which("native-read-io")||which("native-absent")||which("native-control-absent")||which("timezone")||
   which("timezone-io-before")||which("timezone-io-after")||which("timezone-retry-before")||which("timezone-retry-after")||
   which("basis-context")||which("zone-context")||
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
  in_main=true;app_main();
  if(async_case()&&!retained)assert(!async_pending&&async_completions==presents);
#ifdef TEST_NATIVE_SETTINGS_QUICK
  if(!strncmp(test_name,"quick-",6)) {
    assert(!retained&&quick.loaded&&quick.ui.radios_valid);
    assert(wifi_disconnects&&wifi_statuses&&ble_sets&&ble_statuses&&!rtc_reads&&!rtc_writes&&!seeds);
    quick_exercising=true;
    if(which("quick-time")||which("quick-time-context")) {
      unsigned before=native_reads;char value[6];quick_time(value);
      assert(native_reads==before+1&&!kind_live(K_RTC));
      if(which("quick-time"))assert(!strcmp(value,"12:34")&&!kind_live(K_CONTROL));
      else assert(retained&&!strcmp(value,"--:--"));
    } else if(which("quick-refresh-retained")) {
      quick.ui.action_dnd=true;assert(!quick_apply(PQA_DND));
    } else if(!which("quick-startup")) {
      assert(!quick_apply(PQA_AIRPLANE));
    }
    if(which("quick-startup")||which("quick-time"))assert(!retained&&!radio_puts&&!quick_puts);
    else assert(retained);
  }
#endif
  in_main=false;
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
  if(timezone_page_case()) {
    unsigned pages=(PORTABLE_TIMEZONE_REGION_COUNT+stz_rows()-1)/stz_rows();
    assert(timezone_exit&&!async_pending&&timezone_dispatches==pages-1);
    assert(timezone_seen_first==(pages-1)*stz_rows());
    assert(timezone_samples>20&&timezone_motion_samples>5);
    assert(timezone_final_rasters==timezone_rasters&&timezone_final_presents==timezone_presents);
    if(getenv("NATIVE_SETTINGS_TRACE"))fprintf(stderr,"timezone rasters %u presents %u visible %u dispatch %u busy %u first %u burst %u\n",timezone_rasters,timezone_presents,timezone_visible_at,timezone_dispatches,timezone_busy_dispatches,timezone_seen_first,timezone_burst_first);
    if(async_case()) {
      assert(timezone_busy_dispatches==pages-1&&timezone_burst_first==(pages-1)*stz_rows());
      assert(timezone_presents==2&&timezone_rasters==2&&timezone_visible_at==2000);
    } else assert(!timezone_busy_dispatches&&!timezone_burst_first&&timezone_presents==pages&&timezone_rasters==pages);
    for(unsigned n=0;n<sizeof(timezone_taps)/sizeof(*timezone_taps);++n) {
      timezone_tap *tap=&timezone_taps[n];assert(tap->detected&&tap->detected_ms-tap->up_ms<=20);
      if(tap->dispatched)assert(tap->dispatch_ms-tap->detected_ms<=20);
      if(tap->moving)assert(!tap->dispatched);
    }
  } else if(async_case()) {
    if(which("async-retained"))assert(retained&&hidden&&async_pending&&presents==1&&!async_completions);
    else if(which("async-exit"))assert(!async_pending&&presents==1&&async_completions==1&&!async_edits);
    else {
      assert(!async_pending&&async_completions==presents&&async_value_frames==1&&async_edits==5);
      assert(settings_draft.year==2029&&presents>=3);
    }
  }
  if(no_save_case())assert(!rtc_writes&&!seeds&&!basis_puts);
  else if(which("bad-zone")||which("unavailable-zone"))assert(!rtc_writes&&!seeds&&!basis_puts);
  else assert(rtc_writes==((which("native-retry")||which("metadata-retry-before")||which("metadata-retry-after")||which("metadata-read-retry"))?2u:1u));
  if(which("save-local")||which("save-utc")||which("save-touch")||which("save-touch-flip")||
     which("held-save")||which("held-touch")||which("value-edit")||which("fold-first")||
     which("fold-second")||which("fold-second-utc")||which("missing-zone")||which("native-retry")||which("timezone-then-save"))
    assert(!retained&&basis_puts==1&&seeds==(which("native-retry")?2u:1u)&&
      snt_last_result.stage==PORTABLE_SET_TIME_DONE&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED);
  if(basis_puts) {
    assert(seeds&&snt_last_result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED);
    assert(snt_last_result.native.verified&&seed_epoch==expected_epoch);
    assert(portable_rtc_basis_word(basis_record+4)==
      (which("metadata-before-io")||which("metadata-write-context")?UINT32_C(1767225600):(uint32_t)expected_epoch));
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
  if(which("metadata-write-io")||which("metadata-read-io")||which("metadata-before-io")||which("metadata-held-save")) {
    /* Typed KV IO is ordinary persistence failure. Preserve independent
     * outcomes and close the checked grant; retry only on a new Save. */
    assert(!retained&&!hidden&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED);
    assert(metadata_failure_calls&&provider_calls>metadata_failure_calls&&basis_puts==1&&seeds==1);
    assert(strstr(final_status,"RTC verified")&&strstr(final_status,"Native verified")&&strstr(final_status,"Basis unconfirmed"));
  }
  if(which("metadata-retry-before")||which("metadata-retry-after")||which("metadata-read-retry"))
    assert(!retained&&rtc_writes==2&&seeds==2&&basis_puts==2&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED);
  if(which("metadata-write-context")||which("metadata-read-context"))assert(retained&&hidden&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED);
  if(which("metadata-mismatch"))assert(!retained&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED);
  if(which("rtc-release-false"))assert(snt_last_result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED&&!seeds&&!basis_puts);
  if(which("metadata-acquire-false"))assert(snt_last_result.native.verified&&!basis_puts);
  if(which("metadata-release-false")||which("native-release-false"))assert(snt_last_result.native.verified&&snt_last_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED);
  if(which("rtc-acquire-false")||which("rtc-release-false")||which("metadata-acquire-false")||
     which("metadata-release-false")||which("native-release-false")||which("native-context")||which("native-seed-context"))assert(retained);
  if(which("missing-basis"))assert(saw_default_basis&&strstr(first_basis,"default")&&basis_puts==1&&!basis_record[3]);
  if(which("bad-basis"))assert(saw_fields&&strstr(first_basis,"blocked"));
  if(which("unavailable-basis"))assert(!retained&&saw_fields&&strstr(first_basis,"Unavailable")&&strstr(first_basis,"blocked"));
  if(which("unavailable-zone"))assert(!retained&&saw_fields&&snt_zone_status==PORTABLE_TIMEZONE_UNAVAILABLE);
  if(which("basis-context")||which("zone-context"))assert(retained&&hidden);
  if(which("unset-local")||which("unset-utc"))assert(saw_snapshot_unset&&saw_fields&&first_draft.year==2000&&basis_puts==1);
  if(which("fold-first")||which("fold-second")||which("fold-second-utc")||which("fold-held")||which("fold-back")||which("fold-home"))assert(saw_fold);
  if(timezone_page_case()||which("home")||which("value-home")||which("fold-home"))assert(launches==1);
  else assert(!launches);
  if(which("timezone-then-save"))assert(!strcmp((char *)zone_record+4,"America/Denver")&&first_draft.hour==12&&first_draft.minute==34);
  if(which("timezone-io-before")||which("timezone-io-after")) {
    assert(!retained&&saw_zone_unconfirmed&&stz_unconfirmed&&zone_puts==1);
    assert((!strcmp((char *)zone_record+4,"UTC"))==which("timezone-io-before"));
  } else if(which("timezone-retry-before")||which("timezone-retry-after")) {
    assert(!retained&&saw_zone_unconfirmed&&!stz_unconfirmed);
    assert(zone_puts==(which("timezone-retry-before")?2u:1u));
    assert(!strcmp((char *)zone_record+4,portable_timezone_get(stz_choice)->id));
  } else assert(zone_puts==((which("timezone")||which("timezone-then-save"))?1u:0u));
  assert(other_puts==(which("flip-editor")?1u:0u));
  assert(!rtc_reads||rtc_writes); /* Native UNSET and opening never bootstrap RTC. */
#ifdef TEST_NATIVE_SETTINGS_ALARMS
  if(!strncmp(test_name,"alarm-",6)) {
    assert(retained&&barriers==1&&native_sleep_retained);
    assert(alarm_acks==(which("alarm-ack-retained")?1u:0u));
    assert(alarm_refreshes==(which("alarm-refresh-retained")?1u:0u));
    assert(alarm_stops==(which("alarm-stop-retained")?1u:0u));
  } else assert(!alarm_acks&&alarm_refreshes==(which("quick-refresh-retained")?1u:0u)&&!alarm_stops);
#endif
  if(timezone_page_case()) {
    printf("{\"case\":\"%s\",\"clock\":\"simulated_ms\",\"transfer_ms\":1000,\"held_or_moving_rasters\":0,\"unchanged_page_rasters\":0,\"region_rasters\":%u,\"region_presents\":%u,\"page_dispatches\":%u,\"busy_page_dispatches\":%u,\"held_and_moving_samples\":%u,\"moving_samples\":%u,\"first_changed_visible_ms\":%u,\"damage\":{\"x\":%d,\"y\":%d,\"width\":%u,\"height\":%u},\"taps\":[",
      test_name,timezone_rasters,timezone_presents,timezone_dispatches,timezone_busy_dispatches,
      timezone_samples,timezone_motion_samples,timezone_visible_at,timezone_damage.x,timezone_damage.y,timezone_damage.width,timezone_damage.height);
    for(unsigned n=0;n<sizeof(timezone_taps)/sizeof(*timezone_taps);++n) {
      timezone_tap *tap=&timezone_taps[n];if(n)printf(",");
      printf("{\"release_ms\":%u,\"detected_ms\":%u,\"moving\":%s,\"dispatch_ms\":",tap->up_ms,tap->detected_ms,tap->moving?"true":"false");
      if(tap->dispatched)printf("%u",tap->dispatch_ms);else printf("null");
      printf(",\"first_visible_ms\":");if(tap->visible)printf("%u",tap->visible_ms);else printf("null");
      printf(",\"coalesced_before_visible\":%s",tap->dispatched&&!tap->visible?"true":"false");
      printf(",\"first_region\":");if(tap->dispatched)printf("%u",tap->first);else printf("null");printf("}");
    }
    printf("]}\n");return 0;
  }
  printf("{\"case\":\"%s\",\"rtc_writes\":%u,\"native_seeds\":%u,\"basis_puts\":%u,\"zone_puts\":%u,\"retained\":%s,\"native_reads\":%u,\"frames\":%u,\"grant_high_water\":%u,\"status\":\"%s\"}\n",
    test_name,rtc_writes,seeds,basis_puts,zone_puts,retained?"true":"false",native_reads,presents,high_water,final_status);
  return 0;
}
