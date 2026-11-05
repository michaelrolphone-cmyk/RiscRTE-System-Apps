/* Actual Wi-Fi app/controller, namespace 6 profiles and namespace 1 Quick. */
#include <stdint.h>
#include "RiscRuntimeV1.h"
static unsigned ov_stride,ov_bytes;
static bool ov_acquire(const char*,uint32_t,uint64_t,risc_runtime_capability_v1*);
static bool ov_release(risc_runtime_capability_v1*);
static bool ov_retain(void);
#define TEST_WIFI_RUNTIME_BINDINGS .retain_invocation=ov_retain,
#define WIFI_AVAILABILITY_LIBRARY
#include NATIVE_OVERLAY_WIFI_FIXTURE
#include "native_overlay_providers_test.c"
static risc_display_output_api_v1 ov_display;
static alarm_service_v1 ov_alarm;
static unsigned ov_hardware_brightness=40,ov_ble_state;
static uint64_t ov_clock(void){return ticks;}
static bool ov_rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;*t=(twatch_rtc_time_v1){2028,4,20,4,17,23,31};return true;}
static bool ov_rtc_write(void*c,const twatch_rtc_time_v1*t){(void)c;(void)t;assert(!"unexpected RTC write");return false;}
static const twatch_rtc_api_v1 ov_rtc={.api_version=2,.struct_size=sizeof(ov_rtc),.read=ov_rtc_read,.write=ov_rtc_write};
static bool ov_info(void*c,risc_display_info_v1*s){bool ok=fake_info(c,s);s->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;return ok;}
static bool ov_frame_acquire(void*c,uint32_t f,risc_display_surface_v1*s){assert(!ov_pending);return fake_frame(c,f,s);}
static bool ov_frame_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 assert(!ov_pending);bool ok=fake_submit(c,f,r,n,o,t);if(ok){if(alarm_modal)++ov_alarm_frames;else if(quick_modal){++ov_quick_frames;if(quick.ui.torch)++ov_torch_frames;}ov_pending=true;ov_submitted=ticks;memcpy(ov_frozen,ov_frame,ov_bytes);}return ok;
}
static bool ov_brightness(void*c,uint16_t v,uint16_t m){(void)c;assert(!surface.frame&&m==100&&v<=100);ov_hardware_brightness=v;return true;}
static bool ov_ble_set(void*c,bool on){(void)c;ov_ble_state=on;return true;}
static bool ov_ble_get(void*c,uint8_t*v){(void)c;*v=(uint8_t)ov_ble_state;return true;}
static const portable_bluetooth_control_v1 ov_ble={.api_version=1,.struct_size=sizeof(ov_ble),.set_enabled=ov_ble_set,.status=ov_ble_get};
static int32_t ov_alarm_step(void*c){
 if(ov_inject&&!ov_injections&&quick_modal&&ov_quick_frames&&(!ov_torch||(quick.ui.torch&&ov_torch_frames))){
  fake_alarm.state=ALARM_STATE_ALERT;fake_alarm.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,7,88,9};ov_schedule_dismiss();
 }return fake_alarm_step(c);
}
static int32_t ov_alarm_ack(void*c,const alarm_token_v1*t){assert(!memcmp(t,&fake_alarm.occurrence,sizeof(*t)));return fake_alarm_ack(c,t);}
static bool ov_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 bool ours=true;
 if(!strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY)&&id==1)g->api=&ov_atomic;
 else if(!strcmp(n,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY)&&id==1)g->api=&ov_availability;
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&id==1)g->api=&ov_kv;
 else if(!strcmp(n,"board.battery")){assert(!id);g->api=&ov_battery;}
 else if(!strcmp(n,"rtc.clock")){assert(v==2&&!id);g->api=&ov_rtc;}
 else if(!strcmp(n,"display.output")){assert(!id);g->api=&ov_display;}
 else if(!strcmp(n,"input.touch.raw")){assert(!id);g->api=&ov_touch;}
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(!id);g->api=&ov_alarm;}
 else if(!strcmp(n,"bluetooth.hci")){assert(id==16);g->api=&ov_ble;}
 else ours=false;
 if(!ours)return workflow_acquire(n,v,id,g);
 assert(v==1||(!strcmp(n,"rtc.clock")&&v==2));++grant_count;g->struct_size=sizeof(*g);g->slot=++grant_serial;g->generation=1;return true;
}
static bool ov_release(risc_runtime_capability_v1*g){
 if(g->api==&ov_atomic||g->api==&ov_availability||g->api==&ov_kv||g->api==&ov_display||g->api==&ov_touch||g->api==&ov_alarm||g->api==&ov_ble||g->api==&ov_battery||g->api==&ov_rtc){
  if(g->api==&ov_atomic){assert(!ov_token);}
  assert(grant_count);--grant_count;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
 }return workflow_release(g);
}
static void ov_start(void){
 ov_display=display_api;ov_display.get_info=ov_info;ov_display.acquire=ov_frame_acquire;ov_display.submit=ov_frame_submit;ov_display.present_status=ov_display_status;ov_display.set_brightness=ov_brightness;
 ov_alarm=alarm_api;ov_alarm.step=ov_alarm_step;ov_alarm.acknowledge=ov_alarm_ack;current_availability=RISC_KEY_VALUE_AVAILABLE;availability_start();limit_polls=1000000;
 assert(quick.ui.native_width==ov_width&&quick.ui.logical_height==ov_height&&wifi_storage_ready());render();ov_drain();
}
static void ov_wifi_draft(void){draft();draft_dirty=true;wifi_page(WP_ROOT);render();ov_drain();ov_copy_underlay();}
static void ov_wifi_unchanged(void){
 assert(page==WP_ROOT&&draft_dirty&&!strcmp(credentials.ssid,"Fixture network")&&!strcmp(credentials.password,"testpass123"));
 assert(!writes&&!connects&&!text_opens&&!launches&&!submits);ov_check_underlay();
}
static void ov_profile_persistence(void){
 wifi_activate(6);prepared();assert(job_state==1&&!ov_token&&submits==1&&!writes);native_worker();drained();
 assert(native_sequence==1&&ov_submits==1&&!draft_dirty);assert_record(0,"Fixture network","testpass123");
 assert(ov_cells[ov_index(PQA_VOLUME_KEY,false)].value[0]==0);assert(ov_availability_reads&&availability_snapshots);
}
static void ov_queued_input(void){
 ov_latency=600;uint64_t start=ticks;wn_rect row=wn_row(0);ov_tap(start+100,(unsigned)(row.x+row.w/2),(unsigned)(row.y+row.h/2));render();ov_drain();assert(ticks>=start+600&&ov_pending_edges>=2);
 for(unsigned i=0;i<8;i++){t5_app_input_t in;assert(poll(&in,20));assert(!in.tapped&&!in.buttons&&!text_opens&&!connects&&!writes);}
 ov_latency=0;ov_tap(ticks+100,(unsigned)(row.x+row.w/2),(unsigned)(row.y+row.h/2));bool fresh=false;uint64_t until=ticks+1000;
 while(ticks<until){t5_app_input_t in;assert(poll(&in,20));if(in.tapped){fresh=true;wifi_make_view();assert(portable_wifi_native_accept(&view));assert(portable_wifi_hit(&view,in.touch_x,in.touch_y)==0);wifi_activate(0);assert(text_opens==1&&text_live&&wifi_text_active&&!ov_subscriptions);text_done(RISC_TEXT_ENTRY_CANCELLED,"",0);assert(!text_live&&!wifi_text_active);break;}}
 assert(fresh&&text_opens==1&&!connects&&!writes&&!ov_submits);ov_drain();
}
int main(int argc,char**argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);assert(which<=4);ov_allocate(TEST_WIFI_RGB_WIDTH,TEST_WIFI_RGB_HEIGHT);pixels=(uint16_t*)(void*)ov_frame;ov_start();
 if(which==4)ov_queued_input();else{
  ov_wifi_draft();ov_overlay_script(which);ov_run(ticks+5000);ov_wifi_unchanged();
  assert(ov_quick_frames&&!quick_modal&&!quick_restore_pending&&!quick.ui.torch&&ov_hardware_brightness==40);
  if(which==1){ov_saved_pair();ov_profile_persistence();}else assert(!ov_submits&&!ov_puts);
  if(which==2||which==3)assert(ov_injections==1&&ov_alarm_frames&&(!ov_torch||ov_torch_frames)&&alarm_acks==1&&fake_alarm.state==ALARM_STATE_READY);else assert(!alarm_acks);
 }
 if(draft_dirty){assert(!portable_wifi_close()&&page==WP_STORAGE);explicit_discard();assert(!draft_dirty);}
 finish();assert(!ov_subscriptions&&!ov_pending&&!ov_token&&!ov_retains);
 if(which==1){ov_start();assert(quick.volume==0&&quick.restore_volume==50);assert_record(0,"Fixture network","testpass123");finish();assert(!ov_subscriptions);}
 ov_free();printf("Wi-Fi native overlays %ux%u case%u PASS: namespace1_submits=%u paired=%u namespace6_submits=%u alarm_acks=%u\n",ov_width,ov_height,which,ov_submits,ov_pairs,submits,alarm_acks);return 0;
}
