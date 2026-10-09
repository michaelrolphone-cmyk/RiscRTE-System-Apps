/* Settings-only model/controller, reusing Apps/settings.c and the compact view. */
#include <stdio.h>
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#ifndef PORTABLE_SETTINGS_VERSION
#define PORTABLE_SETTINGS_VERSION "dev"
#endif

#ifndef PORTABLE_SETTINGS_NATIVE_TIME
static risc_runtime_capability_v1 rtc_grant;
static const twatch_rtc_api_v1 *rtc;
#endif
static const char *settings_message = "";
static const char *editor_message = "";
static unsigned editor_field;
static risc_runtime_capability_v1 settings_grant;
static const risc_key_value_v1 *settings_store;
#ifdef PORTABLE_SETTINGS_TIME_ZONE
#include "PortableTimeZonePreference.h"
static unsigned stz_choice,stz_region,stz_selected,stz_first;
static bool stz_unconfirmed;
static int stz_status;
static const char *stz_message;
static void stz_redraw(void);
static bool stz_poll(t5_app_input_t *out);
#endif
static unsigned time_format_mode, time_format_choice;
static int time_format_status;
static const char *time_format_message;
static void settings_time_format_load(void) {
  unsigned mode;
  time_format_status=portable_time_format_load(settings_store,&mode);
  /* An unavailable refresh must not replace the last confirmed choice. */
  if(time_format_status!=PORTABLE_TIME_FORMAT_UNAVAILABLE)time_format_mode=mode;
}
static bool settings_hour_24(void) { return time_format_mode==PORTABLE_TIME_FORMAT_24; }
#ifdef PORTABLE_SLEEP_SETTINGS
#include "PortableSleepPolicy.h"
static unsigned sleep_choice;
static const char *sleep_message;
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
#define SETTINGS_SLEEP_MODES (PORTABLE_SLEEP_MASK_LIGHT|PORTABLE_SLEEP_MASK_DEEP)
#define SETTINGS_SLEEP_DEFAULT PORTABLE_SLEEP_LIGHT
static bool sleep_unconfirmed;
#else
#define SETTINGS_SLEEP_MODES PORTABLE_SLEEP_MASK_ALL
#define SETTINGS_SLEEP_DEFAULT PORTABLE_SLEEP_HYBRID
#endif
static int settings_sleep_load(unsigned *mode) {
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  return portable_sleep_load_profile(settings_store,SETTINGS_SLEEP_MODES,SETTINGS_SLEEP_DEFAULT,mode);
#else
  return portable_sleep_load(settings_store,mode);
#endif
}
static unsigned settings_sleep_count(void) {
  unsigned n=0;for(unsigned mode=0;mode<=PORTABLE_SLEEP_HYBRID;++mode)
    if(portable_sleep_supported(SETTINGS_SLEEP_MODES,mode))++n;
  return n;
}
static unsigned settings_sleep_at(unsigned row) {
  for(unsigned mode=0;mode<=PORTABLE_SLEEP_HYBRID;++mode)
    if(portable_sleep_supported(SETTINGS_SLEEP_MODES,mode)){if(!row)return mode;--row;}
  return SETTINGS_SLEEP_DEFAULT;
}
static unsigned settings_sleep_next(unsigned mode) {
  for(unsigned i=0;i<3;++i){mode=(mode+1u)%3u;if(portable_sleep_supported(SETTINGS_SLEEP_MODES,mode))return mode;}
  return SETTINGS_SLEEP_DEFAULT;
}
#define SETTINGS_BASE_ROW_COUNT 6u
#define SETTINGS_ABOUT_ROW 5u
#else
#define SETTINGS_BASE_ROW_COUNT 5u
#define SETTINGS_ABOUT_ROW 4u
#endif
#ifdef PORTABLE_ALARM_SETTINGS
#include "PortableAlarmSettings.h"
#include "AlarmServiceV1.h"
static bool settings_visual_alerts(void) {
#ifdef PORTABLE_ALARM_CLIENT
#ifdef ALARM_SERVICE_TAGGED_V2
  return alarms.api && portable_alarm_output_modes(&alarms)==ALARM_MODE_VISUAL;
#else
  return alarms.api && alarm_service_output_modes(alarms.api)==ALARM_MODE_VISUAL;
#endif
#else
  return false;
#endif
}
static const risc_key_value_v1 *alert_store;
static unsigned alert_choice;
static const char *alert_message;
static bool alert_unconfirmed;
/* Append after About so pre-existing configurations retain every row index. */
#define SETTINGS_ALERT_ROW SETTINGS_BASE_ROW_COUNT
#define SETTINGS_NON_FACE_ROW_COUNT (SETTINGS_BASE_ROW_COUNT+1u)
#else
#define SETTINGS_NON_FACE_ROW_COUNT SETTINGS_BASE_ROW_COUNT
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
#ifndef PORTABLE_SLEEP_SETTINGS
#error "X4 desk-clock Settings requires sleep settings"
#endif
#include "PortableDeskClockSettings.h"
#define SETTINGS_FACE_ROW SETTINGS_NON_FACE_ROW_COUNT
#define SETTINGS_FLIP_ROW (SETTINGS_NON_FACE_ROW_COUNT+1u)
#define SETTINGS_LANGUAGE_ROW (SETTINGS_NON_FACE_ROW_COUNT+2u)
#define SETTINGS_ROW_COUNT (SETTINGS_NON_FACE_ROW_COUNT+3u)
static unsigned reader_choice;
static const char *reader_message;
static bool reader_unconfirmed[2];
static unsigned face_choice;
static const char *face_message;
static bool face_unconfirmed;
#else
#define SETTINGS_ROW_COUNT SETTINGS_NON_FACE_ROW_COUNT
#endif
static twatch_rtc_time_v1 settings_draft;
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
static portable_time_candidate save_candidates[2];
static unsigned save_candidate_count;
#endif
static const char *const editor_names[] = {"Year","Month","Day","Hour","Minute","Second"};
static unsigned calendar_days(unsigned year, unsigned month) {
  static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return month >= 1 && month <= 12 ? days[month - 1] + (month == 2 && year % 4 == 0) : 0;
}
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
static bool calendar_valid(const twatch_rtc_time_v1 *t) {
  return t && t->year >= 2000 && t->year <= 2099 && t->month >= 1 && t->month <= 12 &&
         t->day >= 1 && t->day <= calendar_days(t->year, t->month) && t->weekday <= 6 &&
         t->hour <= 23 && t->minute <= 59 && t->second <= 59;
}
#endif
/* Bounded to 100 years. Unsigned seconds through 2099 fit in uint32_t. */
static uint32_t calendar_day_index(const twatch_rtc_time_v1 *t) {
  uint32_t days = 0;
  for (unsigned y = 2000; y < t->year; ++y) days += 365 + (y % 4 == 0);
  for (unsigned m = 1; m < t->month; ++m) days += calendar_days(t->year, m);
  return days + t->day - 1;
}
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
static uint32_t calendar_seconds(const twatch_rtc_time_v1 *t) {
  return ((calendar_day_index(t) * 24 + t->hour) * 60 + t->minute) * 60 + t->second;
}
#endif
static uint8_t calendar_weekday(const twatch_rtc_time_v1 *t) {
  return (uint8_t)((calendar_day_index(t) + 6) % 7); /* 2000-01-01 was Saturday. */
}
static inline void format_wall_time(char *out, size_t capacity, const twatch_rtc_time_v1 *t) {
  if(settings_hour_24()) {
    snprintf(out,capacity,"%02u:%02u:%02u",(unsigned)t->hour,(unsigned)t->minute,(unsigned)t->second);
    return;
  }
  unsigned hour = t->hour % 12;
  snprintf(out, capacity, "%u:%02u:%02u %s", hour ? hour : 12,
           (unsigned)t->minute, (unsigned)t->second, t->hour < 12 ? "AM" : "PM");
}

