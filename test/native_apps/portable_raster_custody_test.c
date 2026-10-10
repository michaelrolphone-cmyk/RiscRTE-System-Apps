/* Additive snapshot custody regressions over the unchanged Quick/Alarm
 * provider fixtures. Each named case runs in a fresh process. */
#ifdef RASTER_CUSTODY_CLIP
#define main raster_base_handoff_main
#include "portable_handoff_test.c"
#undef main
int main(int argc,char **argv) {
 assert(argc==2 && !strcmp(argv[1],"clipped-begin-equivalence"));
 uint16_t expected[240*243];
 for(unsigned snapshot=0;snapshot<2;snapshot++) {
  begin_test();assert(!app_module_init());handoff_finish();
  raster_clip=(portable_scroll_viewport){37,41,19,13};raster_clip_active=true;
  if(snapshot)assert(portable_paper_frame_ready());
  clear_color(0xffff);fill(40,44,3,3,0x1357);present(false);
  assert(portable_paper_frame_drain());
  if(!snapshot)memcpy(expected,mock_pixels,sizeof(expected));
  else assert(!memcmp(expected,mock_pixels,sizeof(expected)));
  raster_clip_active=false;end_test();
 }
 puts("raster custody clipped-begin-equivalence PASS");return 0;
}
#else
#include <stdbool.h>
#include <stdlib.h>
static unsigned custody_allocations,custody_fail_allocation;
static bool custody_fail_offscreen;
static void *custody_malloc(size_t bytes) {
 if(custody_fail_offscreen){custody_fail_offscreen=false;return NULL;}
 if(++custody_allocations==custody_fail_allocation)return NULL;
 return malloc(bytes);
}
static void *custody_calloc(size_t count,size_t bytes) {
 if(custody_fail_offscreen){custody_fail_offscreen=false;return NULL;}
 return calloc(count,bytes);
}
#define malloc custody_malloc
#define calloc custody_calloc
#define PORTABLE_QUICK_FIXTURE_MAIN raster_base_quick_main
#include "quick_adapter_test.c"
#undef malloc
#undef calloc

