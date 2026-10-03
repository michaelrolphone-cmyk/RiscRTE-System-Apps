/* Settings-only model/controller, reusing Apps/settings.c and the compact view. */
#include <stdio.h>
#include "PortableTime.h"
#ifndef PORTABLE_SETTINGS_VERSION
#define PORTABLE_SETTINGS_VERSION "dev"
#endif
#ifdef PORTABLE_INPUT_NAVIGATION
#include "RiscInputNavigationV1.h"
static risc_runtime_capability_v1 navigation_grant;
static const risc_input_navigation_api_v1 *navigation;
static bool navigation_neutral, navigation_ready;
#endif
static risc_runtime_capability_v1 rtc_grant;
static const twatch_rtc_api_v1 *rtc;
static const char *settings_message = "";
static const char *editor_message = "";
static unsigned editor_field;
static twatch_rtc_time_v1 settings_draft;
static portable_time_candidate save_candidates[2];
static unsigned save_candidate_count;
static const char *const editor_names[] = {"Year","Month","Day","Hour","Minute","Second"};
static unsigned calendar_days(unsigned year, unsigned month) {
  static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return month >= 1 && month <= 12 ? days[month - 1] + (month == 2 && year % 4 == 0) : 0;
}
static bool calendar_valid(const twatch_rtc_time_v1 *t) {
  return t && t->year >= 2000 && t->year <= 2099 && t->month >= 1 && t->month <= 12 &&
         t->day >= 1 && t->day <= calendar_days(t->year, t->month) && t->weekday <= 6 &&
         t->hour <= 23 && t->minute <= 59 && t->second <= 59;
}
/* Bounded to 100 years. Unsigned seconds through 2099 fit in uint32_t. */
static uint32_t calendar_day_index(const twatch_rtc_time_v1 *t) {
  uint32_t days = 0;
  for (unsigned y = 2000; y < t->year; ++y) days += 365 + (y % 4 == 0);
  for (unsigned m = 1; m < t->month; ++m) days += calendar_days(t->year, m);
  return days + t->day - 1;
}
static uint32_t calendar_seconds(const twatch_rtc_time_v1 *t) {
  return ((calendar_day_index(t) * 24 + t->hour) * 60 + t->minute) * 60 + t->second;
}
static uint8_t calendar_weekday(const twatch_rtc_time_v1 *t) {
  return (uint8_t)((calendar_day_index(t) + 6) % 7); /* 2000-01-01 was Saturday. */
}
static inline void format_wall_time(char *out, size_t capacity, const twatch_rtc_time_v1 *t) {
  unsigned hour = t->hour % 12;
  snprintf(out, capacity, "%u:%02u:%02u %s", hour ? hour : 12,
           (unsigned)t->minute, (unsigned)t->second, t->hour < 12 ? "AM" : "PM");
}

