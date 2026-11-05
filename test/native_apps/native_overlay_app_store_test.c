/* Real App Store and native Quick/alarm composition with bounded providers. */
#include <stdint.h>
#include "RiscRuntimeV1.h"
static unsigned ov_stride,ov_bytes;
static bool ov_acquire(const char*,uint32_t,uint64_t,risc_runtime_capability_v1*);
static bool ov_release(risc_runtime_capability_v1*),ov_retain(void);
#include NATIVE_UPDATE_OVERLAY_FIXTURE
static bool ov_checked_poll(t5_app_input_t *in,unsigned wait){bool ok=poll(in,wait);if(!ok)fprintf(stderr,"poll failure ticks=%u failed=%d retained=%d quick=%d limit=%u polls=%u\n",ticks,failed,native_custody_retained,quick_modal,limit_polls,poll_count);return ok;}
#define poll ov_checked_poll
#include "native_overlay_providers_test.c"
#undef poll
static risc_display_output_api_v1 ov_display;
static alarm_service_v1 ov_alarm;
static unsigned ov_hardware_brightness=40,ov_ble_state,ov_grant_serial;
static uint64_t ov_clock(void){return ticks;}
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
 if(ov_inject&&!ov_injections&&quick_modal&&ov_quick_frames&&(!ov_torch||(quick.ui.torch&&ov_torch_frames))){fake_alarm.state=ALARM_STATE_ALERT;fake_alarm.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,7,88,9};ov_schedule_dismiss();}
 return fake_alarm_step(c);
}
static int32_t ov_alarm_ack(void*c,const alarm_token_v1*t){assert(!memcmp(t,&fake_alarm.occurrence,sizeof(*t)));return fake_alarm_ack(c,t);}
static bool ov_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 bool ours=true;
 if(!strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY)&&id==1)g->api=&ov_atomic;
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&id==1)g->api=&ov_kv;
 else if(!strcmp(n,"board.battery")){assert(!id);g->api=&ov_battery;}
 else if(!strcmp(n,"display.output")){assert(!id);g->api=&ov_display;}
 else if(!strcmp(n,"input.touch.raw")){assert(!id);g->api=&ov_touch;}
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(!id);g->api=&ov_alarm;}
 else if(!strcmp(n,"bluetooth.hci")){assert(id==16);g->api=&ov_ble;}
 else ours=false;
 if(!ours){if(!fake_acquire(n,v,id,g))return false;}else{assert(v==1);++grant_count;}g->struct_size=sizeof(*g);g->slot=++ov_grant_serial;g->generation=1;return true;
}
static bool ov_release(risc_runtime_capability_v1*g){
 if(g->api==&ov_atomic||g->api==&ov_kv||g->api==&ov_display||g->api==&ov_touch||g->api==&ov_alarm||g->api==&ov_ble||g->api==&ov_battery){if(g->api==&ov_atomic)assert(!ov_token);assert(grant_count);--grant_count;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
 bool ok=fake_release(g);if(ok)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return ok;
}
static void ov_start(void){
 ov_display=display_api;ov_display.get_info=ov_info;ov_display.acquire=ov_frame_acquire;ov_display.submit=ov_frame_submit;ov_display.present_status=ov_display_status;ov_display.set_brightness=ov_brightness;
 ov_alarm=alarm_api;ov_alarm.step=ov_alarm_step;ov_alarm.acknowledge=ov_alarm_ack;start_fixture();limit_polls=1000000;mock_update.count=3;mock_update.state=SOFTWARE_UPDATE_LIST;ust=mock_update;selected=1;
 assert(quick.ui.native_width==ov_width&&quick.ui.logical_height==ov_height);render_update();ov_drain();assert(un_get(&selected_row));confirming=true;confirm_choice=0;un_prepare();render_update();ov_drain();ov_copy_underlay();
}
static void ov_finish(void){assert(portable_update_close());app_module_fini();assert(!grant_count&&!opened&&!native_active&&!frame_count&&!ov_pending&&!ov_subscriptions&&!ov_token&&!ov_retains);}
static void ov_queued_input(void){
 ov_latency=600;uint64_t start=ticks;wn_rect row=wn_footer(1);ov_tap(start+100,(unsigned)(row.x+row.w/2),(unsigned)(row.y+row.h/2));render_update();ov_drain();assert(ticks>=start+600&&ov_pending_edges>=2);
 for(unsigned i=0;i<8;i++){t5_app_input_t in;assert(poll(&in,20));assert(!in.tapped&&!in.buttons&&!connects&&!begins);}
 ov_latency=0;ov_tap(ticks+100,(unsigned)(row.x+row.w/2),(unsigned)(row.y+row.h/2));bool fresh=false;uint64_t until=ticks+1000;
 while(ticks<until){t5_app_input_t in;assert(poll(&in,20));if(in.tapped){fresh=true;assert(portable_wifi_native_accept(&uv));assert(portable_wifi_hit(&uv,in.touch_x,in.touch_y)==WIFI_HIT_SELECT);break;}}
 assert(fresh&&!connects&&!begins&&!ov_submits);ov_drain();
}
int main(int argc,char **argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);assert(which<5);ov_allocate(UW,UH);pixels=(uint16_t*)(void*)ov_frame;ov_start();
 if(which==4)ov_queued_input();else{
  ov_overlay_script(which);ov_run(ticks+5000);
  assert(confirming&&confirm_choice==0&&selected==1&&!connects&&!begins&&!activations&&!writes);
  ov_check_underlay();assert(ov_quick_frames&&!quick_modal&&!quick_restore_pending&&!quick.ui.torch&&ov_hardware_brightness==40);
  if(which==1)ov_saved_pair();else assert(!ov_submits&&!ov_puts);
  if(which==2||which==3)assert(ov_injections==1&&ov_alarm_frames&&(!ov_torch||ov_torch_frames)&&alarm_acks==1&&fake_alarm.state==ALARM_STATE_READY);else assert(!alarm_acks);
 }
 ov_finish();if(which==1){ov_start();assert(quick.volume==0&&quick.restore_volume==50);ov_finish();}
 ov_free();printf("App Store native overlays %ux%u case%u PASS\n",UW,UH,which);return 0;
}
