/* Execute the unchanged Settings app with its production portable client. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define PORTABLE_SETTINGS_APP
#include "../../lib/PortableApps/src/adapter.c"

int app_module_init(void);
void app_module_fini(void);
const t5_app_manifest_t portable_catalog[] = {{.compatible=false}};
const unsigned portable_catalog_count = 0;
static unsigned scenario, ticks, polls, grants, subscriptions, frame_count, displays;
static unsigned writes, reads_after_write, diagnostics;
static uint16_t framebuffer[240 * 240];
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
static bool test_health(risc_runtime_health_v1 *h) {
  h->uptime_ms = ticks;
  return polls < 120;
}
static void test_yield(uint32_t ms) { ticks += ms; }
static bool test_diagnostic(const char *s) { assert(s); ++diagnostics; return true; }
static bool test_launch(const char *path) { (void)path; assert(!"Unexpected app launch"); return false; }
static bool display_info(void *c, risc_display_info_v1 *out) {
  (void)c; *out = (risc_display_info_v1){.width=240,.height=240,
    .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)}; return true;
}
static bool frame_acquire(void *c, uint32_t f, risc_display_surface_v1 *out) {
  (void)c; assert(!frame_count); frame_count=1;
  *out=(risc_display_surface_v1){.frame=1,.pixels=framebuffer,.width=240,.height=240,
    .stride_bytes=480,.size_bytes=sizeof(framebuffer),.pixel_format=f};return true;
}
static void frame_release(void *c, risc_display_frame_v1 f) {
  (void)c;assert(f==1 && frame_count);frame_count=0;
}
static bool frame_submit(void *c, risc_display_frame_v1 f, const risc_display_rect_v1 *r,
                         size_t n, const risc_display_present_options_v1 *o,
                         risc_display_present_token_v1 *token) {
  (void)c;(void)r;(void)n;(void)o;assert(f==1 && frame_count);
  if(scenario==10)return false;
  frame_count=0;*token=++displays;
  const char *directory=getenv("PORTABLE_SETTINGS_FRAME_DIR");
  if(directory && displays<=12 && (scenario==0 || scenario==21 || scenario==34)){
    char path[512];snprintf(path,sizeof(path),"%s/scenario-%u-frame-%u.ppm",directory,scenario,displays);
    FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;++i){unsigned v=framebuffer[i];
      unsigned char rgb[]={(unsigned char)((v>>11)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};
      assert(fwrite(rgb,1,3,file)==3);
    }fclose(file);
  }
  return true;
}
static bool frame_status(void *c, risc_display_present_token_v1 token, risc_display_present_status_v1 *out) {
  (void)c;assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static uint64_t touch_subscribe(void *c){(void)c;++subscriptions;return 1;}
static bool touch_unsubscribe(void *c,uint64_t s){(void)c;assert(s==1 && subscriptions);--subscriptions;return true;}
static bool touch_poll(void *c,size_t n){(void)c;assert(n==1);++polls;return polls!=fail_poll_at;}
static int32_t touch_next(void *c,uint64_t s,risc_touch_event_v1 *e){(void)c;(void)s;(void)e;return polls==gap_at?-1:0;}
static bool touch_snapshot(void *c,risc_touch_snapshot_v1 *out){
  (void)c;memset(out,0,sizeof(*out));out->width=out->height=240;
  for(size_t i=0;i<input_count;++i)if(input_script[i].poll==polls){
    out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=polls==replace_at?2:1,.x=input_script[i].x,.y=input_script[i].y};
#if PORTABLE_TOUCH_ROTATION == 180
    out->contacts[0].x=239-out->contacts[0].x;out->contacts[0].y=239-out->contacts[0].y;
#endif
    break;
  }return true;
}
static bool rtc_read(void *c,twatch_rtc_time_v1 *out){
  (void)c;
  if(writes){++reads_after_write;if(scenario==4)return false;if(scenario==18)ticks+=300;}
  if(!writes && scenario==2)return false;
  *out=stored;
  if(!writes && scenario==12)out->year=9999;
  if(writes && scenario==11)out->minute=(out->minute+5)%60;
  if(writes && scenario==19)out->second=(out->second+2)%60;
  return true;
}
static bool rtc_write(void *c,const twatch_rtc_time_v1 *in){
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
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *out){
  (void)c;*out=(risc_input_navigation_frame_v1){0};unsigned button=0;
  if(scenario==30){
    switch(polls){case 3:case 8:case 10:case 12:case 22:case 24:case 26:button=RISC_NAV_DOWN;break;
    case 5:case 14:case 19:case 28:button=RISC_NAV_CONFIRM;break;
    case 17:button=RISC_NAV_UP;break;case 32:button=RISC_NAV_BACK;break;}
    out->buttons=out->pressed=button;
    if(polls==18)out->buttons=RISC_NAV_UP; /* Held input must not repeat. */
  }else if(scenario==31 && polls<=5)out->buttons=out->pressed=RISC_NAV_CONFIRM;
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
static bool test_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant){
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
static bool test_release(risc_runtime_capability_v1 *grant){assert(grants && grant->api);--grants;grant->api=NULL;return true;}
static const risc_runtime_api_v1 runtime_api={1,sizeof(runtime_api),test_health,test_yield,test_diagnostic,test_launch,test_acquire,test_release};
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
  if((scenario>=30 && scenario<=32) || scenario>=36)return 0;
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
    if(scenario==15){tap(4,80,140);replace_at=4;}
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
  if(no_write)assert(!writes);
  else {assert(writes==1);assert(reads_after_write>=1 || scenario==3);}
  if(scenario==0){assert(written.year==2025 && written.day==28);assert(!strcmp(settings_message,"Saved to RTC"));}
  if(scenario==2 || scenario==12){assert(written.year==2001 && written.month==1 && written.day==1);}
  if(scenario==5 || scenario==21 || scenario==22)assert(!strcmp(settings_message,"Saved to RTC"));
  if(scenario==21)assert(written.hour==15 && written.minute==30);
  if(scenario==22)assert(written.hour==16 && written.minute==30);
  if(scenario==3 || scenario==4 || scenario==11 || scenario==18 || scenario==19)assert(!settings_message[0]);
  if(scenario==27 || scenario==28 || scenario==34)assert(sv_scroll[0]>0);
  assert(polls<120 || scenario==10);
  printf("portable Settings scenario %u passed\n",scenario);return 0;
}
