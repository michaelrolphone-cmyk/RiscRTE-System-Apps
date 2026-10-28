/* Production Files/controller/adapter/native reader; hardware and Light helper
 * are modeled here. Whole-product tests exercise the actual helper separately. */
#define FILE_SHARED_INPUT_FIXTURE
#include "portable_file_browser_test.c"
#include "PortableNativeTimeToolbar.h"
static unsigned sleep_calls,retains,native_reads,zone_reads;
static bool retain_sleep,clock_unknown,format24;
#ifndef TEST_INSET_LEFT
#define TEST_INSET_LEFT 0
#define TEST_INSET_TOP 0
#define TEST_INSET_RIGHT 0
#define TEST_INSET_BOTTOM 0
#endif
static bool native_info(void*c,risc_display_info_v1*i){test_info(c,i);i->safe_insets=(risc_display_insets_v1){.left=TEST_INSET_LEFT,.top=TEST_INSET_TOP,.right=TEST_INSET_RIGHT,.bottom=TEST_INSET_BOTTOM};return true;}
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){(void)c;++native_reads;if(clock_unknown)return RISC_REALTIME_IO;*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_VALID,.epoch_seconds=1767268800+test_ticks/1000,.monotonic_before_us=(uint64_t)test_ticks*1000,.monotonic_after_us=(uint64_t)test_ticks*1000};return 0;}
static int32_t zone_get(void*c,const char*k,void*b,uint32_t n,uint32_t*used){(void)c;(void)b;(void)n;if(!strcmp(k,"time_format")){if(!format24){*used=0;return RISC_KEY_VALUE_NOT_FOUND;}assert(n>=4);unsigned char value[]={0x54,1,1,0xa4};memcpy(b,value,4);*used=4;return RISC_KEY_VALUE_OK;}assert(!strcmp(k,"time_zone"));++zone_reads;*used=0;return RISC_KEY_VALUE_NOT_FOUND;}
static bool battery_read(void*c,risc_battery_sample_v1*out){(void)c;*out=(risc_battery_sample_v1){.percent=80,.millivolts=4000};return true;}
static const risc_realtime_api_v1 native_clock={1,sizeof(native_clock),&native_reads,native_read};
static const risc_key_value_v1 zone={.api_version=1,.struct_size=sizeof(zone),.get=zone_get};
static const risc_battery_gauge_api_v1 native_battery={1,sizeof(native_battery),NULL,battery_read};
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){(void)c;*out=(risc_input_navigation_frame_v1){0};return true;}
static bool nav_foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;(void)f;(void)n;return true;}
static bool nav_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,nav_foreground,nav_reset};
static risc_display_output_api_v1 native_display;
static bool acquire_native(const char*n,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 assert(!retains&&version==1);
 if(!strcmp(n,"runtime.realtime")){assert(!id);g->api=&native_clock;}
 else if(!strcmp(n,"storage.key-value")){assert(id==1);g->api=&zone;}
 else if(!strcmp(n,"board.battery"))g->api=&native_battery;
 else if(!strcmp(n,"input.navigation"))g->api=&nav;
 else if(!strcmp(n,"display.output"))g->api=&native_display;
 else{if(!test_acquire(n,version,id,g))return false;--test_grants;}
 ++test_grants;g->slot=test_grants;g->generation=1;return true;
}
static bool release_native(risc_runtime_capability_v1*g){assert(!retains);if(!test_release(g))return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool native_launch(const char*name){assert(!strcmp(name,"default.elf"));assert(!fb_grant.api&&!fb_retained_file&&!retains);++test_launches;return true;}
static bool retain_native(void){++retains;return true;}
uint32_t portable_foreground_idle_timeout_ms(void){return 15000;}
int portable_foreground_idle_configure(const risc_runtime_api_v1*r){(void)r;return 0;}
void portable_foreground_idle_timeout_changed(uint32_t n){(void)n;}
int portable_app_foreground_idle_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b){(void)r;(void)d;(void)b;assert(!test_subs&&!touch.grant.api&&navigation_ready&&portable_file_browser_safe());++sleep_calls;test_ticks+=7000;return retain_sleep?-2:1;}
static void neutral(void){touch.neutral=touch.home_neutral=true;touch.down=touch.home_down=touch.resync=touch.cancel_pending=false;input_pending=false;navigation_neutral=true;}
static void check_pixels(void){
 unsigned lit=0;for(unsigned y=0;y<FILE_TEST_HEIGHT;y++)for(unsigned x=0;x<FILE_TEST_STRIDE;x++){
  unsigned p=fb_pixels[y*FILE_TEST_STRIDE+x];if(x>=FILE_TEST_WIDTH){assert(p==0xa5a5);continue;}
  bool inside=x>=content_view.x&&x-content_view.x<content_view.width&&y>=content_view.y&&y-content_view.y<content_view.height;
  if(!inside)assert(!p);
  if(p)++lit;
#ifdef TEST_ROUND
  if(p){int64_t dx=2*(int64_t)x+1-FILE_TEST_WIDTH,dy=2*(int64_t)y+1-FILE_TEST_HEIGHT;assert(dx*dx+dy*dy<=(int64_t)FILE_TEST_WIDTH*FILE_TEST_WIDTH);}
#endif
 }
 assert(lit>1000);
}
static void verify_inverse(int left,int top,int w,int h){
 watch_layout_rect box={left,fb_layout_y(top),w,h},native;
 assert(watch_layout_native_rect(&fb_layout,&box,&native));
 for(unsigned y=0;y<FILE_TEST_HEIGHT;y++)for(unsigned x=0;x<FILE_TEST_WIDTH;x++){
  uint16_t cx=x,cy=y;int lx=-1,ly=-1;
  bool hit=portable_safe_viewport_touch(&content_view,&cx,&cy)&&fb_layout_touch(cx,cy,&lx,&ly)&&portable_nova_hit(lx,ly,left,top,w,h);
  bool expected=(int)x>=native.x+(int)content_view.x&&(int)x<native.x+(int)content_view.x+(int)native.width&&(int)y>=native.y+(int)content_view.y&&(int)y<native.y+(int)content_view.y+(int)native.height;
  assert(hit==expected);
 }
}
int main(int argc,char**argv){
 bool terminal=argc>1&&!strcmp(argv[1],"retained");
 test_directory=argc>2?argv[2]:NULL;memset(fb_pixels,0xa5,sizeof(fb_pixels));
 test_volume=(risc_storage_volume_api_v1){1,sizeof(test_volume),NULL,test_ready,test_ready,test_label,test_stat,test_diropen,test_dirnext,test_dirclose,test_open,test_read,NULL,NULL,test_close,NULL,test_error_get};
 native_display=test_display;native_display.get_info=native_info;
 risc_runtime_api_v1 native=test_runtime;native.acquire=acquire_native;native.release=release_native;native.retain_invocation=retain_native;native.request_launch=native_launch;test_runtime_override=&native;
 assert(app_module_init()==0);assert(fb_open());assert(fb_count==2);fb_draw();check_pixels();
 assert(portable_nova_measure(3,"12:59 PM")<=116);assert(native_reads&&zone_reads);assert(!portable_app_native_hour_24());
 format24=true;fb_draw();assert(portable_app_native_hour_24());check_pixels();
 clock_unknown=true;twatch_rtc_time_v1 sample;assert(!native_toolbar_local(&sample));fb_draw();check_pixels();
 clock_unknown=false;format24=false;assert(native_toolbar_local(&sample));assert(!portable_app_native_hour_24());
 verify_inverse(16,188,64,44);verify_inverse(12,178,68,29);
 fb_mode=FB_FILTER;strcpy(fb_editor,"file");fb_choice=PWK_DELETE;fb_draw();check_pixels();
 char draft[sizeof(fb_editor)];strcpy(draft,fb_editor);
 for(unsigned i=0;i<3;i++){
  neutral();assert(foreground_idle_ready(false));unsigned before=test_grants;
  retain_sleep=terminal;bool ok=foreground_idle_enter();
  if(terminal){assert(!ok&&retains==1&&native_custody_retained&&sleep_calls==1);puts("Files terminal Light retains draft/invocation without resumed provider work PASS");return 0;}
  assert(ok&&test_subs==1&&test_grants==before&&fb_mode==FB_FILTER&&!strcmp(draft,fb_editor));fb_draw();check_pixels();
 }
 assert(sleep_calls==3);
 strcpy(fb_editor,"file1");
 watch_layout_rect done={170,fb_layout_y(185),1,1},point;assert(watch_layout_native_rect(&fb_layout,&done,&point));
 uint16_t tx=point.x+content_view.x,ty=point.y+content_view.y;int lx,ly;
 assert(portable_safe_viewport_touch(&content_view,&tx,&ty)&&fb_layout_touch(tx,ty,&lx,&ly));assert(!fb_touch(lx,ly));assert(fb_count==1);fb_draw();
 fb_activate_file(0);assert(fb_mode==FB_DETAIL);fb_draw();check_pixels();fb_activate();assert(fb_mode==FB_PREVIEW&&fb_preview_count);fb_draw();check_pixels();
 neutral();fb_owned[0].handle=9;assert(!foreground_idle_ready(false));fb_owned[0].handle=0;
 assert(!fb_back());assert(!fb_back());assert(fb_mode==FB_LIST);assert(fb_back());assert(test_launches==1);app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains);
 fb_clock_minute=123;fb_clock_sample_at=789;assert(app_module_init()==0);assert(fb_open());assert(fb_clock_minute==-2&&!fb_clock_sample_at);app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 puts("Native Files: safe physical raster, exact footer/editor inverse, readonly clock, filter/preview/root-return, 3 draft-preserving Light cycles PASS");return 0;
}