static void set_back_exits(bool enabled) { back_exits_app=enabled; }
static uint32_t settings_categories(void) { return 1; }
static bool settings_category(uint32_t category,char *out,size_t capacity) {
  if(category || !out || capacity<sizeof("System"))return false;
  memcpy(out,"System",sizeof("System"));return true;
}
static uint32_t settings_count(uint32_t category) { return category?0:5; }
static bool settings_get(uint32_t category,uint32_t index,t5_app_setting_t *out) {
  if(category || index>=5 || !out)return false;
  memset(out,0,sizeof(*out));out->type=T5_APP_SETTING_VALUE;
  static const char *const names[]={"Set Time","Time Zone","Time Format","RTC Basis","About"};
  strcpy(out->label,names[index]);
  if(index==0) {
    out->type=T5_APP_SETTING_ACTION;twatch_rtc_time_v1 raw,local;
    if(rtc && rtc->read(rtc->context,&raw) && portable_time_forward(&raw,&local)) {
      unsigned h=local.hour%12;
      snprintf(out->value,sizeof(out->value),"%u:%02u %s",h?h:12,(unsigned)local.minute,local.hour<12?"AM":"PM");
    } else strcpy(out->value,"Unavailable");
  } else if(index==1)strcpy(out->value,portable_time_zone());
  else if(index==2)strcpy(out->value,"12-hour");
  else if(index==3)strcpy(out->value,portable_time_basis());
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
  if(field==3)snprintf(out,capacity,"%u %s",v%12?v%12:12,v<12?"AM":"PM");
  else snprintf(out,capacity,"%u",v);
}
static void editor_step(bool forward) {
  unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);unsigned value=editor_value(editor_field);
  editor_set_value(forward?(value<hi?value+1:lo):(value>lo?value-1:hi));
}
static void settings_navigation_poll(t5_app_input_t *out) {
#ifdef PORTABLE_INPUT_NAVIGATION
  risc_input_navigation_frame_v1 frame;
  if(!navigation || !navigation->poll(navigation->context,&frame)) {navigation_neutral=false;return;}
  if(!navigation_neutral) {if(!frame.buttons)navigation_neutral=true;return;}
  out->buttons=frame.pressed&(T5_APP_BUTTON_BACK|T5_APP_BUTTON_CONFIRM|T5_APP_BUTTON_LEFT|
                              T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN);
#else
  (void)out;
#endif
}
#include "settings_view.inc"
static void settings_render(uint32_t category,int32_t selected) {
  (void)category;
  bool changed=sv_selected!=(unsigned)selected;
  sv_selected=selected<0?0:(unsigned)selected;list_mode=true;sv_active=true;
  if(changed && selected>0 && selected<=5) {
    int y=sv_row_y((unsigned)selected-1)-sv_scroll[0];
    if(y<52)sv_scroll[0]=sv_row_y((unsigned)selected-1)-52;
    else if(y+40>174)sv_scroll[0]=sv_row_y((unsigned)selected-1)-134;
  }
  if(sv_page!=SV_ROOT)sv_switch(SV_ROOT);else settings_view_redraw();
}
/* Exactly one write per explicit Save/choice. Verification stays in raw RTC
 * coordinates and tolerates one ticking second/date boundary within250ms. */
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
static void editor_select_field(unsigned field) {
  editor_field=field;
  if(field<6) {
    int y=60+(int)field*40-sv_scroll[1];
    if(y<60)sv_scroll[1]=(int)field*40;
    else if(y+40>176)sv_scroll[1]=60+(int)field*40-136;
    if(sv_scroll[1]<0)sv_scroll[1]=0;
    if(sv_scroll[1]>144)sv_scroll[1]=144;
  }
}
static bool editor_try_save(void) {
  save_candidate_count=portable_time_inverse(&settings_draft,save_candidates);
  if(!save_candidate_count) {
    editor_message="DST gap or RTC range";sv_switch(SV_FIELDS);return false;
  }
  if(save_candidate_count==2) {sv_fold_choice=0;sv_switch(SV_FOLD);return false;}
  if(editor_save_raw(&save_candidates[0].rtc))return true;
  sv_switch(SV_FIELDS);return false;
}
static uint8_t settings_activate(uint32_t category,uint32_t index) {
  if(category || index>=5)return T5_APP_SETTING_ERROR;
  if(index!=0 && index!=4)return T5_APP_SETTING_NO_CHANGE;
  if(index==4) {
    sv_switch(SV_ABOUT);t5_app_input_t input;
    while(poll(&input,20))if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK))break;
    sv_switch(SV_ROOT);return T5_APP_SETTING_NO_CHANGE;
  }
  twatch_rtc_time_v1 raw;
  bool readable=rtc && rtc->read(rtc->context,&raw) && portable_time_forward(&raw,&settings_draft);
  if(!readable)settings_draft=(twatch_rtc_time_v1){2000,1,1,6,0,0,0};
  editor_message=readable?portable_time_zone():"Unset - edit then SAVE";
  editor_field=0;sv_scroll[1]=0;sv_switch(SV_FIELDS);
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
        if(editor_save_raw(&save_candidates[sv_fold_choice].rtc)){result=T5_APP_SETTING_UPDATED;break;}
        sv_switch(SV_FIELDS);
      } else settings_view_redraw();
      continue;
    }
    if(input.buttons&T5_APP_BUTTON_UP)editor_select_field((editor_field+7)%8);
    else if(input.buttons&T5_APP_BUTTON_DOWN)editor_select_field((editor_field+1)%8);
    else if(input.tapped && input.touch_y>=50 && input.touch_y<184) {
      int y=input.touch_y+sv_scroll[1]-60;
      if(y>=0 && y<240){editor_field=(unsigned)y/40;sv_switch(SV_VALUE);continue;}
    } else if(input.buttons&T5_APP_BUTTON_CONFIRM) {
      if(editor_field==7)break;
      if(editor_field<6){sv_switch(SV_VALUE);continue;}
      if(editor_try_save()){result=T5_APP_SETTING_UPDATED;break;}
      continue;
    }
    settings_view_redraw();
  }
  settings_message=result==T5_APP_SETTING_UPDATED?"Saved to RTC":"";
  sv_switch(SV_ROOT);return result;
}
static uint8_t settings_touch(int16_t x,int16_t y,uint32_t *category,int32_t *selected) {
  if(!category || !selected || x<20 || x>=220 || y<0 || y>=190)return T5_APP_SETTING_NO_CHANGE;
  *category=0;
  for(unsigned i=0;i<5;++i)if(y+sv_scroll[0]>=sv_row_y(i) && y+sv_scroll[0]<sv_row_y(i)+40) {
    *selected=(int32_t)i+1;return settings_activate(0,i);
  }
  return T5_APP_SETTING_NO_CHANGE;
}
static bool settings_open(void) {
  if(width()<240 || height()<240)return false;
  rtc_grant.struct_size=sizeof(rtc_grant);back_exits_app=true;settings_message="";
  sv_active=false;sv_need_neutral=true;sv_page=SV_ROOT;
  if(!rt->acquire(TWATCH_RTC_CAPABILITY,TWATCH_RTC_API_V1,0,&rtc_grant))return false;
  rtc=rtc_grant.api;
  if(!rtc || rtc->api_version!=TWATCH_RTC_API_V1 || rtc->struct_size<sizeof(*rtc) || !rtc->read || !rtc->write)return false;
#ifdef PORTABLE_INPUT_NAVIGATION
  navigation_grant.struct_size=sizeof(navigation_grant);
  if(!rt->acquire("input.navigation",1,0,&navigation_grant))return false;
  navigation=navigation_grant.api;
  if(!navigation || navigation->api_version!=1 || navigation->struct_size<sizeof(*navigation) ||
     !navigation->poll || !navigation->reset || !navigation->foreground)return false;
  navigation_ready=true;
  /* The raw touch consumer owns this source; navigation must suppress any
   * overlapping derived touch mapping before we accept its button events. */
  const risc_input_foreground_v1 foreground[]={{"input.touch.raw",1}};
  if(!navigation->foreground(navigation->context,foreground,1) ||
     !navigation->reset(navigation->context))return false;
  navigation_neutral=false;
#endif
  return true;
}
static void settings_close(void) {
  sv_active=false;
#ifdef PORTABLE_INPUT_NAVIGATION
  if(navigation_ready) {
    bool cleared=navigation->foreground(navigation->context,NULL,0);
    bool reset=navigation->reset(navigation->context);
    if(!cleared || !reset)rt->diagnostic("PORTABLE_APP error=navigation-reset");
    navigation_ready=false;
  }
  if(navigation_grant.api && !rt->release(&navigation_grant))rt->diagnostic("PORTABLE_APP error=navigation-release");
  navigation=NULL;
#endif
  if(rtc_grant.api && !rt->release(&rtc_grant))rt->diagnostic("PORTABLE_APP error=rtc-release");
  rtc=NULL;
}
