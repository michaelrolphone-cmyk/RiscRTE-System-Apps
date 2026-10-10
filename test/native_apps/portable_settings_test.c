/* Execute the unchanged Settings app with its production portable client. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define PORTABLE_SETTINGS_APP
#include "../../lib/PortableApps/src/adapter.c"
#ifndef TEST_PAPER_SETTINGS
#include "nova_settings_coordinates.h"
#endif

int app_module_init(void);
void app_module_fini(void);
const t5_app_manifest_t portable_catalog[] = {{.compatible=false}};
const unsigned portable_catalog_count = 0;
static unsigned scenario, ticks, polls, grants, subscriptions, frame_count, displays;
static unsigned writes, reads_after_write, diagnostics,writes_at_submit_failure;
static bool submit_failed;
/* Ordinary failed submission may release known-owned resources. It must not
 * dispatch more input, service state, acquisition or drawing after observation. */
static void active_provider(void){assert(!submit_failed);}
#ifdef TEST_PAPER_SETTINGS
static uint8_t framebuffer[100 * 480];
#else
static uint16_t framebuffer[240 * 240];
#endif
static twatch_rtc_time_v1 stored = {2024, 2, 29, 4, 23, 59, 59};
static twatch_rtc_time_v1 written;
typedef struct { unsigned poll; uint16_t x, y; } contact;
static contact input_script[48];
static size_t input_count;
static unsigned gap_at, fail_poll_at, replace_at;

static void tap(unsigned at, uint16_t x, uint16_t y) {
  assert(input_count < sizeof(input_script)/sizeof(input_script[0]));
  input_script[input_count++] = (contact){at,x,y};
}
static bool test_health(risc_runtime_health_v1 *h) {active_provider();
  h->uptime_ms = ticks;
  return polls < 120;
}
static void test_yield(uint32_t ms) {active_provider(); ticks += ms; }
static bool test_diagnostic(const char *s) {active_provider(); assert(s); ++diagnostics; return true; }
static unsigned return_launches;
static bool test_launch(const char *path) {active_provider();
#ifdef PORTABLE_HOME_APP
 if(!strcmp(path,PORTABLE_HOME_APP)){assert(sv_page!=SV_ROOT);return_launches++;return true;}
#endif
#ifdef PORTABLE_RETURN_APP
 assert(!strcmp(path,PORTABLE_RETURN_APP));assert(sv_page==SV_ROOT);return_launches++;return true;
#else
 (void)path; assert(!"Unexpected app launch"); return false;
#endif
}
static bool display_info(void *c, risc_display_info_v1 *out) {active_provider();
  (void)c;
#ifdef TEST_PAPER_SETTINGS
  *out=(risc_display_info_v1){.width=800,.height=480,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT,.damage_x_alignment=8,.damage_width_alignment=8};
#else
  *out = (risc_display_info_v1){.width=240,.height=240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};
#endif
  return true;
}
static bool frame_acquire(void *c, uint32_t f, risc_display_surface_v1 *out) {active_provider();
  (void)c; assert(!frame_count); frame_count=1;
#ifdef TEST_PAPER_SETTINGS
  assert(f==RISC_DISPLAY_FORMAT_MONO1);
  *out=(risc_display_surface_v1){.frame=1,.pixels=framebuffer,.width=800,.height=480,.stride_bytes=scenario==215?99:100,.size_bytes=sizeof(framebuffer),.pixel_format=f};
#else
  *out=(risc_display_surface_v1){.frame=1,.pixels=framebuffer,.width=240,.height=240,.stride_bytes=480,.size_bytes=sizeof(framebuffer),.pixel_format=f};
#endif
  return true;
}
static void frame_release(void *c, risc_display_frame_v1 f) {
  (void)c;assert(f==1 && frame_count);frame_count=0;
}
static bool frame_submit(void *c, risc_display_frame_v1 f, const risc_display_rect_v1 *r,
                         size_t n, const risc_display_present_options_v1 *o,
                         risc_display_present_token_v1 *token) {active_provider();
  (void)c;(void)r;(void)n;(void)o;assert(f==1 && frame_count);
  if(scenario==10){submit_failed=true;writes_at_submit_failure=writes;return false;}
  frame_count=0;*token=++displays;
#ifdef TEST_PAPER_SETTINGS
  const char *path=getenv("PAPER_SETTINGS_FRAME");if(path&&displays==1){FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n800 480\n");assert(fwrite(framebuffer,1,sizeof(framebuffer),file)==sizeof(framebuffer));fclose(file);}
#else
  const char *directory=getenv("PORTABLE_SETTINGS_FRAME_DIR");
  if(directory && displays<=12 && (scenario==0 || scenario==21 || scenario==34 || scenario>=40)){
    char path[512];snprintf(path,sizeof(path),"%s/scenario-%u-frame-%u.ppm",directory,scenario,displays);
    FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;++i){unsigned v=framebuffer[i];
      unsigned char rgb[]={(unsigned char)((v>>11)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};
      assert(fwrite(rgb,1,3,file)==3);
    }fclose(file);
  }
#endif
  return true;
}
static bool frame_status(void *c, risc_display_present_token_v1 token, risc_display_present_status_v1 *out) {active_provider();
  (void)c;assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
/* Ordered raw ABI fixture; snapshots alone cannot represent queued taps. */
static risc_touch_snapshot_v1 touch_state;
static risc_touch_event_v1 touch_events[RISC_TOUCH_QUEUE_LENGTH];
static unsigned touch_head,touch_count;
static bool touch_script_snapshot(void *c,risc_touch_snapshot_v1 *out){
  (void)c;memset(out,0,sizeof(*out));
#ifdef TEST_PAPER_SETTINGS
  out->width=480;out->height=800;
  if(scenario==218&&polls==9)out->buttons=RISC_TOUCH_BUTTON_PRIMARY;
#else
  out->width=out->height=240;
#endif
  /* Held-entry case starts before subscription; later DOWN is a new gesture. */
  if(scenario==9 && polls==0){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=80,.y=140};}
  for(size_t i=0;i<input_count;++i)if(input_script[i].poll==polls){
    out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=polls==replace_at?2:1,.x=input_script[i].x,.y=input_script[i].y};
#if defined(PORTABLE_NOVA_UI) && !defined(TEST_PAPER_SETTINGS)
    nova_fixture_choice_coordinates(sv_page,&out->contacts[0].x,&out->contacts[0].y);
#endif
#if PORTABLE_TOUCH_ROTATION == 180
    out->contacts[0].x=239-out->contacts[0].x;out->contacts[0].y=239-out->contacts[0].y;
#endif
    break;
  }return true;
}