#ifdef PORTABLE_SETTINGS_NATIVE_TIME
#include "settings_native_time.inc"
#endif
static void set_back_exits(bool enabled) { back_exits_app=enabled; }
static uint32_t settings_categories(void) { return 1; }
static bool settings_category(uint32_t category,char *out,size_t capacity) {
  if(category || !out || capacity<sizeof("System"))return false;
  memcpy(out,"System",sizeof("System"));return true;
}
static uint32_t settings_count(uint32_t category) { return category?0:SETTINGS_ROW_COUNT; }
static bool settings_get(uint32_t category,uint32_t index,t5_app_setting_t *out) {
  if(category || index>=SETTINGS_ROW_COUNT || !out)return false;
  memset(out,0,sizeof(*out));out->type=T5_APP_SETTING_VALUE;
  static const char *const names[]={"Set Time","Time Zone","Time Format","RTC Basis",
#ifdef PORTABLE_SLEEP_SETTINGS
    "Clock Sleep Mode",
#endif
    "About"
#ifdef PORTABLE_ALARM_SETTINGS
    ,"Alarm Alerts"
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
    ,"Clock face","Flip UI 180°","Language"
#endif
    };
  strcpy(out->label,names[index]);
  if(index==0) {
    out->type=T5_APP_SETTING_ACTION;
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
    twatch_rtc_time_v1 local;
    bool readable=snt_read_local(&local);if(native_custody_retained)return false;
    if(readable) {
#else
    twatch_rtc_time_v1 raw,local;
    if(rtc && rtc->read(rtc->context,&raw) && portable_time_forward(&raw,&local)) {
#endif
      if(settings_hour_24())snprintf(out->value,sizeof(out->value),"%02u:%02u",(unsigned)local.hour,(unsigned)local.minute);
      else {
        unsigned h=local.hour%12;
        snprintf(out->value,sizeof(out->value),"%u:%02u %s",h?h:12,(unsigned)local.minute,local.hour<12?"AM":"PM");
      }
    } else {
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
      strcpy(out->value,snt_snapshot_status==PORTABLE_REALTIME_UNSET?"Native unset":"Unavailable");
#else
      strcpy(out->value,"Unavailable");
#endif
    }
  } else if(index==1) {
#ifdef PORTABLE_SETTINGS_TIME_ZONE
    char id[PORTABLE_TIMEZONE_ID_BYTES];
    int status=portable_timezone_preference_load(settings_store,id);
    out->type=T5_APP_SETTING_ACTION;
    strcpy(out->value,stz_unconfirmed?"Unconfirmed":status==PORTABLE_TIMEZONE_UNAVAILABLE?"Unavailable":
        status==PORTABLE_TIMEZONE_INVALID_RECORD?
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
        "Invalid - choose zone":
#else
        "Invalid (UTC fallback)":
#endif
        status==PORTABLE_TIMEZONE_MISSING?"UTC (default)":id);
#else
    strcpy(out->value,portable_time_zone());
#endif
  }
  else if(index==2) {
    out->type=T5_APP_SETTING_ACTION;
    strcpy(out->value,time_format_status==PORTABLE_TIME_FORMAT_UNAVAILABLE?"Unavailable":settings_hour_24()?"24-hour":"12-hour");
  }
  else if(index==3) {
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
    snt_load_basis();if(native_custody_retained)return false;strcpy(out->value,snt_basis_label());
#else
    strcpy(out->value,portable_time_basis());
#endif
  }
#ifdef PORTABLE_SLEEP_SETTINGS
  else if(index==4) {
    unsigned mode;int loaded=settings_sleep_load(&mode);
    out->type=T5_APP_SETTING_ACTION;
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
    if(sleep_unconfirmed){strcpy(out->value,"Unconfirmed");return true;}
    if(loaded==PORTABLE_SLEEP_UNSUPPORTED){strcpy(out->value,"Light (unsupported saved)");return true;}
    if(loaded==PORTABLE_SLEEP_INVALID){strcpy(out->value,"Light (invalid saved)");return true;}
    if(loaded==PORTABLE_SLEEP_MISSING){strcpy(out->value,"Light (default)");return true;}
    if(loaded==PORTABLE_SLEEP_LOADED&&mode==PORTABLE_SLEEP_DEEP){strcpy(out->value,"Deep Desk Clock");return true;}
#endif
    strcpy(out->value,loaded==PORTABLE_SLEEP_UNAVAILABLE?"Unavailable":mode==PORTABLE_SLEEP_DEEP?"Deep":mode==PORTABLE_SLEEP_LIGHT?"Light":"Light then Deep");
  }
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  else if(index==SETTINGS_ALERT_ROW) {
    if(settings_visual_alerts()){out->type=T5_APP_SETTING_ACTION;strcpy(out->value,"VISUAL ONLY");return true;}
    unsigned mode;int loaded=portable_alert_load(alert_store,&mode);
    out->type=T5_APP_SETTING_ACTION;
    strcpy(out->value,alert_unconfirmed?"Unconfirmed":
        loaded==PORTABLE_ALERT_UNAVAILABLE?"Unavailable":
        loaded==PORTABLE_ALERT_INVALID?"Invalid":portable_alert_name(mode));
  }
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  else if(index==SETTINGS_FACE_ROW) {
    unsigned face;int loaded=portable_desk_face_load(settings_store,&face);
    out->type=T5_APP_SETTING_ACTION;
    strcpy(out->value,face_unconfirmed?"Unconfirmed":loaded==PORTABLE_DESK_FACE_UNAVAILABLE?"Unavailable":
        loaded==PORTABLE_DESK_FACE_INVALID?"Segments (invalid saved)":
        loaded==PORTABLE_DESK_FACE_MISSING?"Segments (default)":portable_desk_face_name(face));
  }
  else if(index==SETTINGS_FLIP_ROW || index==SETTINGS_LANGUAGE_ROW) {
    bool language=index==SETTINGS_LANGUAGE_ROW;unsigned value;
    int loaded=portable_reader_preference_load(settings_store,language,&value);
    out->type=T5_APP_SETTING_ACTION;
    strcpy(out->value,reader_unconfirmed[language]?"Unconfirmed":
      loaded==PORTABLE_READER_UNAVAILABLE?"Unavailable":
      loaded==PORTABLE_READER_INVALID?"Invalid saved value":
      language?portable_reader_language_name(value):value?"On":"Off");
  }
#endif
  else {out->type=T5_APP_SETTING_ACTION;strcpy(out->value,PORTABLE_SETTINGS_VERSION);}
  return true;
}
static unsigned editor_value(unsigned field) {
  return field==0?settings_draft.year:field==1?settings_draft.month:field==2?settings_draft.day:
         field==3?settings_draft.hour:field==4?settings_draft.minute:settings_draft.second;
}
static void editor_bounds(unsigned field,unsigned *lo,unsigned *hi) {
  *lo=field==0?2000:field<3?1:0;
  *hi=field==0?2099:field==1?12:field==2?calendar_days(settings_draft.year,settings_draft.month):field==3?23:59;
}
static void editor_set_value(unsigned value) {
  unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);
  if(value<lo)value=lo;
  if(value>hi)value=hi;
  switch(editor_field) {
  case 0:settings_draft.year=(uint16_t)value;break;
  case 1:settings_draft.month=(uint8_t)value;break;
  case 2:settings_draft.day=(uint8_t)value;break;
  case 3:settings_draft.hour=(uint8_t)value;break;
  case 4:settings_draft.minute=(uint8_t)value;break;
  case 5:settings_draft.second=(uint8_t)value;break;
  }
  if(settings_draft.day>calendar_days(settings_draft.year,settings_draft.month))
    settings_draft.day=(uint8_t)calendar_days(settings_draft.year,settings_draft.month);
  settings_draft.weekday=calendar_weekday(&settings_draft);
}
static void editor_value_text(unsigned field,char *out,size_t capacity) {
  unsigned v=editor_value(field);
  if(field==3 && !settings_hour_24())snprintf(out,capacity,"%u %s",v%12?v%12:12,v<12?"AM":"PM");
  else snprintf(out,capacity,"%u",v);
}
static void editor_step(bool forward) {
  unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);unsigned value=editor_value(editor_field);
