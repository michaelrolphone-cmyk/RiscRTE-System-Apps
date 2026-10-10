/* Actual shared adapter, deterministic bounded setup/lifecycle doubles.
 * Tests every provider through the inherited call counter and freezes frame
 * bytes throughout refused cleanup. No device or wall-clock timing claim. */
#include "wifi_adapter_deferral_test.c"
static bool setup_active,setup_cleanup,setup_arm_close,setup_recursive,setup_cpu,setup_rgb;
static unsigned setup_polls,setup_entries,setup_last,setup_gap,setup_closes,setup_refusals;
static unsigned setup_blocked_calls,setup_blocked_entries,setup_display_done;
static bool setup_close_terminal,setup_acquire_dirty,setup_acquire_lost;
static uint16_t setup_rgb_pixels[240*240];
static uint8_t setup_frozen_pixels[sizeof(setup_rgb_pixels)];
static void *setup_pixels(void){return setup_rgb?(void*)setup_rgb_pixels:(void*)pixels;}
static size_t setup_pixel_bytes(void){return setup_rgb?sizeof(setup_rgb_pixels):sizeof(pixels);}
bool portable_file_browser_cleanup_only(void){return setup_cleanup;}
bool portable_file_browser_setup_service(void){
 assert(!setup_cleanup);
 if(!setup_active)return true;
 ++setup_entries;
 if(setup_recursive){setup_recursive=false;input_service();return !retained;}
 if(ticks-setup_last>=8){
  unsigned gap=ticks-setup_last;if(gap>setup_gap)setup_gap=gap;
  setup_last=ticks;++setup_polls;
 }
 return true;
}
static int fixture_setup_close_step(void){
 if(!setup_active)return -1;
 if(!setup_arm_close&&!setup_cleanup)return -1;
 if(!setup_cleanup){
  setup_cleanup=true;setup_blocked_calls=calls;setup_blocked_entries=setup_entries;
  memcpy(setup_frozen_pixels,setup_pixels(),setup_pixel_bytes());
 }
 assert(calls==setup_blocked_calls&&setup_entries==setup_blocked_entries);
 assert(!memcmp(setup_frozen_pixels,setup_pixels(),setup_pixel_bytes()));
 ++setup_closes;
 if(setup_close_terminal){setup_close_terminal=false;portable_adapter_retain();return 0;}
 if(setup_refusals){--setup_refusals;++ticks;return 0;} /* Native scheduler-only delay. */
 setup_cleanup=setup_active=setup_arm_close=false;
 operation_owned=worker_busy=stop_requested=sharing_active=false;return 1;
}
bool portable_file_browser_setup_cleanup(void){return fixture_setup_close_step()>0;}
static bool setup_health(risc_runtime_health_v1 *out){
 assert(!setup_cleanup);if(setup_cpu)++ticks;return fx_health(out);
}
static bool setup_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!strcmp(name,"bluetooth.session-setup")){
  io();if(setup_acquire_dirty)out->slot=11;
  if(setup_acquire_lost)portable_adapter_retain_silent();return false;
 }
 return checked_acquire(name,version,instance,out);
}
static bool setup_info(void *c,risc_display_info_v1 *out){
 if(!setup_rgb)return fx_info(c,out);
 io();*out=(risc_display_info_v1){.width=240,.height=240,
  .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),
  .flags=RISC_DISPLAY_INFO_PARTIAL_DAMAGE};return true;
}
static bool setup_frame(void *c,uint32_t format,risc_display_surface_v1 *out){
 if(!setup_rgb)return fx_frame(c,format,out);
 io();assert(!frames&&format==RISC_DISPLAY_FORMAT_RGB565);frames=1;
 *out=(risc_display_surface_v1){.frame=1,.pixels=setup_rgb_pixels,.width=240,.height=240,
  .stride_bytes=480,.size_bytes=sizeof(setup_rgb_pixels),.pixel_format=format};return true;
}
static bool setup_present(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out){
 assert(!setup_cleanup);fx_present(c,token,out);
 unsigned before=calls;assert(!portable_file_browser_setup_frame_ready()&&calls==before);
 out->state=ticks<setup_display_done?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool setup_wait(void *c,risc_display_present_token_v1 token,uint32_t timeout,risc_display_present_status_v1 *out){
 (void)timeout;return setup_present(c,token,out);
}
static void setup_begin(void){
 assert(portable_broadcast_stop());setup_active=true;sharing_active=true;setup_last=ticks;setup_gap=setup_polls=0;
}
static void setup_end(void){
 setup_active=sharing_active=false;setup_cpu=false;
 app_module_fini();assert(!retained&&!live&&!frames&&!subscriptions);
}
int main(int argc,char **argv){
 assert(argc==2);const char *name=argv[1];setup_rgb=!strncmp(name,"rgb-",4);if(setup_rgb)name+=4;scenario="valid";
 source_kv.get=checked_get;source_kv.put=checked_put;
 tagged.base.step=checked_alarm_step;tagged.base.status=checked_alarm_status;
 tagged.base.refresh=checked_alarm_refresh;tagged.base.acknowledge=checked_alarm_ack;
 fx_runtime.acquire=setup_acquire;fx_runtime.release=app_release;
 fx_runtime.request_launch=checked_launch;fx_runtime.yield_ms=checked_yield;fx_runtime.health=setup_health;
 app_display=fx_display;app_display.get_info=setup_info;app_display.acquire=setup_frame;app_display.submit=app_submit;app_display.present_status=setup_present;
 assert(app_module_init()==0);paint();
 assert(portable_file_browser_setup_service_supported());
 if(!strncmp(name,"acquire-",8)){
  setup_acquire_dirty=!strcmp(name,"acquire-dirty");setup_acquire_lost=!strcmp(name,"acquire-lost");
  bool wrong=!strcmp(name,"acquire-wrong-instance");
  const risc_runtime_api_v1 *runtime=portable_app_custody_runtime();assert(runtime);
  risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
  assert(!runtime->acquire("bluetooth.session-setup",1,wrong?32:31,&grant));
  if(!strcmp(name,"acquire-empty")){assert(!retained&&!grant.api&&!grant.slot&&!grant.generation);setup_end();}
  else {assert(retained&&barriers==1);unsigned before=calls;app_module_fini();assert(calls==before);}
 }else if(!strcmp(name,"support")){
  app_display.wait_present=setup_wait;assert(!portable_file_browser_setup_service_supported());
  info.flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;assert(portable_file_browser_setup_service_supported());
  info.flags&=~RISC_DISPLAY_INFO_ASYNC_PRESENT;surface_format=RISC_DISPLAY_FORMAT_RGB565;
  assert(portable_file_browser_setup_service_supported());surface_format=RISC_DISPLAY_FORMAT_MONO1;
  app_display.wait_present=NULL;setup_end();
 }else if(!strcmp(name,"input")){
  t5_app_input_t out;for(unsigned i=0;i<3;++i)assert(poll(&out,1));
  setup_begin();unsigned started=ticks;
  assert(poll(&out,400));assert(ticks-started>=390&&setup_polls>=40&&setup_gap<=20);
  setup_end();
 }else if(!strcmp(name,"raster")||!strcmp(name,"snapshot")){
  bool snapshot=!strcmp(name,"snapshot");setup_begin();setup_cpu=true;
  if(snapshot)assert(portable_paper_frame_ready());
  clear_color(0x1357);fill(13,17,100,100,0xffff);text_color(20,40,"IMMUTABLE COMPARISON",22,0);
  present(false);assert(portable_paper_frame_drain());
  assert(setup_polls>10&&setup_gap<=20&&portable_file_browser_setup_frame_ready());
  setup_end();
 }else if(!strcmp(name,"display")||!strcmp(name,"async-display")){
  bool asynchronous=!strcmp(name,"async-display");setup_begin();
  if(asynchronous){info.flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;assert(portable_paper_frame_ready());}
  clear_color(0x2468);setup_display_done=ticks+400;present(false);
  if(asynchronous){assert(!portable_file_browser_setup_frame_ready());assert(paper_token||raster_sealed);}
  assert(portable_paper_frame_drain());assert(setup_polls>=40&&setup_gap<=20);
  assert(portable_file_browser_setup_frame_ready());setup_end();
 }else if(!strcmp(name,"cleanup")||!strcmp(name,"cleanup-fini")||!strcmp(name,"cleanup-terminal")){
  setup_begin();assert(portable_paper_frame_ready());clear_color(0x1357);present(false);
  assert(raster_sealed);unsigned commands=raster_command_count;
  setup_arm_close=true;setup_refusals=4;assert(!portable_file_browser_close()&&setup_cleanup);
  unsigned before=calls;uint32_t health_before=ticks;t5_app_input_t out;
  rt->yield_ms(50);assert(!rt->diagnostic("forbidden"));
  risc_runtime_health_v1 health={.struct_size=sizeof(health)};assert(!rt->health(&health));
  input_service();input_dispatch();clear_color(0);fill(1,1,1,1,0);present(false);
  assert(!portable_paper_frame_ready()&&!portable_paper_frame_drain());
  assert(!paper_present_progress()&&calls==before&&ticks==health_before&&raster_command_count==commands);
  assert(!memcmp(setup_frozen_pixels,setup_pixels(),setup_pixel_bytes()));
  risc_runtime_capability_v1 blocked={.struct_size=sizeof(blocked)};
  assert(!rt->acquire("anything",1,0,&blocked)&&calls==before&&!retained);
  if(!strcmp(name,"cleanup-terminal")){
   setup_close_terminal=true;assert(!poll(&out,100)&&retained&&barriers==1);
   assert(calls==before);app_module_fini();assert(calls==before);return 0;
  }
  if(!strcmp(name,"cleanup-fini")){app_module_fini();assert(!setup_cleanup&&!live&&!frames&&!subscriptions);}
  else {
   for(unsigned i=0;i<4;i++){assert(poll(&out,100)&&!out.buttons&&!out.exit_requested);assert(calls==before);}
   assert(!setup_cleanup&&!retained);assert(portable_paper_frame_drain());setup_end();
  }
  assert(setup_closes==5);
 }else if(!strcmp(name,"battery-refusal")){
  setup_begin();setup_arm_close=true;setup_refusals=3;battery_sample.percent=9;
  t5_app_input_t out;assert(poll(&out,1)&&setup_cleanup&&low_battery_transition_pending);
  assert(calls==setup_blocked_calls&&!out.buttons&&!out.exit_requested&&!failed);
  while(setup_cleanup)assert(poll(&out,10));
  assert(low_battery_poll()&&!low_battery_transition_pending);setup_end();
 }else if(!strcmp(name,"sleep-refusal")){
  setup_begin();setup_arm_close=true;setup_refusals=3;
  assert(idle_sleep()&&setup_cleanup&&wifi_deferred_sleep&&!failed);
  assert(calls==setup_blocked_calls&&!sleep_calls);
  t5_app_input_t out;while(setup_cleanup)assert(poll(&out,10));
  assert(poll(&out,10)&&sleep_calls==1&&!wifi_deferred_sleep);setup_end();
 }else if(!strcmp(name,"recursive")){
  setup_begin();setup_recursive=true;assert(!file_setup_checkpoint()&&retained&&barriers==1);
  unsigned before=calls;input_service();app_module_fini();assert(calls==before);return 0;
 }else assert(!"unknown setup adapter case");
 printf("Files BLE actual adapter %s: max service gap=%ums, polls=%u, close calls=%u PASS\n",argv[1],setup_gap,setup_polls,setup_closes);
}
