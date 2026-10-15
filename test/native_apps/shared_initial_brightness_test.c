/* Unchanged production adapter, modeled providers, no hardware qualification. */
#define main existing_toolbar_main
#include TOOLBAR_FIXTURE
#undef main
static bool test_async,test_brightness=true,test_user_action;
static unsigned test_level,test_sets,test_completes,test_status_reads,test_fault;
static unsigned test_saved=40;
#ifdef BRIGHTNESS_RGB_WIDTH
static uint8_t test_pixels[BRIGHTNESS_RGB_WIDTH*BRIGHTNESS_RGB_HEIGHT*2];
#endif
static bool test_info(void *c,risc_display_info_v1 *out) {
 bool ok=fx_info(c,out);
#ifdef BRIGHTNESS_RGB_WIDTH
 out->width=BRIGHTNESS_RGB_WIDTH;out->height=BRIGHTNESS_RGB_HEIGHT;
 out->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);
#endif
 if(test_async)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 if(test_brightness)out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;
 return ok;
}
#ifdef BRIGHTNESS_RGB_WIDTH
static bool test_frame(void*c,uint32_t format,risc_display_surface_v1*out){
 (void)c;io();assert(!frames&&format==RISC_DISPLAY_FORMAT_RGB565);frames=1;
 *out=(risc_display_surface_v1){.frame=1,.pixels=test_pixels,.width=BRIGHTNESS_RGB_WIDTH,.height=BRIGHTNESS_RGB_HEIGHT,
  .stride_bytes=BRIGHTNESS_RGB_WIDTH*2,.size_bytes=sizeof(test_pixels),.pixel_format=format};return true;
}
#endif
static bool test_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,
 const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 test_status_reads=0;if(test_fault==1){io();return false;}return fx_submit(c,f,r,n,o,t);
}
static bool test_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 if(test_fault==2){io();return false;}
 if(test_fault==3||test_fault==4){io();out->state=test_fault==3?RISC_DISPLAY_PRESENT_FAILED:RISC_DISPLAY_PRESENT_SUPERSEDED;return true;}
 if(++test_status_reads<3||test_fault==5){io();out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 bool ok=fx_present(c,t,out);++test_completes;
 if(test_fault==6)portable_adapter_retain();
 return ok;
}
static bool test_set(void*c,uint16_t value,uint16_t maximum){
 (void)c;io();assert(value<=maximum&&maximum==100);assert(test_user_action||(!frames&&test_completes));++test_sets;
 if(test_fault==7)return false;
 if(test_fault==8){portable_adapter_retain();return true;}
 test_level=value;return true;
}
static int32_t test_get(void*c,const char*k,void*p,uint32_t cap,uint32_t*used){
 if(!strcmp(k,PQA_BRIGHTNESS_KEY)){io();assert(cap>=1);*(uint8_t*)p=(uint8_t)test_saved;*used=1;return RISC_KEY_VALUE_OK;}
 return fx_kv_get(c,k,p,cap,used);
}
static const risc_key_value_v1 test_kv={1,sizeof(test_kv),NULL,test_get,fx_kv_put};
static risc_display_output_api_v1 test_display;
static const alarm_service_descriptor_v2 test_alarm={.base={2,sizeof(test_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_refresh,fx_alarm_ack,fx_alarm_prepare,fx_alarm_stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=0};
static bool test_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){io();assert(v==2&&!i);++acquires;++live;*g=(risc_runtime_capability_v1){sizeof(*g),acquires,1,&test_alarm};return true;}
 bool ok=fx_acquire(n,v,i,g);if(ok&&g->api==&fx_display)g->api=&test_display;
 if(ok&&g->api==&fx_kv)g->api=&test_kv;
 return ok;
}
static void test_paint(void){if(!portable_paper_frame_ready())return;clear_color(0x2468);present(false);}
static void test_settle(void){
 for(unsigned n=0;n<10020&&!failed&&paper_token;n++){++ticks;(void)paper_present_progress();}
}
int main(int argc,char**argv){
 assert(argc==3);const char*name=argv[2];test_async=!strcmp(argv[1],"async");
 test_level=!strcmp(name,"retained-lit")?40:0;test_brightness=strcmp(name,"unsupported")!=0;
 test_saved=!strcmp(name,"saved-level")?64:40;
 test_fault=!strcmp(name,"submit-failed")?1:!strcmp(name,"status-failed")?2:!strcmp(name,"completion-failed")?3:
  !strcmp(name,"superseded")?4:!strcmp(name,"timeout")?5:!strcmp(name,"status-retained")?6:
  !strcmp(name,"brightness-failed")?7:!strcmp(name,"brightness-retained")?8:0;
 test_display=fx_display;test_display.get_info=test_info;test_display.submit=test_submit;
 test_display.present_status=test_status;test_display.set_brightness=test_set;
 if(!strcmp(name,"missing-callback"))test_display.set_brightness=NULL;
#ifdef BRIGHTNESS_RGB_WIDTH
 test_display.acquire=test_frame;
#endif
 fx_runtime.acquire=test_acquire;
 assert(app_module_init()==0&&!test_sets&&!test_completes&&!retained);assert(!alarm_output_modes());
 unsigned expected=test_saved;
 if(!strcmp(name,"preview-before")||!strcmp(name,"torch-before")){
  bool changed=false;test_user_action=true;
  if(!strcmp(name,"preview-before")){quick.ui.brightness=quick.ui.action_brightness=73;assert(pqa_session_apply(&quick,rt,display,PQA_BRIGHTNESS_PREVIEW,&changed));expected=73;}
  else{quick.ui.torch=true;assert(pqa_session_apply(&quick,rt,display,PQA_TORCH,&changed));expected=100;}
  test_user_action=false;
 }
 unsigned before=test_sets;test_paint();
 if(test_async&&!test_fault){assert(!test_completes&&test_sets==before);assert(paper_present_progress()&&!test_completes&&test_sets==before);}
 test_settle();
#ifdef BRIGHTNESS_BASELINE
 assert(!failed&&test_completes&&!test_sets&&!test_level);app_module_fini();
 puts("Baseline reproduced: completed RGB frame remains at brightness zero PASS");return 0;
#endif
#ifdef BRIGHTNESS_RGB_WIDTH
 if(test_fault||!strcmp(name,"missing-callback")){
  assert(failed&&retained);assert(test_sets==((test_fault==7||test_fault==8)?1u:0u));
  unsigned old=calls;test_paint();(void)paper_present_progress();app_module_fini();assert(calls==old);
  puts("Failed/retained presentation or brightness stays terminal without unblank/callbacks PASS");return 0;
 }
 assert(!failed&&!retained&&test_completes&&test_sets==before+(test_brightness?1u:0u));
 assert(test_level==(test_brightness?expected:0));
#else
 (void)expected;assert(!failed&&!retained&&test_completes&&test_sets==before);
#endif
 before=test_sets;test_paint();test_settle();assert(!failed&&test_sets==before);
#ifdef BRIGHTNESS_RGB_WIDTH
 if(test_brightness){
  bool changed=false;test_user_action=true;quick.ui.torch=false;quick.ui.brightness=quick.ui.action_brightness=68;
  assert(pqa_session_apply(&quick,rt,display,PQA_BRIGHTNESS_PREVIEW,&changed));test_user_action=false;
  assert(test_level==68);before=test_sets;test_paint();test_settle();assert(test_level==68&&test_sets==before);
 }
#endif
 app_module_fini();assert(!live&&!frames&&!subscriptions&&!retained&&!kv_writes);
 for(unsigned repeat=0;repeat<3;repeat++){
  before=test_sets;test_completes=0;test_level=repeat&1?40:0;
  assert(!app_module_init()&&test_sets==before);test_paint();test_settle();
#ifdef BRIGHTNESS_RGB_WIDTH
  assert(test_sets==before+(test_brightness?1u:0u));assert(test_level==(test_brightness?test_saved:(repeat&1?40:0)));
#else
  assert(test_sets==before);
#endif
  app_module_fini();assert(!live&&!frames&&!subscriptions&&!retained&&!kv_writes);
 }
 printf("Shared initial brightness %s %s: completion ordering, repeated reopen/retention, preference and paper policy PASS\n",argv[1],name);
 return 0;
}
