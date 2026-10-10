/* Exercise production Settings storage/controller/view, without output grants. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define PORTABLE_SETTINGS_APP
#define PORTABLE_ALARM_SETTINGS
#define PORTABLE_INPUT_NAVIGATION
#include "../../lib/PortableApps/src/adapter.c"
#include "nova_settings_coordinates.h"
int app_module_init(void);
void app_module_fini(void);
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned scenario,ticks,polls,grants,kv_grants,kv_releases,subscriptions,frames,presents;
static unsigned kv_writes,kv_reads,rtc_writes,diagnostics,alert_frames,error_frames;
static uint8_t bytes[64];
static uint32_t blob_size;
static bool exists;
static uint16_t framebuffer[240*240];
typedef struct {unsigned poll;uint16_t x,y;} contact;
static contact contacts[20];
static unsigned contact_count;
static void tap(unsigned at,unsigned x,unsigned y) {
  assert(contact_count<20);contacts[contact_count++]=(contact){at,(uint16_t)x,(uint16_t)y};
}
static bool health(risc_runtime_health_v1 *out) {out->uptime_ms=ticks;return polls<100;}
static void yield_ms(uint32_t ms) {ticks+=ms;}
static bool diagnostic(const char *message) {(void)message;++diagnostics;return true;}
static bool request_launch(const char *path) {(void)path;assert(!"No launch is authorized");return false;}
static bool display_info(void *c,risc_display_info_v1 *out) {
  (void)c;*out=(risc_display_info_v1){.width=240,.height=240,
    .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;
}
static bool frame_acquire(void *c,uint32_t format,risc_display_surface_v1 *out) {
  (void)c;assert(!frames);frames=1;
  *out=(risc_display_surface_v1){.frame=1,.pixels=framebuffer,.width=240,.height=240,
    .stride_bytes=480,.size_bytes=sizeof(framebuffer),.pixel_format=format};return true;
}
static void frame_release(void *c,risc_display_frame_v1 frame) {(void)c;assert(frame==1 && frames);frames=0;}
static bool frame_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rect,
    size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
  (void)c;(void)rect;(void)count;(void)options;assert(frame==1 && frames);frames=0;*token=++presents;
  if(sv_page==SV_ALERT) {
    ++alert_frames;
    if(alert_message[0])++error_frames;
    assert(settings_editing);
  }
  const char *directory=getenv("PORTABLE_ALERT_FRAME_DIR");
  if(directory && (sv_page==SV_ROOT || sv_page==SV_TIME_FORMAT || (sv_page==SV_ALERT && (alert_frames==1 || alert_message[0])))) {
    char path[512];snprintf(path,sizeof(path),"%s/scenario-%u-frame-%u.ppm",directory,scenario,presents);
    FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;++i) {
      unsigned v=framebuffer[i];uint8_t rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};
      assert(fwrite(rgb,1,3,file)==3);
    }
    assert(!fclose(file));
  }
  return true;
}
static bool frame_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
  (void)c;assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static uint64_t touch_subscribe(void *c) {(void)c;++subscriptions;return 1;}
static bool touch_unsubscribe(void *c,uint64_t s) {(void)c;assert(s==1 && subscriptions);--subscriptions;return true;}
static bool touch_poll(void *c,size_t n) {(void)c;assert(n==1);++polls;return true;}
static int32_t touch_next(void *c,uint64_t s,risc_touch_event_v1 *out) {(void)c;(void)s;(void)out;return 0;}
static bool touch_snapshot(void *c,risc_touch_snapshot_v1 *out) {
  (void)c;memset(out,0,sizeof(*out));out->width=out->height=240;
  for(unsigned i=0;i<contact_count;++i)if(contacts[i].poll==polls) {
    out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=contacts[i].x,.y=contacts[i].y};
#ifdef PORTABLE_NOVA_UI
    nova_fixture_choice_coordinates(sv_page,&out->contacts[0].x,&out->contacts[0].y);
#endif
#if PORTABLE_TOUCH_ROTATION == 180
    out->contacts[0].x=239-out->contacts[0].x;out->contacts[0].y=239-out->contacts[0].y;
#endif
    break;
  }
  return true;
}
static bool rtc_read(void *c,twatch_rtc_time_v1 *out) {(void)c;*out=(twatch_rtc_time_v1){2026,10,4,0,12,0,0};return true;}
static bool rtc_write(void *c,const twatch_rtc_time_v1 *value) {(void)c;(void)value;++rtc_writes;assert(!"Alert settings cannot write RTC");return false;}
static int32_t kv_get(void *c,const char *key,void *data,uint32_t capacity,uint32_t *size) {
  (void)c;++kv_reads;*size=0;
  if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#ifdef PORTABLE_SLEEP_SETTINGS
  if(!strcmp(key,PORTABLE_SLEEP_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#endif
  assert(!strcmp(key,"alert_mode") && capacity==1);
  if(scenario==12 || (scenario==7 && kv_writes) || (scenario==16 && kv_writes==1))return RISC_KEY_VALUE_IO;
  if(!exists)return RISC_KEY_VALUE_NOT_FOUND;
  if(capacity<blob_size){*size=blob_size;return RISC_KEY_VALUE_BUFFER_SMALL;}
  memcpy(data,bytes,blob_size);*size=blob_size;return RISC_KEY_VALUE_OK;
}
static int32_t kv_put(void *c,const char *key,const void *data,uint32_t size) {
  (void)c;assert(!strcmp(key,"alert_mode") && size==1);++kv_writes;
  assert(*(const uint8_t *)data>=1 && *(const uint8_t *)data<=3);
  if(scenario==23)return RISC_KEY_VALUE_CONTEXT;
  if(scenario==4 || scenario==6 || ((scenario==15 || scenario==17) && kv_writes==1))
    return scenario==6?RISC_KEY_VALUE_OK:RISC_KEY_VALUE_IO;
  memcpy(bytes,data,size);blob_size=size;exists=true;
  if(scenario==32)blob_size=0;
  if(scenario==33)blob_size=2;
  if(scenario==34)bytes[0]=4;
  if(scenario==35)exists=false;
  return scenario==5?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
}
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *out) {
  (void)c;*out=(risc_input_navigation_frame_v1){0};unsigned button=0;
  if(scenario==28) {
    if(polls==3 || polls==5 || polls==7)button=RISC_NAV_DOWN;
    if(polls==9)button=RISC_NAV_CONFIRM;
  }
  if(scenario==30) {
    if(polls==3)button=RISC_NAV_UP;
    if(polls==5)button=RISC_NAV_CONFIRM;
  }
  out->buttons=out->pressed=button;return true;
}
static bool nav_reset(void *c) {(void)c;return true;}
static bool nav_foreground(void *c,const risc_input_foreground_v1 *f,size_t n) {(void)c;assert((n==1 && f) || (!n && !f));return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),
 .get_info=display_info,.acquire=frame_acquire,.release=frame_release,.submit=frame_submit,.present_status=frame_status};
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,touch_subscribe,touch_unsubscribe,touch_poll,touch_next,touch_snapshot};
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=rtc_read,.write=rtc_write};
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,nav_poll,nav_foreground,nav_reset};
static risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,kv_get,kv_put};
static bool acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant) {
  assert(grant->struct_size==sizeof(*grant));
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
    assert(version==1 && id==1 && !kv_grants);++kv_grants;
    if(scenario==13)return false;
    grant->api=&kv_api;
  } else {
    assert(!id);
    if(!strcmp(name,"display.output")){assert(version==1);grant->api=&display_api;}
    else if(!strcmp(name,"input.touch.raw")){assert(version==1);grant->api=&touch_api;}
    else if(!strcmp(name,"input.navigation")){assert(version==1);grant->api=&nav_api;}
    else if(!strcmp(name,"rtc.clock")){assert(version==2);if(scenario==24)return false;grant->api=&rtc_api;}
    else {assert(!strcmp(name,"board.battery"));return false;}
  }
  ++grants;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
  assert(grants && grant->api);if(grant->api==&kv_api)++kv_releases;
  --grants;grant->api=NULL;return true;
}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=request_launch,.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {return version==1?&runtime_api:NULL;}
static void helper_checks(void) {
  unsigned mode=9;
  assert(portable_alert_load(NULL,&mode)==PORTABLE_ALERT_UNAVAILABLE && mode==0);
  assert(portable_alert_load(&kv_api,NULL)==PORTABLE_ALERT_INVALID);
  assert(!portable_alert_save(NULL,1));
  assert(!portable_alert_save(&kv_api,0) && !portable_alert_save(&kv_api,4));
  assert(!kv_writes);
  risc_key_value_v1 bad=kv_api;bad.get=NULL;assert(!portable_alert_api_valid(&bad));
  bad=kv_api;bad.put=NULL;assert(!portable_alert_api_valid(&bad));
  bad=kv_api;bad.api_version=2;assert(!portable_alert_api_valid(&bad));
  bad=kv_api;bad.struct_size=1;assert(!portable_alert_api_valid(&bad));
}
static void check_row(const char *value) {
  t5_app_setting_t row;assert(settings_get(0,SETTINGS_ALERT_ROW,&row));
  assert(!strcmp(row.label,"Alarm Alerts") && !strcmp(row.value,value));
  assert(row.type==T5_APP_SETTING_ACTION);
}
int main(int argc,char **argv) {
  assert(argc==2);scenario=(unsigned)atoi(argv[1]);assert(scenario<=37);helper_checks();
  if(scenario==8 || scenario==9 || scenario==10 || scenario==11 || scenario==21 || scenario==29) {
    exists=true;blob_size=scenario==9?2:scenario==10?0:1;bytes[0]=scenario==11?4:0;
  }
  if(scenario==4 || scenario==6 || scenario==23 || scenario==36 || scenario==37) {
    exists=true;blob_size=1;bytes[0]=scenario==36?2:scenario==37?3:1;
  }
  if(scenario==14)kv_api.api_version=2;
  if(scenario==25)kv_api.struct_size=1;
  if(scenario==26)kv_api.get=NULL;
  if(scenario==27)kv_api.put=NULL;
  int init=app_module_init();
  if(scenario==24){assert(init!=0 && !grants && !subscriptions && !frames && kv_releases==1);return 0;}
  assert(init==0 && kv_grants==1);
#ifdef PORTABLE_SLEEP_SETTINGS
  assert(SETTINGS_ABOUT_ROW==5 && SETTINGS_ALERT_ROW==6 && settings_count(0)==7);
  assert(alert_store==settings_store);
  t5_app_setting_t row;assert(settings_get(0,4,&row) && !strcmp(row.label,"Clock Sleep Mode"));
#else
  assert(SETTINGS_ABOUT_ROW==4 && SETTINGS_ALERT_ROW==5 && settings_count(0)==6);
#endif
  bool unavailable=scenario==12 || scenario==13 || scenario==14 || (scenario>=25 && scenario<=27);
  bool invalid=(scenario>=8 && scenario<=11) || scenario==21 || scenario==29;
  check_row(unavailable?"Unavailable":invalid?"Invalid":scenario==36?"Sound":scenario==37?"Both":"Vibrate");
  settings_render(0,scenario==31?(int32_t)SETTINGS_ALERT_ROW+1:0);polls=0;
  bool cancel=scenario==0 || scenario==3 || scenario==36 || scenario==37 || (scenario>=8 && scenario<=14) || (scenario>=25 && scenario<=27);
  if(scenario==28 || scenario==30) { /* Actual navigation provider drives controller. */ }
  else if(scenario==29) {tap(3,170,213);tap(7,60,213);}
  else if(scenario==20) {tap(3,30,80);tap(4,110,80);}
  else {
    if((!invalid && !unavailable && scenario!=0 && scenario!=36 && scenario!=37) || scenario==21)
      tap(3,100,scenario==2 || scenario==21?154:scenario==22?80:118);
    if(scenario==19){tap(7,170,213);tap(8,20,180);tap(12,60,213);}
    else {tap(7,cancel?60:170,213);if(scenario==18)tap(8,170,213);}
    if(scenario==15 || scenario==16)tap(12,170,213);
    if(scenario==4 || scenario==6 || scenario==7 || scenario==17 || scenario==23 || (scenario>=32 && scenario<=35))tap(12,60,213);
  }
  uint32_t category=0;int32_t selected=0;
  uint8_t result=scenario==31?settings_touch(100,154,&category,&selected):settings_activate(0,SETTINGS_ALERT_ROW);
  if(scenario==31)assert(category==0 && selected==(int32_t)SETTINGS_ALERT_ROW+1);
  bool saved=scenario==1 || scenario==2 || scenario==5 || scenario==15 || scenario==16 ||
    scenario==18 || scenario==21 || scenario==22 || scenario==28 || scenario==30 || scenario==31;
  bool unconfirmed=scenario==4 || scenario==6 || scenario==7 || scenario==17 || scenario==23 || (scenario>=32 && scenario<=35);
  assert((result==T5_APP_SETTING_UPDATED)==saved);
  assert((strcmp(settings_message,"Alert mode saved")==0)==saved);
  assert(sv_page==SV_ROOT && !settings_editing && alert_frames);
  assert(!rtc_writes && polls<100);
  if(saved) {
    unsigned expected=scenario==2 || scenario==21 || scenario==30?3:scenario==22 || scenario==28?1:2;
    assert(exists && blob_size==1 && bytes[0]==expected && !alert_unconfirmed);
    check_row(portable_alert_name(expected));
    assert(kv_writes==(scenario==15 || scenario==16?2u:1u));
  } else if(unconfirmed) {
    assert(kv_writes==1 && alert_choice==PORTABLE_ALERT_SOUND && alert_unconfirmed && error_frames);
    assert(!strcmp(alert_message,"Save unconfirmed - retry"));check_row("Unconfirmed");
    assert(!strcmp(settings_message,"Alert mode unconfirmed"));
  } else assert(!kv_writes);
  if(invalid && !saved)assert(error_frames && !alert_choice);
  if(unavailable)assert(error_frames);
  if(scenario==36 || scenario==37)assert(alert_choice==(scenario==36?2u:3u));
  if(scenario==29)assert(!strcmp(alert_message,"Choose an alert mode"));
  if(scenario==17) {
    /* Cancelling an uncertain save retains the draft for a later explicit retry. */
    contact_count=0;polls=0;tap(3,170,213);
    assert(settings_activate(0,SETTINGS_ALERT_ROW)==T5_APP_SETTING_UPDATED);
    assert(kv_writes==2 && bytes[0]==PORTABLE_ALERT_SOUND && !alert_unconfirmed);
    check_row("Sound");
  }
  app_module_fini();
  assert(!grants && !subscriptions && !frames && !diagnostics);
  assert(kv_releases==(scenario==13?0u:1u));
  if(scenario==37) {
    polls=0;kv_grants=kv_releases=0;assert(app_module_init()==0);check_row("Both");
    assert(kv_grants==1);app_module_fini();assert(!grants && kv_releases==1);
  }
  printf("portable alarm Settings scenario %u passed\n",scenario);return 0;
}
