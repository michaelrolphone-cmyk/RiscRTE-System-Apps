/* Actual Settings app/controller with native Quick and alarm overlays. */
#include <stdint.h>
static unsigned test_width,test_height,test_stride,test_size;
#define TEST_SETTINGS_CUSTOM_RUNTIME
#define PORTABLE_QUICK_FIXTURE_MAIN inherited_overlay_quick_main
#include NATIVE_OVERLAY_SETTINGS_FIXTURE
#include "native_overlay_providers_test.c"
static risc_display_output_api_v1 ov_display;
static alarm_service_v1 ov_alarm;
static bool ov_saw_fields;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&quick_runtime:NULL;}
static uint64_t ov_clock(void){return ticks;}
static bool ov_health(risc_runtime_health_v1*h){assert(ticks<120000);if(sv_page==SV_FIELDS)ov_saw_fields=true;h->uptime_ms=ticks;return true;}
static bool ov_info(void*c,risc_display_info_v1*s){bool ok=display_info(c,s);s->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;return ok;}
static bool ov_frame_acquire(void*c,uint32_t f,risc_display_surface_v1*s){assert(!ov_pending);return frame_acquire(c,f,s);}
static bool ov_frame_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 assert(!ov_pending);bool ok=frame_submit(c,f,r,n,o,t);if(ok){if(alarm_modal)++ov_alarm_frames;else if(quick_modal){++ov_quick_frames;if(quick.ui.torch)++ov_torch_frames;}ov_pending=true;ov_submitted=ticks;memcpy(ov_frozen,ov_frame,ov_bytes);}return ok;
}
static int32_t ov_alarm_step(void*c){
 if(ov_inject&&!ov_injections&&quick_modal&&ov_quick_frames&&(!ov_torch||(quick.ui.torch&&ov_torch_frames))){
  alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,7,88,9};ov_schedule_dismiss();
 }return service_step(c);
}
static bool ov_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 bool ok=true;
 if(!strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){assert(v==1&&id==1);g->api=&ov_atomic;++grants;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(v==1&&id==1);g->api=&ov_kv;++grants;}
 else if(!strcmp(n,"board.battery")){assert(v==1&&!id);g->api=&ov_battery;++grants;}
 else if(!strcmp(n,"display.output")){assert(v==1&&!id);g->api=&ov_display;++grants;}
 else if(!strcmp(n,"input.touch.raw")){assert(v==1&&!id);g->api=&ov_touch;++grants;}
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(v==1&&!id);g->api=&ov_alarm;++grants;}
 else ok=quick_acquire(n,v,id,g);
 if(ok){g->struct_size=sizeof(*g);g->slot=grants;g->generation=1;}fprintf(stderr,"GRANT %s v%u id%llu ok%d live%u\n",n,v,(unsigned long long)id,ok,grants);return ok;
}
static bool ov_release(risc_runtime_capability_v1*g){if(g->api==&ov_atomic)assert(!ov_token);bool ok=test_release(g);if(ok)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return ok;}
static void ov_start(void){
 scenario=100;alarm_scenario=0;alarm_fake.state=ALARM_STATE_READY;
 ov_display=display_api;ov_display.get_info=ov_info;ov_display.acquire=ov_frame_acquire;ov_display.submit=ov_frame_submit;ov_display.present_status=ov_display_status;ov_display.set_brightness=quick_brightness;
 ov_alarm=service_api;ov_alarm.step=ov_alarm_step;quick_touch_api=touch_api;
 quick_runtime=runtime_api;quick_runtime.acquire=ov_acquire;quick_runtime.release=ov_release;quick_runtime.health=ov_health;quick_runtime.retain_invocation=ov_retain;
 int init=app_module_init();fprintf(stderr,"INIT rc%d failed%d retained%u grants%u settings%p alarm%p\n",init,failed,ov_retains,grants,(void*)settings_store,(void*)alarms.api);assert(init==0&&sn_enabled());assert(quick.ui.native_width==ov_width&&quick.ui.logical_height==ov_height);
 settings_render(0,0);ov_drain();
}
static void ov_settings_draft(void){
 settings_draft=(twatch_rtc_time_v1){2028,4,20,4,17,23,31};editor_field=3;sv_switch(SV_VALUE);ov_drain();ov_copy_underlay();
 assert(sv_page==SV_VALUE&&settings_editing);
}
static void ov_settings_unchanged(void){
 assert(sv_page==SV_VALUE&&settings_editing&&editor_field==3&&settings_draft.year==2028&&settings_draft.hour==17&&settings_draft.minute==23);
 assert(!writes&&!format_writes&&!kv_writes&&!return_launches&&!wifi_launches);ov_check_underlay();
}
static void ov_queued_input(void){
 ov_latency=600;uint64_t start=ticks;unsigned y=(ov_height<200?28:56)+(ov_height<200?32:44)/2;
 ov_tap(start+100,ov_width/2,y);settings_render(0,0);ov_drain();assert(ticks>=start+600&&ov_pending_edges>=2);
 for(unsigned i=0;i<8;i++){t5_app_input_t in;assert(poll(&in,20));assert(!in.tapped&&!in.buttons&&sv_page==SV_ROOT);}
 ov_latency=0;ov_tap(ticks+100,ov_width/2,y);bool fresh=false;uint64_t until=ticks+1000;
 while(ticks<until){t5_app_input_t in;assert(poll(&in,20));if(in.tapped){uint32_t category=0;int32_t selected=0;fresh=true;unsigned m=ov_width<200?8:20,fh=ov_height<200?28:44,gap=ov_width<200?6:12,half=(ov_width-2*m-gap)/2;ov_tap(ticks+300,m+half/2,ov_height-8-fh/2);(void)settings_touch(in.touch_x,in.touch_y,&category,&selected);break;}}
 assert(fresh&&ov_saw_fields&&sv_page==SV_ROOT&&!ov_submits&&!writes);ov_drain();
}
int main(int argc,char**argv){
 assert(argc==4);test_width=(unsigned)atoi(argv[1]);test_height=(unsigned)atoi(argv[2]);unsigned which=(unsigned)atoi(argv[3]);assert(which<=4);
 ov_allocate(test_width,test_height);test_stride=ov_stride;test_size=ov_bytes;framebuffer=(uint16_t*)(void*)ov_frame;ov_start();
 if(which==4)ov_queued_input();else{
  ov_settings_draft();ov_overlay_script(which);ov_run(ticks+5000);ov_settings_unchanged();
  assert(ov_quick_frames&&!quick_modal&&!quick_restore_pending&&!quick.ui.torch&&hardware_brightness==40);
  if(which==1)ov_saved_pair();else assert(!ov_submits&&!ov_puts);
  if(which==2||which==3)assert(ov_injections==1&&ov_alarm_frames&&(!ov_torch||ov_torch_frames)&&acks==1&&alarm_fake.state==ALARM_STATE_READY);else assert(!acks);
 }
 app_module_fini();assert(!grants&&!ov_subscriptions&&!subscriptions&&!frame_count&&!ov_token&&!ov_pending&&!ov_retains);
 if(which==1){ov_start();assert(quick.volume==0&&quick.restore_volume==50);app_module_fini();assert(!grants&&!ov_subscriptions);}
 ov_free();printf("Settings native overlays %ux%u case%u PASS: namespace1_submits=%u paired=%u alarm_acks=%u\n",test_width,test_height,which,ov_submits,ov_pairs,acks);return 0;
}