#ifdef PORTABLE_NOVA_UI
  if(editor_field==3 && !settings_hour_24()) {editor_set_value((value/12)*12+(value%12+(forward?1:11))%12);return;}
#endif
  editor_set_value(forward?(value<hi?value+1:lo):(value>lo?value-1:hi));
}
static void settings_navigation_poll(t5_app_input_t *out) {input_navigation_take(out);}
#include "settings_view.inc"
static void settings_render(uint32_t category,int32_t selected) {
  if(handoff_requested)return;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(failed)return;
#endif
  (void)category;
  bool changed=sv_selected!=(unsigned)selected;
  sv_selected=selected<0?0:(unsigned)selected;list_mode=true;sv_active=true;
  if(pp_enabled() && changed && selected>0 && selected<=(int32_t)SETTINGS_ROW_COUNT) {
    unsigned row=(unsigned)selected-1,rows=sp_rows();
    if(row<sp_first)sp_first=row;
    else if(row>=sp_first+rows)sp_first=row-rows+1;
  }
  if(changed && selected>0 && selected<=(int32_t)SETTINGS_ROW_COUNT) {
    int y=sv_row_y((unsigned)selected-1)-sv_scroll[0];
    if(y<52)sv_scroll[0]=sv_row_y((unsigned)selected-1)-52;
    else if(y+SV_ROW_HEIGHT>174)sv_scroll[0]=sv_row_y((unsigned)selected-1)-(174-SV_ROW_HEIGHT);
  }
  if(sv_page!=SV_ROOT)sv_switch(SV_ROOT);else settings_view_redraw();
}
/* Exactly one write per explicit Save/choice. Verification stays in raw RTC
 * coordinates and tolerates one ticking second/date boundary within250ms. */
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
static bool editor_save_raw(const twatch_rtc_time_v1 *value) {
  if(!calendar_valid(value)){editor_message="Invalid date or time";return false;}
  uint32_t start=millis_now();if(failed)return false;
  if(!rtc->write(rtc->context,value)){editor_message="Write failed - check RTC";return false;}
  for(unsigned attempt=0;attempt<4;++attempt) {
    twatch_rtc_time_v1 actual;bool read=rtc->read(rtc->context,&actual);
    uint32_t elapsed=millis_now()-start;if(failed || elapsed>250)break;
    if(read && calendar_valid(&actual) && actual.weekday==calendar_weekday(&actual)) {
      uint32_t wanted=calendar_seconds(value),got=calendar_seconds(&actual);
      if(got>=wanted && got-wanted<=elapsed/1000+1)return true;
    }
    if(attempt!=3)rt->yield_ms(20);
  }
  editor_message="Verify failed - check RTC";return false;
}
#endif
static void editor_select_field(unsigned field) {
  editor_field=field;
  if(pp_enabled()&&field<6){unsigned rows=sp_rows();if(field<sp_field_first)sp_field_first=field;else if(field>=sp_field_first+rows)sp_field_first=field-rows+1;}
  if(field<6) {
    int y=60+(int)field*SV_ROW_HEIGHT-sv_scroll[1];
    if(y<60)sv_scroll[1]=(int)field*SV_ROW_HEIGHT;
    else if(y+SV_ROW_HEIGHT>SV_FIELD_BOTTOM)sv_scroll[1]=60+(int)field*SV_ROW_HEIGHT-(SV_FIELD_BOTTOM-SV_ROW_HEIGHT);
    if(sv_scroll[1]<0)sv_scroll[1]=0;
    if(sv_scroll[1]>sv_max_scroll())sv_scroll[1]=sv_max_scroll();
  }
}
static bool editor_try_save(void) {
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  if(snt_basis_status!=PORTABLE_RTC_BASIS_LOADED && snt_basis_status!=PORTABLE_RTC_BASIS_MISSING) {
    editor_message=snt_basis_label();sv_switch(SV_FIELDS);return false;
  }
  if(snt_zone_status!=PORTABLE_TIMEZONE_LOADED && snt_zone_status!=PORTABLE_TIMEZONE_MISSING) {
    editor_message="Choose a valid Time Zone first";sv_switch(SV_FIELDS);return false;
  }
  portable_set_time_request request=snt_request(-1);portable_set_time_plan plan;
  if(portable_set_time_prepare(&request,&plan)==PORTABLE_REALTIME_FOLD) {
    sv_fold_choice=0;sv_switch(SV_FOLD);return false;
  }
  if(snt_save(-1))return true;
  sv_switch(SV_FIELDS);return false;
#else
  save_candidate_count=portable_time_inverse(&settings_draft,save_candidates);
  if(!save_candidate_count) {
    editor_message="DST gap or RTC range";sv_switch(SV_FIELDS);return false;
  }
  if(save_candidate_count==2) {sv_fold_choice=0;sv_switch(SV_FOLD);return false;}
  if(editor_save_raw(&save_candidates[0].rtc))return true;
  sv_switch(SV_FIELDS);return false;
#endif
}
#ifdef PORTABLE_SETTINGS_TIME_ZONE
#include "settings_timezone.inc"
#endif
static uint8_t settings_activate(uint32_t category,uint32_t index) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(failed)return T5_APP_SETTING_ERROR;
#endif
  if(category || index>=SETTINGS_ROW_COUNT)return T5_APP_SETTING_ERROR;