static void touch_emit(unsigned kind,risc_touch_contact_v1 point) {
  assert(touch_count<RISC_TOUCH_QUEUE_LENGTH);
  touch_events[(touch_head+touch_count++)%RISC_TOUCH_QUEUE_LENGTH]=(risc_touch_event_v1){
    .sequence=++touch_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=point.id,.x=point.x,.y=point.y};
}
static uint64_t touch_subscribe(void *c){active_provider();
  (void)c;++subscriptions;touch_head=touch_count=0;touch_script_snapshot(NULL,&touch_state);return 1;
}
static bool touch_unsubscribe(void *c,uint64_t s){(void)c;assert(s==1 && subscriptions);--subscriptions;return true;}
static bool touch_poll(void *c,size_t n){active_provider();
  (void)c;assert(n==1);++polls;if(polls==fail_poll_at)return false;
  risc_touch_snapshot_v1 next;touch_script_snapshot(NULL,&next);
  bool old_down=touch_state.contact_count!=0,new_down=next.contact_count!=0;
  bool same=old_down&&new_down&&touch_state.contacts[0].id==next.contacts[0].id;
  if(old_down&&!same)touch_emit(RISC_TOUCH_EVENT_UP,touch_state.contacts[0]);
  if(new_down) {
    if(!same)touch_emit(RISC_TOUCH_EVENT_DOWN,next.contacts[0]);
    else if(memcmp(&touch_state.contacts[0],&next.contacts[0],sizeof(next.contacts[0])))touch_emit(RISC_TOUCH_EVENT_MOVE,next.contacts[0]);
  }
  if(next.buttons!=touch_state.buttons)touch_emit(next.buttons?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,(risc_touch_contact_v1){0});
  next.sequence=touch_state.sequence;next.timestamp_ms=ticks;touch_state=next;return true;
}
static int32_t touch_next(void *c,uint64_t s,risc_touch_event_v1 *e){active_provider();
  (void)c;assert(s==1);if(polls==gap_at){touch_head=touch_count=0;return -1;}
  if(!touch_count)return 0;
  *e=touch_events[touch_head];touch_head=(touch_head+1)%RISC_TOUCH_QUEUE_LENGTH;--touch_count;return 1;
}
static bool touch_snapshot(void *c,risc_touch_snapshot_v1 *out){active_provider();(void)c;*out=touch_state;return true;}
static bool rtc_read(void *c,twatch_rtc_time_v1 *out){active_provider();
  (void)c;
  if(writes){++reads_after_write;if(scenario==4)return false;if(scenario==18)ticks+=300;}
  if(!writes && scenario==2)return false;
  *out=stored;
  if(!writes && scenario==12)out->year=9999;
  if(writes && scenario==11)out->minute=(out->minute+5)%60;
  if(writes && scenario==19)out->second=(out->second+2)%60;
  return true;
}
static bool rtc_write(void *c,const twatch_rtc_time_v1 *in){active_provider();
  (void)c;assert(calendar_valid(in));assert(in->weekday==calendar_weekday(in));
  ++writes;written=*in;
  if(scenario==3)return false;
  stored=*in;
  if(scenario==5)stored=(twatch_rtc_time_v1){2025,1,1,3,0,0,0};
  return true;
}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),
  .get_info=display_info,.acquire=frame_acquire,.release=frame_release,.submit=frame_submit,.present_status=frame_status};
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,touch_subscribe,touch_unsubscribe,touch_poll,touch_next,touch_snapshot};
static twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=rtc_read,.write=rtc_write};
#ifdef PORTABLE_INPUT_NAVIGATION
static unsigned navigation_polls;
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *out){active_provider();
  (void)c;*out=(risc_input_navigation_frame_v1){0};unsigned button=0;unsigned nav_at=++navigation_polls;
  if(scenario==30){
    switch(nav_at){case 3:case 8:case 10:case 12:case 22:case 24:case 26:button=RISC_NAV_DOWN;break;
    case 5:case 14:case 19:case 28:button=RISC_NAV_CONFIRM;break;
    case 17:button=RISC_NAV_UP;break;case 32:button=RISC_NAV_BACK;break;}
    out->buttons=out->pressed=button;
    if(nav_at==18)out->buttons=RISC_NAV_UP; /* Held input must not repeat. */
  }else if(scenario==31 && polls<=5)out->buttons=out->pressed=RISC_NAV_CONFIRM;
  static uint32_t prior_buttons;out->released=prior_buttons&~out->buttons;prior_buttons=out->buttons;
  if(scenario==219&&polls==9)out->pressed=out->released=RISC_NAV_HOME;
  return true;
}
static unsigned nav_resets,nav_foregrounds;
static bool nav_reset(void *c){(void)c;++nav_resets;return scenario!=37 || nav_resets!=1;}
static bool nav_foreground(void *c,const risc_input_foreground_v1 *f,size_t n){
  (void)c;++nav_foregrounds;
  if(n){assert(n==1 && f && !strcmp(f[0].capability,"input.touch.raw") && f[0].api_version==1);}
  else assert(!f);
  return scenario!=36 || !n;
}
static risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,nav_poll,nav_foreground,nav_reset};
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
static unsigned local_opens,local_closes;
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1 *runtime){
  assert(runtime);++local_opens;return scenario==38?NULL:&nav_api;
}
void portable_input_navigation_close(const risc_runtime_api_v1 *runtime){
  assert(runtime);++local_closes;
}
#endif
#endif
static uint8_t format_bytes[64];static uint32_t format_size;
static unsigned format_writes,format_reads;
static bool format_read_error,format_write_error,format_committed_error,format_verify_error,format_mismatch;
#ifdef PORTABLE_SLEEP_SETTINGS
static uint8_t kv_bytes[64];static uint32_t kv_size;static unsigned kv_writes;
#endif
static int32_t kv_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size) {active_provider();
  (void)c;*size=0;
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  if(!strcmp(key,PORTABLE_READER_FLIP_KEY)||!strcmp(key,PORTABLE_READER_LANGUAGE_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  if(!strcmp(key,PORTABLE_ALERT_KEY))return RISC_KEY_VALUE_NOT_FOUND;
#endif
  if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)) {
    ++format_reads;
    if(format_read_error || (format_verify_error && format_writes))return RISC_KEY_VALUE_IO;
    if(!format_size)return RISC_KEY_VALUE_NOT_FOUND;
    if(cap<format_size){*size=format_size;return RISC_KEY_VALUE_BUFFER_SMALL;}
    memcpy(data,format_bytes,format_size);*size=format_size;return RISC_KEY_VALUE_OK;
  }
#ifdef PORTABLE_SLEEP_SETTINGS
  assert(!strcmp(key,PORTABLE_SLEEP_KEY));
  if(!kv_size)return RISC_KEY_VALUE_NOT_FOUND;
  if(cap<kv_size){*size=kv_size;return RISC_KEY_VALUE_BUFFER_SMALL;}
  memcpy(data,kv_bytes,kv_size);*size=kv_size;return RISC_KEY_VALUE_OK;
#else
  assert(!"Unexpected key");return RISC_KEY_VALUE_INVALID;
#endif
}
static int32_t kv_put(void *c,const char *key,const void *data,uint32_t size) {active_provider();
  (void)c;
  if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)) {
    assert(size==4);++format_writes;
    if(format_write_error)return RISC_KEY_VALUE_IO;
    memcpy(format_bytes,data,size);format_size=size;
    if(format_mismatch){format_bytes[2]=0;format_bytes[3]=0xa5;}
    return format_committed_error?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
  }
