/* Actual core application loops and production Quick input/rendering. The
 * fixture supplies only providers; gestures never write Quick controller state. */
#ifdef TEST_CORE_SETTINGS
#define main unused_settings_main
#include "portable_native_time_settings_test.c"
#undef main
#define BASE_ACQUIRE acquire
#define BASE_DISPLAY display_api
#define BASE_NAV navigation_api
#define BASE_TOUCH touch_api
#define CORE_RUNTIME runtime
#else
#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
#define BASE_ACQUIRE app_acquire
#define BASE_DISPLAY fx_display
#define BASE_NAV app_navigation
#define BASE_TOUCH fx_touch
#define CORE_RUNTIME fx_runtime
#endif
static const char *motion_case,*motion_frames;
static unsigned motion_intermediate,motion_full,motion_restored,motion_back,motion_home;
static uint8_t motion_background[sizeof(pixels)];
static bool motion_pending;
#ifdef TEST_CORE_SETTINGS
static bool motion_saved_brightness,motion_saved_restore;
static uint8_t motion_restore=40;
#endif
static unsigned motion_submitted,motion_level,motion_off,motion_on,motion_brightness_writes,motion_pending_input,motion_rendered_level;
static uint8_t motion_brightness=40,motion_pending_pixels[sizeof(pixels)];
static bool motion_is(const char *name){return !strcmp(motion_case,name);}
static bool motion_health(risc_runtime_health_v1 *out){io();assert(ticks<6500);out->uptime_ms=ticks;return true;}
static bool motion_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};
 if(motion_is("back-repeat")&&motion_back<2&&ticks>=1000+1700*motion_back){out->buttons=out->pressed=RISC_NAV_BACK;++motion_back;}
 if(motion_is("backlight-latest")&&ticks>=2700&&!motion_back){out->buttons=out->pressed=RISC_NAV_BACK;++motion_back;}
 if((!motion_home&&ticks>=(motion_is("home-interrupted")?180u:4000u))||
    (motion_is("home-interrupted")&&motion_home==1&&ticks>=4000)){out->buttons=out->pressed=RISC_NAV_HOME;++motion_home;}
 return true;
}
static int32_t motion_next(void *c,uint64_t token,risc_touch_event_v1 *out){
 (void)c;(void)token;(void)out;io();
 return motion_is("gap-retained")&&ticks>=160&&ticks<300?-1:0;
}
static bool motion_snapshot(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(motion_is("backlight-latest")&&ticks>=600){
  bool down=false;unsigned x=390,y=100;
  if((ticks>=800&&ticks<880)||(ticks>=1200&&ticks<1280))down=true;
  if(ticks>=1700&&ticks<2100){down=true;y=145;x=ticks<1800?116:ticks<1900?200:336;}
  if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};if(motion_pending)++motion_pending_input;}
  return true;
 }
 unsigned phase=ticks<1700?ticks:ticks-1700;
 bool down=false;unsigned y=20;
 if(phase>=80&&phase<280){down=true;y=phase<120?20:phase<180?200:phase<240?450:650;}
 if(phase>=1000&&phase<1180&&!motion_is("back-repeat")&&
    !(ticks<1700&&(motion_is("replaced-repeat")||motion_is("gap-retained")||motion_is("home-interrupted")))){down=true;y=phase<1050?660:phase<1100?400:100;}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=motion_is("replaced-repeat")&&ticks>=160&&ticks<280?2:1,.x=240,.y=(uint16_t)y};}
 return true;
}
static bool motion_info(void *c,risc_display_info_v1 *out){
 bool ok=BASE_DISPLAY.get_info(c,out);
 if(motion_is("backlight-latest"))out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS|RISC_DISPLAY_INFO_ASYNC_PRESENT;
 return ok;
}
static bool motion_bright(void *c,uint16_t value,uint16_t maximum){
 (void)c;io();assert(!frames&&maximum==100&&value<=100);motion_level=value;
 if(value)++motion_on;else ++motion_off;return true;
}
static bool motion_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 if(!motion_is("backlight-latest"))return BASE_DISPLAY.present_status(c,token,out);
 (void)c;io();assert(token&&motion_pending&&!memcmp(motion_pending_pixels,pixels,sizeof(pixels)));
 if(ticks-motion_submitted<80){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 motion_pending=false;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
#ifdef TEST_CORE_SETTINGS
static int32_t motion_kv_get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *used){
 if(!motion_is("backlight-latest")|| (strcmp(key,PQA_BRIGHTNESS_KEY)&&strcmp(key,PQA_RESTORE_BRIGHTNESS_KEY)))return kv_get(c,key,out,capacity,used);
 io();assert(capacity>=1);bool restore=!strcmp(key,PQA_RESTORE_BRIGHTNESS_KEY);
 *used=0;if(!(restore?motion_saved_restore:motion_saved_brightness))return RISC_KEY_VALUE_NOT_FOUND;
 *(uint8_t*)out=restore?motion_restore:motion_brightness;*used=1;return RISC_KEY_VALUE_OK;
}
static int32_t motion_kv_put(void *c,const char *key,const void *data,uint32_t size){
 if(!motion_is("backlight-latest")|| (strcmp(key,PQA_BRIGHTNESS_KEY)&&strcmp(key,PQA_RESTORE_BRIGHTNESS_KEY)))return kv_put(c,key,data,size);
 io();assert(size==1&&*(const uint8_t*)data<=100);
 if(!strcmp(key,PQA_RESTORE_BRIGHTNESS_KEY)){motion_restore=*(const uint8_t*)data;motion_saved_restore=true;}
 else {motion_brightness=*(const uint8_t*)data;motion_saved_brightness=true;++motion_brightness_writes;}
 return RISC_KEY_VALUE_OK;
}
#endif
static bool motion_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 if(options->intent!=RISC_DISPLAY_PRESENT_CLEAN)assert(options->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 if(!presents)memcpy(motion_background,pixels,sizeof(pixels));
 if(quick_modal){
  if(quick.ui.position_q8>0&&quick.ui.position_q8<PQA_OPEN_Q8)++motion_intermediate;
  if(quick.ui.position_q8==PQA_OPEN_Q8)++motion_full;
  if(!quick.ui.position_q8)assert(!memcmp(motion_background,pixels,sizeof(pixels)));
  if(!memcmp(motion_background,pixels,sizeof(pixels)))++motion_restored;
 }
 if(motion_frames){char path[1024];snprintf(path,sizeof(path),"%s/frame-%03u.pbm",motion_frames,presents);FILE *fp=fopen(path,"wb");assert(fp);fprintf(fp,"P4\n800 480\n");assert(fwrite(pixels,1,sizeof(pixels),fp)==sizeof(pixels));assert(!fclose(fp));}
 if(quick_modal&&quick.ui.applied_brightness_valid)motion_rendered_level=quick.ui.applied_brightness;
 bool ok=BASE_DISPLAY.submit(c,f,r,n,options,token);
 if(ok&&motion_is("backlight-latest")){assert(!motion_pending);motion_pending=true;motion_submitted=ticks;memcpy(motion_pending_pixels,pixels,sizeof(pixels));}
 return ok;
}
static bool motion_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!BASE_ACQUIRE(name,version,instance,out))return false;
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 a;a=BASE_TOUCH;a.snapshot=motion_snapshot;a.next=motion_next;out->api=&a;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 a;a=BASE_NAV;a.poll=motion_nav;out->api=&a;}
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 a;a=BASE_DISPLAY;a.submit=motion_submit;a.get_info=motion_info;a.present_status=motion_present;if(motion_is("backlight-latest"))a.set_brightness=motion_bright;out->api=&a;}
#ifdef TEST_CORE_SETTINGS
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&motion_is("backlight-latest")){static risc_key_value_v1 a;a=*(const risc_key_value_v1*)out->api;a.get=motion_kv_get;a.put=motion_kv_put;out->api=&a;}
#endif
 return true;
}
int main(int argc,char **argv){
 assert(argc==2||argc==3);motion_case=argv[1];if(argc==3)motion_frames=argv[2];
#ifdef TEST_CORE_SETTINGS
 test_name="motion";zone_id="America/Denver";native_epoch=1768505696;configure_records();
#else
 scenario="valid";source_kv.get=app_get;source_kv.put=fx_kv_put;
 CORE_RUNTIME.release=app_release;CORE_RUNTIME.request_launch=app_launch;
#ifdef PORTABLE_FILE_BROWSER_APP
 app_display=fx_display;app_display.submit=app_submit;
#endif
#endif
 CORE_RUNTIME.acquire=motion_acquire;CORE_RUNTIME.health=motion_health;
 assert(app_module_init()==0);
#ifdef TEST_CORE_SETTINGS
 in_main=true;
#endif
 app_main();
 if(motion_is("gap-retained")){
  assert(retained&&barriers==1&&!launches&&motion_intermediate>0);
  app_module_fini();
  printf("{\"case\":\"gap-retained\",\"retained\":true,\"launches\":0,\"intermediate\":%u}\n",motion_intermediate);return 0;
 }
 assert(!retained&&launches==1&&motion_home==(motion_is("home-interrupted")?2u:1u)&&!quick_modal&&!quick_background);
 assert(motion_intermediate>=2);
 if(!motion_is("home-interrupted")){assert(motion_full>=1&&motion_restored>=1);}
 if(motion_is("repeat")||motion_is("back-repeat"))assert(motion_restored>=2);
 assert(!memcmp(motion_background,pixels,sizeof(pixels)));
 if(motion_is("backlight-latest"))assert(motion_off>=1&&motion_on>=2&&motion_brightness_writes==3&&motion_level==90&&motion_brightness==90&&motion_rendered_level==90&&motion_pending_input>0&&!motion_pending);
#ifdef TEST_CORE_SETTINGS
 assert(sv_page==SV_ROOT&&!seeds&&!rtc_writes&&!basis_puts&&!zone_puts&&!other_puts&&!radio_puts&&!quick_puts);
 in_main=false;in_fini=true;
#else
 assert(!storage_live&&!wifi_live&&!credentials_live&&!dirs&&!kv_writes);no_legacy();
#endif
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);
 printf("{\"case\":\"%s\",\"frames\":%u,\"intermediate\":%u,\"fully_open\":%u,\"exact_restores\":%u,\"launches\":%u,\"simulated_ms\":%u,\"brightness_writes\":%u,\"effective_brightness\":%u,\"rendered_brightness\":%u,\"pending_input_samples\":%u}\n",motion_case,presents,motion_intermediate,motion_full,motion_restored,launches,ticks,motion_brightness_writes,motion_level,motion_rendered_level,motion_pending_input);
 return 0;
}