#ifdef PORTABLE_SETTINGS_TIME_ZONE
  if(index==1)return stz_activate();
#endif
  if(index==2) {
    settings_time_format_load();time_format_choice=time_format_mode;
    time_format_message=time_format_status==PORTABLE_TIME_FORMAT_UNAVAILABLE?"Storage unavailable":
                        time_format_status==PORTABLE_TIME_FORMAT_INVALID?"Invalid: 12-hour default":"";
    sv_switch(SV_TIME_FORMAT);bool armed=false;t5_app_input_t input;
    uint8_t result=T5_APP_SETTING_NO_CHANGE;
    while(poll(&input,20)) {
      if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
      if(!input.buttons && !input.tapped){armed=true;continue;}
      if(!armed)continue;
      armed=false;
      bool save=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
      if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT))time_format_choice^=1u;
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,76,184,32))time_format_choice=PORTABLE_TIME_FORMAT_12;
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,124,184,32))time_format_choice=PORTABLE_TIME_FORMAT_24;
      if(save) {
        if(portable_time_format_save(settings_store,time_format_choice)) {
          time_format_mode=time_format_choice;time_format_status=PORTABLE_TIME_FORMAT_LOADED;
          result=T5_APP_SETTING_UPDATED;break;
        }
        time_format_choice=time_format_mode;
        time_format_message="Save unconfirmed - retry";
      }
      settings_view_redraw();
    }
    settings_message=result==T5_APP_SETTING_UPDATED?"Time format saved":time_format_message;
    sv_switch(SV_ROOT);return result;
  }