#ifdef PORTABLE_SLEEP_SETTINGS
  assert(!strcmp(key,PORTABLE_SLEEP_KEY) && size<=64);++kv_writes;
  if(scenario==42)return RISC_KEY_VALUE_IO;
  memcpy(kv_bytes,data,size);kv_size=size;
  return scenario==47?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;
#else
  assert(!"Unexpected key");return RISC_KEY_VALUE_INVALID;
#endif
}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,kv_get,kv_put};
static bool test_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant){active_provider();
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
    assert(version==1 && id==PORTABLE_TIME_FORMAT_STORE_INSTANCE);
    if(scenario==43)return false;
    grant->api=&kv_api;++grants;return true;
  }
  assert(!id && grant->struct_size==sizeof(*grant));
  if(!strcmp(name,"display.output")){assert(version==1);grant->api=&display_api;}
  else if(!strcmp(name,"input.touch.raw")){assert(version==1);grant->api=&touch_api;}
  else if(!strcmp(name,"rtc.clock")){assert(version==2);if(scenario==7)return false;grant->api=&rtc_api;}
#if defined(PORTABLE_INPUT_NAVIGATION) && !defined(PORTABLE_INPUT_NAVIGATION_LOCAL)
  else if(!strcmp(name,"input.navigation")){assert(version==1);grant->api=&nav_api;}
