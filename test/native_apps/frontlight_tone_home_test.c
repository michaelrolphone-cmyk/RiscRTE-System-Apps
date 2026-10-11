/* Actual sparse Home startup/wake hooks with a present optional provider.
 * These tests use strict native doubles; the separate ELF suite owns routing. */
#define SPARSE_EXPECT_LAUNCHES 0
#define REFERENCE_HOME_MAIN unused_reference_home_main
#include "shared_quick_home_test.c"
#include "RiscDisplayOutputFrontlightV1.h"
extern void reference_finish_home(void);
static const char *tone_case;
static unsigned tone_value=50,tone_gets,tone_sets,tone_brightness_calls;
static bool tone_fail(const char *operation){return strstr(tone_case,operation)!=NULL;}
static int32_t tone_get(void*c,uint16_t*w,uint16_t*m){
 (void)c;safe();assert(promoted&&!frames);++tone_gets;
 if(tone_fail("get-failed")){terminal=true;return RISC_DISPLAY_TONE_FAILED;}
 *w=(uint16_t)tone_value;*m=100;return RISC_DISPLAY_TONE_OK;
}
static bool tone_set(void*c,uint16_t w,uint16_t m){
 (void)c;safe();assert(promoted&&!frames&&w==80&&m==100);++tone_sets;
 if(tone_fail("set-failed")){terminal=true;return false;}
 tone_value=w;return true;
}
static bool tone_metrics(void*c,risc_display_present_metrics_v1*out){(void)c;(void)out;return false;}
static bool tone_snapshot(void*c,uint32_t f,void*p,size_t n,uint32_t stride){(void)c;(void)f;(void)p;(void)n;(void)stride;return false;}
static bool tone_info(void*c,risc_display_info_v1*out){raw_display_info(c,out);out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;return true;}
static bool tone_brightness(void*c,uint16_t v,uint16_t m){safe();assert((which("terminal")?tone_value==50:tone_value==80)&&v==0);++tone_brightness_calls;return bright(c,v,m);}
static bool tone_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 if(promoted){assert(tone_value==80&&raw_hardware_brightness==0);if(which("gpio"))assert(o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);}
 return frame_submit(c,f,r,n,o,t);
}
static int32_t tone_preferences(void*c,const char*k,void*out,uint32_t cap,uint32_t*size){
 if(!strcmp(k,"brightness")||!strcmp(k,"frontlight_tone")){safe();assert(promoted&&cap>=1);++kv_reads;*(uint8_t*)out=!strcmp(k,"brightness")?0:80;*size=1;return RISC_KEY_VALUE_OK;}
 return preferences(c,k,out,cap,size);
}
static bool tone_navigation(void*c,risc_input_navigation_frame_v1*out){(void)c;safe();assert(promoted);*out=(risc_input_navigation_frame_v1){0};if(polls>=4)reference_finish_home();return true;}
static bool tone_obtain(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*out){
 if(!obtain(name,version,id,out))return false;
 if(!strcmp(name,"storage.key-value")){static risc_key_value_v1 p;p=*(const risc_key_value_v1*)out->api;p.get=tone_preferences;out->api=&p;}
 if(!strcmp(name,"input.navigation")){static risc_input_navigation_api_v1 n;n=navigation;n.poll=tone_navigation;out->api=&n;}
 if(!strcmp(name,"display.output")){
  static risc_display_output_api_v1_frontlight d;d=(risc_display_output_api_v1_frontlight){0};
  d.snapshot.metrics.power=panel;d.snapshot.metrics.power.history.base.struct_size=sizeof(d);
  d.snapshot.metrics.power.history.base.get_info=tone_info;d.snapshot.metrics.power.history.base.set_brightness=tone_brightness;d.snapshot.metrics.power.history.base.submit=tone_submit;
  d.snapshot.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;d.snapshot.metrics.metrics_version=1;d.snapshot.metrics.snapshot=tone_metrics;
  d.snapshot.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;d.snapshot.snapshot_version=1;d.snapshot.copy_completed=tone_snapshot;
  d.frontlight_tag=RISC_DISPLAY_FRONTLIGHT_TAG;d.frontlight_version=1;d.set_tone=tone_set;d.get_tone=tone_get;out->api=&d;
 }
 return true;
}
int main(int argc,char**argv){
 assert(argc==3);tone_case=argv[1];char *mode=strstr(tone_case,"gpio")?"gpio":strstr(tone_case,"minute")?"terminal":"cold";
 char *args[]={argv[0],mode,argv[2]};runtime.acquire=tone_obtain;runtime.resident_shell=ref_resident;
 raw_hardware_brightness=0;dark=true;int result=shared_reference_base_main(argc,args);assert(!result);
 if(strstr(tone_case,"minute")){assert(entries==1&&!tone_gets&&!tone_sets&&tone_brightness_calls==1&&raw_hardware_brightness==0);}
 else if(strstr(tone_case,"failed")){assert(terminal&&barriers==1&&!presents&&!tone_brightness_calls);assert(tone_gets==1&&tone_sets==(unsigned)tone_fail("set-failed"));}
 else {assert(!terminal&&tone_value==80&&tone_gets==tone_sets&&tone_sets==(!strcmp(mode,"gpio")?2u:1u)&&presents&&raw_hardware_brightness==0);assert(tone_brightness_calls==(unsigned)!strcmp(mode,"gpio"));}
 printf("Present-tone sparse Home %s: tone=%u gets=%u sets=%u brightness-writes=%u PASS\n",tone_case,tone_value,tone_gets,tone_sets,tone_brightness_calls);return 0;
}