#ifdef PORTABLE_SLEEP_SETTINGS
  if(index==4) {
    int loaded=settings_sleep_load(&sleep_choice);
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
    sleep_message=sleep_unconfirmed?"Save unconfirmed - retry":
                  loaded==PORTABLE_SLEEP_UNAVAILABLE?"Unavailable: Light default":
                  loaded==PORTABLE_SLEEP_UNSUPPORTED?"Unsupported: Light default":
                  loaded==PORTABLE_SLEEP_INVALID?"Invalid: Light default":
                  loaded==PORTABLE_SLEEP_MISSING?"Missing: Light default":"";
#else
    sleep_message=loaded==PORTABLE_SLEEP_UNAVAILABLE?"Storage unavailable":
                  loaded==PORTABLE_SLEEP_INVALID?"Invalid: Hybrid default":"";
#endif
    sv_switch(SV_SLEEP);bool armed=false;t5_app_input_t input;
    uint8_t result=T5_APP_SETTING_NO_CHANGE;
    while(poll(&input,20)) {
      if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
      if(!input.buttons && !input.tapped){armed=true;continue;}
      if(!armed)continue;
      armed=false;
      bool save=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
      if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT))sleep_choice=settings_sleep_next(sleep_choice);
      else if(input.tapped)for(unsigned row=0;row<settings_sleep_count();++row)
        if(sv_inside(input.touch_x,input.touch_y,28,66+(int)row*38,184,30))sleep_choice=settings_sleep_at(row);
      if(save) {
        if(portable_sleep_save_profile(settings_store,SETTINGS_SLEEP_MODES,sleep_choice)) {
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
          sleep_unconfirmed=false;
#endif
          result=T5_APP_SETTING_UPDATED;break;
        }
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
        sleep_unconfirmed=true;
#endif
        /* A failed commit may or may not have persisted. Re-read rather than
         * claim the old mode or display a false Saved confirmation. */
        (void)settings_sleep_load(&sleep_choice);
        sleep_message="Save unconfirmed - retry";
      }
      settings_view_redraw();
    }
    settings_message=result==T5_APP_SETTING_UPDATED?"Sleep mode saved":
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
      sleep_unconfirmed?"Sleep mode unconfirmed":