static void assert_image(uint16_t value) {
 for(unsigned i=0;i<240u*240u;i++)assert(framebuffer[i]==value);
}
static void retained_background(void) {
 quick_background=malloc(quick_frame_bytes());assert(quick_background);
 memcpy(quick_background,expected_background,quick_frame_bytes());quick_modal=true;
}
static void unchanged_snapshot(void) {
 previous_pixels=malloc(sizeof(expected_background));assert(previous_pixels);
 memcpy(previous_pixels,expected_background,sizeof(expected_background));previous_valid=true;
 unsigned before=displays;
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x24e2);present(false);
 assert(portable_paper_frame_drain());
 assert(displays==before && !surface.frame && !paper_token && !raster_sealed);
 assert(display_settled && alarm_pixels_valid);
 bool consumed=false;assert(alarm_foreground(&consumed)&&!consumed&&!failed&&!stop_calls);
 cleanup();
}
static void immediate_during_snapshot(void) {
 unsigned before=displays;
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x1357);
 fill(0,0,240,240,0x3579);present(false);
 assert(raster_sealed);assert(raster_progress());assert(raster_sealed);
 assert(!surface.frame && !frame_count);
 /* A caller that does not use frame_ready retains its old immediate contract. */
 clear_color(0x2468);
 assert(!raster_sealed && !raster_recording && surface.frame && displays==before+1);
 present(false);assert(portable_paper_frame_drain());
 assert(displays==before+2);assert_image(0x2468);cleanup();
}
static void sticky_restore(void) {
 assert(portable_paper_frame_ready());assert(raster_begin_allowed);
 alarm_modal=true;
 assert(alarm_restore(alarm_pixels));
 assert(!raster_recording && !raster_sealed && !surface.frame && !paper_token && display_settled);
 assert(!memcmp(framebuffer,expected_background,sizeof(expected_background)));
 alarm_modal=false;cleanup();
}
static void direct_quick_overlay(void) {
 retained_background();quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;
 uint16_t expected[240*240];memcpy(expected,expected_background,sizeof(expected));
 risc_display_surface_v1 reference=surface;reference.pixels=expected;
 assert(pqa_render(&reference,&quick.ui,"12:34",true,63));
 assert(memcmp(expected,expected_background,sizeof(expected)));
 assert(portable_paper_frame_ready());assert(quick_copy_background());assert(raster_immediate());
 /* The direct renderer must receive an actual lease, never stale pixel fields. */
 assert(surface.frame && !raster_recording && !raster_sealed);
 assert(pqa_render(&surface,&quick.ui,"12:34",true,63));present(false);
 assert(portable_paper_frame_drain());assert(!memcmp(framebuffer,expected,sizeof(expected)));
 quick_modal=false;cleanup();
}
static void interrupted_quick_snapshot(bool partial) {
 retained_background();quick.ui.paper=true;
 assert(portable_paper_frame_ready());assert(quick_copy_background());
 fill(20,20,200,160,0);fill(30,30,180,140,0xffff);present(false);assert(raster_sealed);
 if(partial){assert(raster_progress());assert(raster_sealed);assert(!surface.frame&&!frame_count);}
 assert(quick_interrupt());
 uint16_t *saved=NULL;assert(alarm_save_frame(&saved));
 assert(portable_paper_frame_drain());
 /* Deferred Quick replay must not overwrite the app image selected by interrupt. */
 assert(!memcmp(saved,expected_background,sizeof(expected_background)));
 assert(alarm_restore(saved));alarm_modal=false;quick_modal=false;
 assert(!memcmp(framebuffer,expected_background,sizeof(expected_background)));cleanup();
}
static void quick_action_during_snapshot(void) {
 retained_background();quick.ui.paper=true;
 assert(portable_paper_frame_ready());assert(quick_copy_background());
 fill(20,20,200,160,0);fill(30,30,180,140,0xffff);present(false);
 assert(raster_progress());assert(raster_sealed);assert(!surface.frame&&!frame_count);
 quick.ui.action_dnd=true;assert(quick_apply(PQA_DND));
 assert(pref_present[3] && pref_value[3] && pref_writes[3]==1);
 assert(portable_paper_frame_drain());quick_modal=false;cleanup();
}
static bool refused_acquire(void *context,uint32_t format,risc_display_surface_v1 *out) {
 (void)context;(void)format;(void)out;return false;
}
static void bitmap_allocation_and_acquire_failure(void) {
 retained_background();quick.ui.paper=true;
 quick_display_api.acquire=refused_acquire;
 assert(portable_paper_frame_ready());
 /* Retain bitmap bytes, fail the command node, then fail prefix materialization.
  * A consumed error is not proof that raster_tail points to a new command. */
 custody_fail_allocation=custody_allocations+2;
 assert(!quick_copy_background() && failed);
 assert(!surface.frame && !frame_count);
 app_module_fini();assert(!grants && !subscriptions && !frame_count);
}
static void failed_snapshot_cleanup(bool partial) {
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x1357);
 assert(raster_recording && raster_commands);
 if(partial){present(false);assert(raster_progress());assert(raster_sealed);}
 failed=true;app_module_fini();
 assert(!grants && !subscriptions && !frame_count && !surface.frame);
 assert(!raster_commands && !raster_cursor && !raster_recording && !raster_sealed && !raster_offscreen);
}
static void offscreen_allocation_recovery(void) {
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x1357);present(false);
 custody_fail_offscreen=true;assert(raster_progress());
 assert(!custody_fail_offscreen && !raster_sealed && !raster_offscreen && !surface.frame);
 assert(display_settled && alarm_pixels_valid);assert_image(0x1357);
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x2468);present(false);
 assert(raster_progress());assert(raster_sealed && raster_offscreen && !surface.frame && !frame_count);
 assert(portable_paper_frame_drain());assert_image(0x2468);cleanup();
}
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
static unsigned terminal_calls;
static bool retain_during_band(void) {assert(!terminal_calls);terminal_calls++;return true;}
static bool terminal_band_health(risc_runtime_health_v1 *out) {
 assert(!terminal_calls && raster_replaying && raster_offscreen);
 assert(surface.pixels==raster_offscreen && !surface.frame);
 out->uptime_ms=ticks;portable_adapter_retain_silent();return false;
}
static void terminal_band_restore(void) {
 quick_runtime.retain_invocation=retain_during_band;native_custody_runtime=&quick_runtime;
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x1357);present(false);
 unsigned live_grants=grants,live_subscriptions=subscriptions,submitted=displays;
 quick_runtime.health=terminal_band_health;
 assert(!raster_progress());
 assert(terminal_calls==1 && native_custody_retained && failed);
 assert(!surface.frame && !surface.pixels && !frame_count);
 assert(native_custody_surface.frame==0 && native_custody_surface.pixels==raster_offscreen);
 assert(raster_offscreen && raster_commands && raster_sealed);
 app_module_fini();
 assert(terminal_calls==1 && grants==live_grants && subscriptions==live_subscriptions && displays==submitted);
 assert(raster_offscreen && raster_commands && !surface.pixels);
}
#endif
#ifdef PORTABLE_TEXT_INPUT_CLIENT
static bool text_custody_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 bool ok=quick_acquire(name,version,instance,grant);if(ok){grant->slot=1;grant->generation=1;}return ok;
}
static bool text_custody_release(risc_runtime_capability_v1 *grant) {
 bool ok=test_release(grant);if(ok){grant->slot=grant->generation=0;}return ok;
}
static void text_scene_handoff(bool submitted) {
 (void)pqa_input(&quick.ui,ticks,true,0,0,0,0,true);assert(!pqa_capture(&quick.ui));
 quick_runtime.acquire=text_custody_acquire;quick_runtime.release=text_custody_release;
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x1357);
 if(submitted){present(false);assert(raster_progress());assert(raster_sealed);}
 else assert(raster_recording);
 int result=portable_text_adapter_suspend();if(result!=RISC_TEXT_ENTRY_OK)fprintf(stderr,"text suspend result=%d failed=%d modal=%d quick=%d visible=%d capture=%d settled=%d\n",result,failed,alarm_modal,quick_modal,pqa_visible(&quick.ui),pqa_capture(&quick.ui),display_settled);assert(result==RISC_TEXT_ENTRY_OK);
 assert(text_input_suspended&&!surface.frame&&!raster_sealed&&!raster_recording&&!raster_offscreen&&!raster_commands&&!paper_token);
 unsigned presents=displays;
 risc_display_surface_v1 host={0};assert(display->acquire(display->context,RISC_DISPLAY_FORMAT_RGB565,&host));
 for(unsigned y=0;y<host.height;y++)for(unsigned x=0;x<host.width;x++)((uint16_t*)((uint8_t*)host.pixels+y*host.stride_bytes))[x]=0x2468;
 assert(paper_present_progress());assert(displays==presents);
 risc_display_present_token_v1 token;const risc_display_present_options_v1 options={0};
 assert(display->submit(display->context,host.frame,NULL,0,&options,&token));
 risc_display_present_status_v1 status;assert(display->present_status(display->context,token,&status)&&status.state==RISC_DISPLAY_PRESENT_COMPLETE);
 assert_image(0x2468);assert(portable_text_adapter_resume()==RISC_TEXT_ENTRY_OK);
 assert(!text_input_suspended);assert(portable_paper_frame_ready());clear_color(0x3579);present(false);
 assert(portable_paper_frame_drain());assert_image(0x3579);cleanup();
}
#endif
int main(int argc,char **argv) {
 assert(argc==2);const char *name=argv[1];
 if(!strcmp(name,"failed-cleanup")) {
  char *args[]={argv[0],"9",NULL};return base_alarm_fixture_main(2,args);
 }
 if(!strcmp(name,"quick-real-open-close")) {
  char *args[]={argv[0],"0",NULL};return raster_base_quick_main(2,args);
 }
 setup();
#ifdef PORTABLE_TEXT_INPUT_CLIENT
 if(!strcmp(name,"text-scene-pending")){text_scene_handoff(true);puts("text pending scene handoff PASS");return 0;}
 if(!strcmp(name,"text-scene-unsubmitted")){text_scene_handoff(false);puts("text unsubmitted scene handoff PASS");return 0;}
#endif
 if(!strcmp(name,"no-op-settle"))unchanged_snapshot();
 else if(!strcmp(name,"non-cooperative-clear"))immediate_during_snapshot();
 else if(!strcmp(name,"sticky-direct-restore"))sticky_restore();
 else if(!strcmp(name,"quick-direct-overlay"))direct_quick_overlay();
 else if(!strcmp(name,"quick-interrupted"))interrupted_quick_snapshot(false);
 else if(!strcmp(name,"quick-interrupted-partial"))interrupted_quick_snapshot(true);
 else if(!strcmp(name,"quick-action-partial"))quick_action_during_snapshot();
 else if(!strcmp(name,"bitmap-allocation-acquire-failure"))bitmap_allocation_and_acquire_failure();
 else if(!strcmp(name,"aborted-recording-cleanup"))failed_snapshot_cleanup(false);
 else if(!strcmp(name,"failed-partial-cleanup"))failed_snapshot_cleanup(true);
 else if(!strcmp(name,"offscreen-allocation-recovery"))offscreen_allocation_recovery();
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
 else if(!strcmp(name,"terminal-band-restore"))terminal_band_restore();
#endif
 else assert(!"unknown raster custody case");
 printf("raster custody %s PASS\n",name);return 0;
}
#endif
