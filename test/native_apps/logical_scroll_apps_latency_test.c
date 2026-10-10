/* Real Wi-Fi / Files controllers and adapter with deterministic raw edges.
 * The presentation provider owns immutable buffers until completion. */
#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
#include "logical_touch_queue_fixture.h"
#ifdef PORTABLE_WIFI_SETTINGS_APP
extern unsigned scroll_wifi_page(void);
extern const char *scroll_wifi_ssid(void);
#else
extern unsigned scroll_files_mode(void);
extern const portable_touch_scroll *scroll_files_state(void);
extern const char *scroll_files_name(void);
#endif
static unsigned latency,submitted_at,logical_at,busy_inputs,edge_buttons;
static bool pending,verified,by_touch,drag,changed;
static const void *logical_data_grant;
static uint8_t immutable_pixels[sizeof(pixels)];
static bool logic_info(void *c,risc_display_info_v1 *out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool logic_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){
 assert(!pending&&!raster_clip_active);bool ok=fx_submit(c,f,r,n,o,t);pending=true;submitted_at=ticks;memcpy(immutable_pixels,pixels,sizeof(pixels));return ok;
}
static bool logic_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *out){
 (void)c;io();assert(pending&&t==presents&&!memcmp(immutable_pixels,pixels,sizeof(pixels)));
 out->state=ticks-submitted_at<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)pending=false;
 return true;
}
static unsigned pulse(unsigned start,unsigned count,unsigned buttons){return ticks>=start&&ticks<start+count*80&&(ticks-start)%80<20?buttons:0;}
static bool logic_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();assert(ticks<10000);*out=(risc_input_navigation_frame_v1){0};unsigned b;
#ifdef PORTABLE_WIFI_SETTINGS_APP
 b=pulse(100,3,RISC_NAV_DOWN)|pulse(340,1,RISC_NAV_CONFIRM)|pulse(500,10,RISC_NAV_DOWN);
 if(drag)b=0;
 if(!by_touch&&!drag)b|=pulse(1380,1,RISC_NAV_CONFIRM);
 if(ticks>=1500&&!verified){
  if(drag)assert(scroll_wifi_page()==0&&portable_scroll_offset(&wpv_scroll)>=200&&!wpv_scroll.velocity_q8);
  else {assert(scroll_wifi_page()==0);assert(!strcmp(scroll_wifi_ssid(),by_touch?"Network 09":"Network 10"));}verified=true;logical_at=ticks;
 }
#else
 b=pulse(100,12,RISC_NAV_DOWN);
 if(drag)b=0;
 if(!by_touch&&!drag)b|=pulse(1100,1,RISC_NAV_CONFIRM);
 if(ticks>=1300&&!verified){
  if(changed)assert(scroll_files_mode()==0&&!scroll_files_name()[0]);
  else if(drag)assert(scroll_files_mode()==0&&portable_scroll_offset(scroll_files_state())>=200&&!scroll_files_state()->velocity_q8);
  else {assert(scroll_files_mode()==3);assert(!strcmp(scroll_files_name(),by_touch?"File 011.txt":"File 012.txt"));}verified=true;logical_at=ticks;
 }
#endif
 if(ticks>=1800&&ticks<1820)b=RISC_NAV_HOME;
 out->buttons=b;out->pressed=b&~edge_buttons;out->released=edge_buttons&~b;edge_buttons=b;return true;
}
static bool logic_touch(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=(uint16_t)width(),.height=(uint16_t)height()};
#ifdef PORTABLE_WIFI_SETTINGS_APP
 unsigned start=1320;const portable_touch_scroll *s=&wpv_scroll;int row=9;
#else
 unsigned start=1100;const portable_touch_scroll *s=scroll_files_state();int row=11;