#endif
      "";
    sv_switch(SV_ROOT);return result;
  }
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  if(index==SETTINGS_ALERT_ROW) {
    if(!alert_unconfirmed) {
      int loaded=portable_alert_load(alert_store,&alert_choice);
      alert_message=loaded==PORTABLE_ALERT_UNAVAILABLE?"Storage unavailable":
                    loaded==PORTABLE_ALERT_INVALID?"Invalid - choose a mode":"";
    }
    sv_switch(SV_ALERT);bool armed=false;t5_app_input_t input;
    uint8_t result=T5_APP_SETTING_NO_CHANGE;
    while(poll(&input,20)) {
      if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
      if(!input.buttons && !input.tapped){armed=true;continue;}
      if(!armed)continue;
      armed=false;
      if(settings_visual_alerts()){if(input.buttons&T5_APP_BUTTON_CONFIRM)break;continue;}
      bool save=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
      if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))
        alert_choice=alert_choice<PORTABLE_ALERT_BOTH?alert_choice+1u:PORTABLE_ALERT_VIBRATE;
      else if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))
        alert_choice=alert_choice>PORTABLE_ALERT_VIBRATE?alert_choice-1u:PORTABLE_ALERT_BOTH;
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,66,184,30))alert_choice=PORTABLE_ALERT_VIBRATE;
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,104,184,30))alert_choice=PORTABLE_ALERT_SOUND;
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,142,184,30))alert_choice=PORTABLE_ALERT_BOTH;
      if(save) {
        if(alert_choice<PORTABLE_ALERT_VIBRATE || alert_choice>PORTABLE_ALERT_BOTH)
          alert_message="Choose an alert mode";
        else if(portable_alert_save(alert_store,alert_choice)) {
          alert_unconfirmed=false;result=T5_APP_SETTING_UPDATED;break;
        } else {
          /* Keep the selected draft for explicit retry. Neither an IO return
           * nor a failed/mismatching readback proves the old value survived. */
          alert_unconfirmed=true;alert_message="Save unconfirmed - retry";
        }
      }
      settings_view_redraw();
    }
    settings_message=result==T5_APP_SETTING_UPDATED?"Alert mode saved":
        alert_unconfirmed?"Alert mode unconfirmed":"";
    sv_switch(SV_ROOT);return result;
  }
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  if(index==SETTINGS_FLIP_ROW || index==SETTINGS_LANGUAGE_ROW) {
    bool language=index==SETTINGS_LANGUAGE_ROW;unsigned count=language?PORTABLE_READER_LANGUAGE_COUNT:2u;
    int loaded=portable_reader_preference_load(settings_store,language,&reader_choice);
    reader_message=reader_unconfirmed[language]?"Save unconfirmed - retry":
      loaded==PORTABLE_READER_UNAVAILABLE?"Storage unavailable":
      loaded==PORTABLE_READER_INVALID?"Invalid saved - default used":"";
    sv_switch(language?SV_LANGUAGE:SV_FLIP);bool armed=false;t5_app_input_t input;
    uint8_t result=T5_APP_SETTING_NO_CHANGE;
    while(poll(&input,20)) {
      if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
      if(!input.buttons && !input.tapped){armed=true;continue;}
      if(!armed)continue;
      armed=false;
      bool save=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
      if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))reader_choice=(reader_choice+1u)%count;
      else if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))reader_choice=(reader_choice+count-1u)%count;
      if(save) {
        if(portable_reader_preference_save(settings_store,language,reader_choice) &&
           (language || paper_apply_flip(reader_choice))) {
          reader_unconfirmed[language]=false;result=T5_APP_SETTING_UPDATED;break;
        }
        reader_unconfirmed[language]=true;reader_message="Save unconfirmed - retry";
      }
      settings_view_redraw();
    }
    settings_message=result==T5_APP_SETTING_UPDATED?(language?"Language saved":"UI flip saved"):
      reader_unconfirmed[language]?"Save unconfirmed - retry":"";
    sv_switch(SV_ROOT);return result;
  }
  if(index==SETTINGS_FACE_ROW) {
    int loaded=portable_desk_face_load(settings_store,&face_choice);
    face_message=face_unconfirmed?"Save unconfirmed - retry":
        loaded==PORTABLE_DESK_FACE_UNAVAILABLE?"Unavailable: Segments default":
        loaded==PORTABLE_DESK_FACE_INVALID?"Invalid: Segments default":
        loaded==PORTABLE_DESK_FACE_MISSING?"Missing: Segments default":"";
    sv_switch(SV_DESK_FACE);bool armed=false;t5_app_input_t input;
    uint8_t result=T5_APP_SETTING_NO_CHANGE;
    while(poll(&input,20)) {
      if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
      if(!input.buttons && !input.tapped){armed=true;continue;}
      if(!armed)continue;
      armed=false;
      bool save=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
      if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))face_choice=(face_choice+1u)%PORTABLE_DESK_FACE_COUNT;
      else if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))face_choice=(face_choice+PORTABLE_DESK_FACE_COUNT-1u)%PORTABLE_DESK_FACE_COUNT;
      else if(input.tapped)for(unsigned i=0;i<PORTABLE_DESK_FACE_COUNT;++i)
        if(sv_inside(input.touch_x,input.touch_y,20+(int)(i%2)*104,62+(int)(i/2)*36,96,30))face_choice=i;
      if(save) {
        if(portable_desk_face_save(settings_store,face_choice)) {
          face_unconfirmed=false;result=T5_APP_SETTING_UPDATED;break;
        }
        face_unconfirmed=true;face_message="Save unconfirmed - retry";
      }
      settings_view_redraw();
    }
    settings_message=result==T5_APP_SETTING_UPDATED?"Clock face saved":face_unconfirmed?"Clock face unconfirmed":"";
    sv_switch(SV_ROOT);return result;
  }
