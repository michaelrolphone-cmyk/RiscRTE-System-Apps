/* Production Settings controller, touch processing and raster adapter. Only
 * provider input/time/completion are scripted; actions never depend on paint. */
#define main settings_fixture_main
#include "portable_native_time_settings_test.c"
#undef main
#include "logical_touch_queue_fixture.h"
static unsigned latency,submitted_at,checks,busy_inputs,logical_at;
static bool pending,verified;
static const char *flow;
static uint8_t immutable_pixels[sizeof(pixels)];
static bool logic_info(void *c,risc_display_info_v1 *out){bool ok=get_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool logic_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 assert(!pending&&!raster_clip_active);bool ok=frame_show(c,f,r,n,o,t);pending=true;submitted_at=ticks;memcpy(immutable_pixels,pixels,sizeof(pixels));return ok;
}
static bool logic_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&t==presents&&!memcmp(immutable_pixels,pixels,sizeof(pixels)));
 out->state=ticks-submitted_at<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)pending=false;
 return true;
}
static unsigned pulse(unsigned start,unsigned count,unsigned buttons){
 return ticks>=start&&ticks<start+count*80&&(ticks-start)%80<20?buttons:0;
}
static bool logic_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();assert(ticks<10000);*out=(risc_input_navigation_frame_v1){0};unsigned b=0;
 if(!strcmp(flow,"drag"))b=0;
 else if(!strcmp(flow,"root"))b=pulse(100,SETTINGS_ABOUT_ROW+1,RISC_NAV_DOWN);
 else if(!strcmp(flow,"fields"))b=pulse(100,1,RISC_NAV_DOWN)|pulse(180,1,RISC_NAV_CONFIRM)|pulse(300,5,RISC_NAV_DOWN);
 else b=pulse(100,2,RISC_NAV_DOWN)|pulse(260,1,RISC_NAV_CONFIRM)|pulse(380,1,RISC_NAV_CONFIRM)|pulse(500,8,RISC_NAV_DOWN);
 unsigned check_at=!strcmp(flow,"timezone")?1400:900;
 if(ticks>=check_at&&!verified){
  if(!strcmp(flow,"drag"))assert(sv_page==SV_ROOT&&portable_scroll_offset(&sp_scroll[0])>=200&&!sp_scroll[0].velocity_q8);
  else if(!strcmp(flow,"root"))assert(sv_page==SV_ABOUT);
  else if(!strcmp(flow,"fields"))assert(sv_page==SV_VALUE&&editor_field==4);
  else assert(sv_page==SV_TIMEZONE_CITIES&&stz_selected==7);
  verified=true;logical_at=ticks;++checks;
 }
 if(ticks>=1600&&ticks<1620)b=RISC_NAV_HOME;
 static unsigned previous;out->buttons=b;out->pressed=b&~previous;out->released=previous&~b;previous=b;return true;
}
static bool logic_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=LOGICAL_WIDTH,.height=LOGICAL_HEIGHT};
 unsigned start=!strcmp(flow,"timezone")?1200:720;
 if(!strcmp(flow,"drag")){
  if(ticks>=100&&ticks<300){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=LOGICAL_WIDTH/2,.y=(uint16_t)(SP_TOP+220-(ticks-100))};if(pending)++busy_inputs;}
  return true;
 }
 if(ticks>=start&&ticks<start+80){
  int y;
  if(!strcmp(flow,"root")){assert(sv_page==SV_ROOT);y=portable_scroll_row_y(&sp_scroll[0],SETTINGS_ABOUT_ROW)+30;}
  else if(!strcmp(flow,"fields")){assert(sv_page==SV_FIELDS);y=portable_scroll_row_y(&sp_scroll[1],4)+30;}
  else {assert(sv_page==SV_TIMEZONE_CITIES);y=portable_scroll_row_y(&stz_scroll,7)+24;}
  assert(y>=100&&y<LOGICAL_HEIGHT-112);
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=LOGICAL_WIDTH/2,.y=(uint16_t)y};if(pending)++busy_inputs;
 }
 return true;
}
static bool logic_acquire(const char *name,uint32_t v,uint64_t i,risc_runtime_capability_v1 *out){
 if(!acquire(name,v,i,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=display_api;a.get_info=logic_info;a.submit=logic_submit;a.present_status=logic_status;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=touch_api;a.poll=logical_raw_poll;a.next=logical_raw_next;a.snapshot=logical_raw_snapshot;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=navigation_api;a.poll=logic_nav;out->api=&a;}
 return true;
}
int main(int argc,char **argv){
 assert(argc==3);flow=argv[1];latency=(unsigned)atoi(argv[2]);test_name="logical-latency";
 zone_id="America/Adak";native_epoch=expected_epoch=1768505696;configure_records();runtime.acquire=logic_acquire;
 logical_raw_init(LOGICAL_WIDTH,LOGICAL_HEIGHT,logic_touch);
 assert(app_module_init()==0);in_main=true;app_main();
 assert(logical_raw_head==logical_raw_tail);
 assert(checks==1&&verified&&logical_at<1600&&!failed&&!retained&&!pending&&launches==1);
 assert(!rtc_reads&&!rtc_writes&&!seeds&&!basis_puts&&!zone_puts&&!other_puts);
 if(latency==2300)assert(busy_inputs>0&&presents==1);
 in_main=false;in_fini=true;app_module_fini();in_fini=false;assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"controller\":\"settings-%s\",\"latency\":%u,\"logical_at\":%u,\"frames\":%u,\"busy_inputs\":%u}\n",flow,latency,logical_at,presents,busy_inputs);return 0;
}