#endif
 if(drag){
  if(ticks>=100&&ticks<300){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=width()/2,.y=(uint16_t)(s->view.y+240-(ticks-100))};if(pending)++busy_inputs;}
  return true;
 }
 if(by_touch&&ticks>=start&&ticks<start+80){
  int y=portable_scroll_row_y(s,row)+24;assert(portable_scroll_inside(&s->view,width()/2,y));
  out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=width()/2,.y=(uint16_t)y};if(pending)++busy_inputs;
 }
 return true;
}
#ifdef PORTABLE_WIFI_SETTINGS_APP
static bool logic_scan(void *c,garden_radio_scan_result_v1 *out){
 (void)c;io();*out=(garden_radio_scan_result_v1){.struct_size=sizeof(*out),.state=GARDEN_RADIO_SCAN_DONE,.count=16};
 for(unsigned i=0;i<16;++i){snprintf(out->entries[i].ssid,sizeof(out->entries[i].ssid),"Network %02u",i);out->entries[i].rssi=-30-(int)i;out->entries[i].auth=GARDEN_RADIO_AUTH_OPEN;out->entries[i].channel=1;}return true;
}
#else
static unsigned dir_index;
static risc_storage_dir_t logic_dir(void *c,const char *path){(void)c;io();assert(!dirs&&path[0]=='/');++dirs;dir_index=0;return 1;}
static bool logic_dir_next(void *c,risc_storage_dir_t h,risc_storage_dirent_v1 *out){
 (void)c;io();assert(h==1&&dirs);if(dir_index>=96)return false;unsigned i=95-dir_index++;
 memset(out,0,sizeof(*out));snprintf(out->name,sizeof(out->name),"File %03u.txt",i);out->size=100+i;return true;
}
static bool logic_stat(void *c,const char *path,uint64_t *size,bool *dir){(void)c;io();if(changed&&ticks>=1150)return false;const char *name=strrchr(path,'/');assert(name);*dir=false;*size=100+(unsigned)atoi(name+6);return true;}
#endif
static bool logic_acquire(const char *name,uint32_t v,uint64_t i,risc_runtime_capability_v1 *out){
 if(!app_acquire(name,v,i,out))return false;
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=fx_display;a.get_info=logic_info;a.submit=logic_submit;a.present_status=logic_status;out->api=&a;}
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=fx_touch;a.poll=logical_raw_poll;a.next=logical_raw_next;a.snapshot=logical_raw_snapshot;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=app_navigation;a.poll=logic_nav;out->api=&a;}
#ifdef PORTABLE_WIFI_SETTINGS_APP
 if(!strcmp(name,"net.wifi")){static wifi_api_v1 a;a=app_wifi;a.scan_poll=logic_scan;out->api=&a;logical_data_grant=&a;}
#else
 if(!strcmp(name,"storage.volume")){static risc_storage_volume_api_v1 a;a=volume;a.stat=logic_stat;a.dir_open=logic_dir;a.dir_next=logic_dir_next;out->api=&a;logical_data_grant=&a;}
#endif
 return true;
}
static bool logic_release(risc_runtime_capability_v1 *g){
 if(g->api==logical_data_grant){
#ifdef PORTABLE_WIFI_SETTINGS_APP
  assert(wifi_live);--wifi_live;
#else
  assert(storage_live&&!dirs);--storage_live;
#endif
  return source_release(g);
 }
 return app_release(g);
}
int main(int argc,char **argv){
 assert(argc==3);changed=!strcmp(argv[1],"changed");by_touch=changed||!strcmp(argv[1],"touch");drag=!strcmp(argv[1],"drag");latency=(unsigned)atoi(argv[2]);scenario="valid";
 source_kv.get=app_get;source_kv.put=fx_kv_put;fx_runtime.acquire=logic_acquire;fx_runtime.release=logic_release;fx_runtime.request_launch=app_launch;
 logical_raw_init(TEST_TOOLBAR_NATIVE_HEIGHT,TEST_TOOLBAR_NATIVE_WIDTH,logic_touch);
 assert(app_module_init()==0);app_main();assert(verified&&logical_at<1800&&!failed&&!retained&&!pending&&launches==1);
 assert(!radio_connects&&!kv_writes&&!rtc_reads&&!rtc_writes&&!dirs&&!storage_live&&!wifi_live&&!credentials_live);
 if(latency==2300){if(by_touch||drag)assert(busy_inputs>0);assert(presents==1);}
 assert(logical_raw_head==logical_raw_tail);
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"controller\":\"%s\",\"input\":\"%s\",\"latency\":%u,\"logical_at\":%u,\"frames\":%u,\"busy_inputs\":%u}\n",
#ifdef PORTABLE_WIFI_SETTINGS_APP
 "wifi",
#else
 "files",
#endif
 argv[1],latency,logical_at,presents,busy_inputs);return 0;
}