#endif
  if(index!=0 && index!=SETTINGS_ABOUT_ROW)return T5_APP_SETTING_NO_CHANGE;
  if(index==SETTINGS_ABOUT_ROW) {
    sv_switch(SV_ABOUT);t5_app_input_t input;
    while(poll(&input,20))if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
    sv_switch(SV_ROOT);return T5_APP_SETTING_NO_CHANGE;
  }
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  bool readable=snt_read_local(&settings_draft);if(native_custody_retained)return T5_APP_SETTING_ERROR;
  snt_load_basis();if(native_custody_retained)return T5_APP_SETTING_ERROR;
  snt_result_available=false;
#else
  twatch_rtc_time_v1 raw;
  bool readable=rtc && rtc->read(rtc->context,&raw) && portable_time_forward(&raw,&settings_draft);
  #endif
  if(!readable)settings_draft=(twatch_rtc_time_v1){2000,1,1,6,0,0,0};
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  editor_message=snt_zone_status==PORTABLE_TIMEZONE_INVALID_RECORD || snt_zone_status==PORTABLE_TIMEZONE_UNAVAILABLE?
    "Choose a valid Time Zone first":readable?"Draft only - use Save":
    snt_snapshot_status==PORTABLE_REALTIME_UNSET?"Native unset - edit then Save":"Native unavailable - edit draft";
#else
  editor_message=readable?portable_time_zone():"Unset - edit then SAVE";
#endif
  editor_field=0;sv_scroll[1]=0;sp_field_first=0;sv_switch(SV_FIELDS);
  uint8_t result=T5_APP_SETTING_NO_CHANGE;bool armed=false;t5_app_input_t input;
  while(poll(&input,20)) {
    if(input.exit_requested)break;
    if(!input.buttons && !input.tapped){armed=true;continue;}
    if(!armed)continue;
    armed=false;
    if(input.buttons&T5_APP_BUTTON_BACK) {
      if(sv_page==SV_FIELDS)break;
      sv_switch(SV_FIELDS);continue;
    }
    if(sv_page==SV_VALUE) {
#ifdef PORTABLE_NOVA_UI
      if(input.tapped && editor_field==3 && !settings_hour_24() && sv_inside(input.touch_x,input.touch_y,76,140,88,44)) {
        editor_set_value((settings_draft.hour+12)%24);settings_view_redraw();continue;
      }
#endif
      if(input.buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_DOWN))editor_step(false);
      else if(input.buttons&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_UP))editor_step(true);
      else if(input.buttons&T5_APP_BUTTON_CONFIRM){sv_switch(SV_FIELDS);continue;}
      settings_view_redraw();continue;
    }
    if(sv_page==SV_FOLD) {
      bool choose=false;
      if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN))sv_fold_choice^=1;
      else if(input.buttons&T5_APP_BUTTON_CONFIRM)choose=true;
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,100,184,32)){sv_fold_choice=0;choose=true;}
      else if(input.tapped && sv_inside(input.touch_x,input.touch_y,28,142,184,32)){sv_fold_choice=1;choose=true;}
      if(choose) {
        #ifdef PORTABLE_SETTINGS_NATIVE_TIME
        if(snt_save((int)sv_fold_choice)){result=
#else
        if(editor_save_raw(&save_candidates[sv_fold_choice].rtc)){result=
#endif
T5_APP_SETTING_UPDATED;break;}
        sv_switch(SV_FIELDS);
      } else settings_view_redraw();
      continue;
    }
    if(input.buttons&T5_APP_BUTTON_UP)editor_select_field((editor_field+7)%8);
    else if(input.buttons&T5_APP_BUTTON_DOWN)editor_select_field((editor_field+1)%8);
    else if(input.tapped && input.touch_y>=50 && input.touch_y<184) {
      int y=input.touch_y+sv_scroll[1]-60;
      if(y>=0 && y<6*SV_ROW_HEIGHT){editor_field=(unsigned)y/SV_ROW_HEIGHT;sv_switch(SV_VALUE);continue;}
    } else if(input.buttons&T5_APP_BUTTON_CONFIRM) {
      if(editor_field==7)break;
      if(editor_field<6){sv_switch(SV_VALUE);continue;}
      if(editor_try_save()){result=T5_APP_SETTING_UPDATED;break;}
      continue;
    }
    settings_view_redraw();
  }
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  settings_message=snt_result_available?snt_status_text:"";
#else
  settings_message=result==T5_APP_SETTING_UPDATED?"Saved to RTC":"";