#endif
  else {assert(!strcmp(name,"board.battery"));return false;}
  ++grants;return true;
}
static bool test_release(risc_runtime_capability_v1 *grant){
  assert(grants && grant->api);
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  if(scenario==301 && grant==&paper_preferences_grant)return false;
#endif
  --grants;grant->api=NULL;return true;
}
static const risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.health=test_health,.yield_ms=test_yield,.diagnostic=test_diagnostic,.request_launch=test_launch,.acquire=test_acquire,.release=test_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&runtime_api:NULL;}

static void calendar_checks(void){
  uint32_t day=0;
  for(unsigned year=2000;year<=2099;++year)
    for(unsigned month=1;month<=12;++month)
      for(unsigned d=1;d<=calendar_days(year,month);++d){
        twatch_rtc_time_v1 value={(uint16_t)year,(uint8_t)month,(uint8_t)d,0,0,0,0};
        assert(calendar_valid(&value));assert(calendar_day_index(&value)==day);
        assert(calendar_weekday(&value)==(day+6)%7);++day;
      }
  assert(day==36525);
  twatch_rtc_time_v1 value={2024,2,29,4,0,0,0};settings_draft=value;editor_field=0;editor_step(true);
  assert(settings_draft.year==2025 && settings_draft.day==28);
  editor_field=3;editor_step(false);assert(settings_draft.hour==23);
  editor_step(true);assert(settings_draft.hour==0);
  value=settings_draft;
  char label_text[24];format_wall_time(label_text,sizeof(label_text),&value);assert(!strcmp(label_text,"12:00:00 AM"));
  value.hour=12;format_wall_time(label_text,sizeof(label_text),&value);assert(!strcmp(label_text,"12:00:00 PM"));
  value.month=0;assert(!calendar_valid(&value));value.month=2;value.day=29;assert(!calendar_valid(&value));
}
int main(int argc,char **argv){
  assert(argc==2);scenario=(unsigned)atoi(argv[1]);calendar_checks();
#ifndef PORTABLE_RTC_UTC8_DENVER
  if(scenario>=20 && scenario<=24)return 0;
#endif
#ifndef PORTABLE_INPUT_NAVIGATION
  if((scenario>=30 && scenario<=32) || (scenario>=36 && scenario<40))return 0;
#endif
#ifdef PORTABLE_SLEEP_SETTINGS
  if(scenario>=40) {
    if(scenario==44){kv_size=4;memset(kv_bytes,0xff,4);}
    if(scenario==45){kv_size=4;memcpy(kv_bytes,(uint8_t[]){0x53,1,1,0xa4},4);}
    unsigned selected;int before=portable_sleep_load(&kv_api,&selected);
    if(scenario==44)assert(before==PORTABLE_SLEEP_INVALID && selected==PORTABLE_SLEEP_HYBRID);
    tap(3,100,126); /* Select Deep, still draft. */
    tap(7,scenario==41?60:170,213); /* Cancel or Save. */
    if(scenario==42 || scenario==43 || scenario==47)tap(12,60,213);
    assert(app_module_init()==0);
    t5_app_setting_t item;assert(settings_count(0)==6 && settings_get(0,4,&item));
    assert(!strcmp(item.label,"Clock Sleep Mode"));
    if(scenario==43)assert(!strcmp(item.value,"Unavailable"));
    settings_render(0,0);polls=0;
    uint8_t result=settings_activate(0,4);
    bool saved=scenario==40 || scenario==44 || scenario==45 || scenario==46;
    assert((result==T5_APP_SETTING_UPDATED)==saved);
    assert((strcmp(settings_message,"Sleep mode saved")==0)==saved);
    assert(!writes); /* Mode selection never mutates RTC. */
    if(scenario==41 || scenario==43 || scenario==45)assert(kv_writes==0);
    else assert(kv_writes==1);
    if(scenario==42 || scenario==47)assert(!strcmp(sleep_message,"Save unconfirmed - retry"));
    app_module_fini();assert(!grants && !subscriptions && !frame_count);
    if(saved || scenario==47) {
      assert(portable_sleep_load(&kv_api,&selected)==PORTABLE_SLEEP_LOADED && selected==PORTABLE_SLEEP_DEEP);
      polls=0;assert(app_module_init()==0);
      assert(settings_get(0,4,&item) && !strcmp(item.value,"Deep"));
      app_module_fini();assert(!grants && !subscriptions && !frame_count);
    }
    printf("portable sleep Settings scenario %u passed\n",scenario);return 0;
  }
#endif
  if(scenario==6)rtc_api.struct_size=8;
  if(scenario==16)rtc_api.api_version=1;
  if(scenario==17)rtc_api.write=NULL;
#ifdef PORTABLE_INPUT_NAVIGATION
  if(scenario==32)nav_api.struct_size=8;
#endif
  if(scenario==5)stored=(twatch_rtc_time_v1){2024,12,31,2,23,59,59};
  if(scenario==20)stored=(twatch_rtc_time_v1){2026,3,8,0,16,30,0};
  if(scenario>=21 && scenario<=23)stored=(twatch_rtc_time_v1){2026,11,1,0,15,30,0};
  if(scenario==24)stored=(twatch_rtc_time_v1){2099,12,31,4,23,40,0};
  if(scenario==25){tap(3,40,140);tap(4,110,142);}
  else if(scenario==27 || scenario==28){tap(3,40,140);tap(4,scenario==28?80:42,90);tap(9,120,205);}
  else if(scenario==30){/* Navigation fixture supplies every event. */}
  else if(scenario==31){tap(9,120,205);}
  else if(scenario==34){tap(3,120,170);tap(4,120,20);tap(8,120,170);tap(9,120,70);tap(13,80,120);tap(18,120,205);tap(22,120,205);}
  else if(scenario==35){tap(3,80,177);tap(7,120,205);}
  else {
    tap(3,80,140);
    if(scenario==9){tap(1,80,140);tap(2,80,140);}
    if(scenario==8)gap_at=4;
    if(scenario==14)fail_poll_at=4;
    if(scenario==15){tap(4,80,140);replace_at=4;
      /* Ordered replacement completes the old gesture and begins a new one;
       * dismiss each logical page reached without persisting a draft. */
      tap(12,60,213);tap(16,120,205);
    }
    if(scenario==8 || scenario==9 || scenario==14 || scenario==15)tap(8,120,205);
    else if(scenario==26){tap(7,40,145);tap(8,110,146);tap(13,120,205);}
    else if(scenario==29){tap(7,170,213);tap(8,20,180);tap(13,60,213);tap(17,120,205);}
    else if(scenario==5){tap(7,170,213);tap(12,120,205);}
    else if(scenario>=21 && scenario<=23){
      tap(7,170,213);
      if(scenario==23){tap(12,120,205);tap(16,60,213);tap(20,120,205);}
      else {tap(12,100,scenario==21?115:157);tap(17,120,205);}
    }else if(scenario==20 || scenario==24){
      tap(7,120,150);tap(8,120,65);tap(12,100,115);
      tap(15,scenario==20?198:176,scenario==20?82:124);
      tap(19,120,205);tap(23,170,213);tap(27,60,213);tap(31,120,205);
    }else {
      tap(7,90,80);tap(11,198,82);tap(15,120,205);
      tap(19,scenario==1?60:170,213);
      if(scenario==3 || scenario==4 || scenario==11 || scenario==18 || scenario==19){tap(23,60,213);tap(27,120,205);}
      else tap(23,120,205);
    }
  }
  int init=app_module_init();
  if(scenario==6 || scenario==7 || scenario==16 || scenario==17 || scenario==32 || scenario==36 || scenario==37 || scenario==38){
    assert(init!=0);assert(!grants && !subscriptions && !frame_count);
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
    if(scenario>=32)assert(local_opens==1 && local_closes==1);
#endif
    return 0;
  }
  assert(init==0);app_main();app_module_fini();
  assert(!grants && !subscriptions && !frame_count && !diagnostics);
#ifdef PORTABLE_INPUT_NAVIGATION
  assert(nav_resets>=2 && nav_foregrounds==2);
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
  assert(local_opens==1 && local_closes==1);
#endif
#endif
  bool no_write=scenario==1 || scenario==8 || scenario==9 || scenario==10 || scenario==14 || scenario==15 ||
                scenario==20 || scenario==23 || scenario==24 || (scenario>=25 && scenario<=29) || scenario==31 || scenario==34 || scenario==35;
  if(scenario==10){
    /* Retain logical actions accepted before asynchronous submit reports failure. */
    assert(submit_failed&&writes==writes_at_submit_failure&&writes<=1);
  }else if(no_write)assert(!writes);
  else {assert(writes==1);assert(reads_after_write>=1 || scenario==3);}
  if(scenario==0){assert(written.year==2025 && written.day==28);assert(!strcmp(settings_message,"Saved to RTC"));}
  if(scenario==2 || scenario==12){assert(written.year==2001 && written.month==1 && written.day==1);}
  if(scenario==5 || scenario==21 || scenario==22)assert(!strcmp(settings_message,"Saved to RTC"));
  if(scenario==21)assert(written.hour==15 && written.minute==30);
  if(scenario==22)assert(written.hour==16 && written.minute==30);
  if(scenario==3 || scenario==4 || scenario==11 || scenario==18 || scenario==19)assert(!settings_message[0]);
  if(scenario==27 || scenario==28 || scenario==34)assert(sv_scroll[0]>0);
  assert(polls<120 || scenario==10);
  #ifdef PORTABLE_RETURN_APP
  assert(return_launches==1);
#else
  assert(!return_launches);
#endif
  printf("portable Settings scenario %u passed\n",scenario);return 0;
}