#endif
  sv_switch(SV_ROOT);return result;
}
static uint8_t settings_touch(int16_t x,int16_t y,uint32_t *category,int32_t *selected) {
  if(pp_enabled()) {
    if(!category||!selected||x<32||x>=width()-32||y<SP_TOP||y>=SP_TOP+(int)sp_rows()*SP_ROW)return T5_APP_SETTING_NO_CHANGE;
    unsigned row=sp_first+(unsigned)(y-SP_TOP)/SP_ROW;if(row>=SETTINGS_ROW_COUNT)return T5_APP_SETTING_NO_CHANGE;
    *category=0;*selected=(int32_t)row+1;return settings_activate(0,row);
  }
  if(!category || !selected || x<20 || x>=220 || y<0 || y>=190)return T5_APP_SETTING_NO_CHANGE;
  *category=0;
  for(unsigned i=0;i<SETTINGS_ROW_COUNT;++i)if(y+sv_scroll[0]>=sv_row_y(i) && y+sv_scroll[0]<sv_row_y(i)+SV_ROW_HEIGHT) {
    *selected=(int32_t)i+1;return settings_activate(0,i);
  }
  return T5_APP_SETTING_NO_CHANGE;
}
static bool settings_open(void) {
  if(width()<240 || height()<240)return false;
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
  rtc_grant.struct_size=sizeof(rtc_grant);
#endif
  back_exits_app=true;settings_message="";
  sv_active=false;sv_need_neutral=true;sv_page=SV_ROOT;sp_first=sp_field_first=0;sp_clean=true;sp_dirty=false;
  settings_grant=(risc_runtime_capability_v1){.struct_size=sizeof(settings_grant)};
  settings_store=NULL;time_format_mode=time_format_choice=PORTABLE_TIME_FORMAT_12;time_format_message="";
  if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,RISC_KEY_VALUE_API_V1,PORTABLE_TIME_FORMAT_STORE_INSTANCE,&settings_grant)) {
    if(portable_time_format_api_valid(settings_grant.api))settings_store=settings_grant.api;
  }
  settings_time_format_load();
#ifdef PORTABLE_SETTINGS_TIME_ZONE
  stz_choice=stz_region=stz_selected=stz_first=0;stz_unconfirmed=false;stz_message="";
#endif
#ifdef PORTABLE_SLEEP_SETTINGS
  sleep_choice=SETTINGS_SLEEP_DEFAULT;sleep_message="";
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  alert_choice=0;alert_message="";alert_unconfirmed=false;alert_store=NULL;
  /* All preferences share the same explicit namespace-1 grant. */
  alert_store=settings_store;
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  face_choice=PORTABLE_DESK_SEGMENTS;face_message="";face_unconfirmed=sleep_unconfirmed=false;
  reader_choice=0;reader_message="";reader_unconfirmed[0]=reader_unconfirmed[1]=false;
  /* This profile is only qualified for the retaining monochrome paper UI. */
  if(!pp_enabled())return false;
#endif
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
  if(!rt->acquire(TWATCH_RTC_CAPABILITY,TWATCH_RTC_API_V1,0,&rtc_grant))return false;
  rtc=rtc_grant.api;
  if(!rtc || rtc->api_version!=TWATCH_RTC_API_V1 || rtc->struct_size<sizeof(*rtc) || !rtc->read || !rtc->write)return false;
#endif
#ifdef PORTABLE_INPUT_NAVIGATION
  if(!input_navigation_open()) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    portable_adapter_retain();
#endif
    return false;
  }
#endif
  return true;
}
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
static void settings_close(void) {
  sv_active=false;
#ifdef PORTABLE_ALARM_SETTINGS
  alert_store=NULL;
#endif
  if(settings_grant.api && !rt->release(&settings_grant))rt->diagnostic("PORTABLE_APP error=settings-release");
  settings_store=NULL;
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_close();
#endif
#ifndef PORTABLE_SETTINGS_NATIVE_TIME
  if(rtc_grant.api && !rt->release(&rtc_grant))rt->diagnostic("PORTABLE_APP error=rtc-release");
  rtc=NULL;
#endif
}

#endif
