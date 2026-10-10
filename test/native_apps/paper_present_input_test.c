/* Production launcher + production native adapter over a deterministic async
 * display. QUEUED, transfer and BUSY timing are fixtures, not device measures. */
#define TEST_CUSTOM_CATALOG
#define main toolbar_fixture_main
#include "portable_native_toolbar_test.c"
#undef main
#include "RiscInputNavigationV1.h"
extern unsigned paper_test_selected(void);
#ifndef TEST_APP_COMPATIBLE
#define TEST_APP_COMPATIBLE false
#endif
const t5_app_manifest_t portable_catalog[3]={
 {.display_name="ZERO",.file_name="zero.elf",.icon="solid:f013",.compatible=TEST_APP_COMPATIBLE},
 {.display_name="ONE",.file_name="one.elf",.icon="solid:f013",.compatible=TEST_APP_COMPATIBLE},
 {.display_name="TWO",.file_name="two.elf",.icon="solid:f013",.compatible=TEST_APP_COMPATIBLE}};
const unsigned portable_catalog_count=3;
static const char *case_name;
static bool mode(const char *name){return !strcmp(case_name,name);}
static uint32_t pending_token,submitted_at,first_detected,first_dispatch,first_visible;
static unsigned selected_seen,changes,launches,completed,modal_opens,alarm_acks,provider_steps,first_completed_at;
static unsigned submit_selected[16],submit_at[16],complete_at[16],status_polls,states[3],launch_at[4];
static risc_display_rect_v1 submitted_damage[16];
static uint8_t frozen[sizeof(pixels)],completed_pixels[sizeof(pixels)];
static bool alarm_due,alarm_done,launch_refused,async_enabled=true;
static bool custom_info(void *c,risc_display_info_v1 *out) {
 fx_info(c,out);if(async_enabled)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 out->damage_x_alignment=8;out->damage_width_alignment=8;return true;
}
static void immutable(void){if(pending_token)assert(!memcmp(frozen,pixels,sizeof(pixels)));}
static void observe_state(void) {
 unsigned value=paper_test_selected();
 if(value!=selected_seen){if(!first_dispatch)first_dispatch=ticks;selected_seen=value;++changes;}
}
static bool custom_frame(void *c,uint32_t f,risc_display_surface_v1 *out) {
 assert(!pending_token);return fx_frame(c,f,out);
}
static void custom_release_frame(void *c,risc_display_frame_v1 f) {assert(!pending_token);fx_frame_release(c,f);}
static void capture_frame(unsigned index) {
 const char *directory=getenv("PAPER_PRESENT_CAPTURE_DIR");if(!directory)return;
 char path[768];snprintf(path,sizeof(path),"%s/frame-%u.pbm",directory,index);
 FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n800 480\n");
 assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));assert(!fclose(file));
}
static bool custom_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token) {
 assert(!pending_token&&presents<16);observe_state();
 /* Damage must cover every changed byte relative to the last completed image. */
 if(completed&&n){assert(n==1&&r&&o->intent!=RISC_DISPLAY_PRESENT_CLEAN);
  for(unsigned y=0;y<480;y++)for(unsigned x=0;x<100;x++)if(pixels[y*100+x]!=completed_pixels[y*100+x])
   assert((int)x*8>=r->x&&(int)y>=r->y&&x*8+8<=(unsigned)r->x+r->width&&y<(unsigned)r->y+r->height);
 }
 if(TEST_APP_COMPATIBLE && presents==1 && !mode("launch-quick") && !mode("launch-alarm")) {
  unsigned logical_x=110+130*paper_test_selected()+40;
  unsigned offset=(479-logical_x)*100+136/8;
  assert(!(completed_pixels[offset]&0x80)&&(pixels[offset]&0x80));
 }
 if(mode("launch-submit-false")&&presents==1)return false;
 if(n)submitted_damage[presents]=*r;
 submit_selected[presents]=paper_test_selected();submit_at[presents]=ticks;
 bool ok=fx_submit(c,f,r,n,o,token);assert(ok);capture_frame(presents);pending_token=*token;submitted_at=ticks;provider_steps=0;
 memcpy(frozen,pixels,sizeof(frozen));return true;
}
static bool custom_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
 (void)c;io();assert(pending_token==token);immutable();++status_polls;
 uint32_t elapsed=ticks-submitted_at;
 if((mode("status-false")||(mode("launch-status-false")&&presents==2))&&elapsed>=40)return false;
 if((mode("failed")||(mode("launch-failed")&&presents==2))&&elapsed>=100){out->state=RISC_DISPLAY_PRESENT_FAILED;return true;}
 if(mode("superseded")&&elapsed>=100){out->state=RISC_DISPLAY_PRESENT_SUPERSEDED;return true;}
 if(elapsed<40){out->state=RISC_DISPLAY_PRESENT_QUEUED;++states[0];}
 else if(elapsed<100){out->state=RISC_DISPLAY_PRESENT_ACTIVE;++states[1];}
 else if(elapsed<240||(mode("pump-count")&&provider_steps<240)||mode("timeout")||(mode("launch-timeout")&&presents==2)){out->state=RISC_DISPLAY_PRESENT_ACTIVE;++states[2];}
 else {
  out->state=RISC_DISPLAY_PRESENT_COMPLETE;pending_token=0;++completed;complete_at[presents-1]=ticks;if(!first_completed_at)first_completed_at=ticks;
  memcpy(completed_pixels,pixels,sizeof(completed_pixels));
  if(submit_selected[presents-1]==1&&presents>1&&!first_visible&&!quick_modal&&!alarm_modal)first_visible=ticks;
 }
 return true;
}
static bool custom_touch(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;io();immutable();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 int x=240,y=136;bool down=false;
 if(mode("latency")||mode("sync")||mode("quick")||mode("alarm"))down=ticks>=40&&ticks<60;
 if(mode("repeat")){
  if(ticks>=40&&ticks<60){down=true;x=240;}
  if(ticks>=80&&ticks<100){down=true;x=370;}
  if(ticks>=120&&ticks<180){down=true;x=ticks<140?370:280;y=300;}
  if(ticks>=200&&ticks<220){down=true;x=240;}
 }
 if(mode("quick")||mode("launch-quick")){
  if(ticks>=100&&ticks<140){down=true;x=200;y=ticks<120?20:100;}
  if(ticks>=300&&ticks<320){down=true;x=340;y=345;}
  if(ticks>=800&&ticks<820){down=true;x=240;y=675;}
 }
 if((mode("alarm")||mode("launch-alarm"))&&ticks>=540&&ticks<560){down=true;x=240;y=650;}
 if(!strncmp(case_name,"launch-",7)) {
  if(ticks>=40&&ticks<(mode("launch-held")?220u:60u)){down=true;x=mode("launch-same")?110:240;}
  if(mode("launch-retry")&&ticks>=600&&ticks<620){down=true;x=370;}
  if(mode("launch-replace")&&ticks>=300&&ticks<320){down=true;x=370;}
  if(mode("launch-cancel")&&ticks>=300&&ticks<380){down=true;x=ticks<340?240:370;y=300;}
  if(mode("launch-drag")&&ticks>=60&&ticks<80){down=true;x=370;y=300;}
  if(mode("launch-multi")&&ticks>=60&&ticks<80){out->contact_count=2;return true;}
  if(mode("launch-held-entry")&&ticks<220){down=true;x=240;}
 }
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};if(!first_detected)first_detected=ticks;}
 return true;
}
static bool custom_nav(void *c,risc_input_navigation_frame_v1 *out) {
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};
 unsigned stop=mode("quick")||mode("alarm")?1500:!strncmp(case_name,"launch-",7)?1500:mode("exit-busy")?100:600;
 if(ticks>=stop)out->buttons=out->pressed=RISC_NAV_BACK;
 return true;
}
static bool custom_foreground(void *c,const risc_input_foreground_v1 *claims,size_t count) {(void)c;(void)claims;(void)count;io();return true;}
static bool custom_reset(void *c){(void)c;io();return true;}
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,custom_nav,custom_foreground,custom_reset};
static int32_t custom_alarm_status(void *c,alarm_status_v1 *out) {
 assert(!pending_token);fx_alarm_status(c,out);
 if(alarm_due){out->state=ALARM_STATE_ALERT;out->occurrence=(alarm_token_v1){1,1,1,1};}
 return ALARM_OK;
}
static int32_t custom_alarm_step(void *c) {
 assert(!pending_token);fx_alarm_step(c);
 if((mode("alarm")||mode("launch-alarm"))&&ticks>=100&&!alarm_done)alarm_due=true;
 return ALARM_OK;
}
static int32_t custom_alarm_ack(void *c,const alarm_token_v1 *token) {
 assert(alarm_due&&!pending_token);fx_alarm_ack(c,token);alarm_due=false;alarm_done=true;++alarm_acks;return ALARM_OK;
}
static risc_display_output_api_v1 custom_display;
static risc_touch_api_v1 custom_touch_api;
static alarm_service_v1 custom_alarm;
static bool custom_acquire(const char *name,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *grant) {
 if(!strcmp(name,"input.navigation")){
  io();++acquires;++live;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant),.slot=acquires,.generation=1,.api=&nav};return true;
 }
 bool ok=fx_acquire(name,v,instance,grant);
 if(!strcmp(name,"display.output"))grant->api=&custom_display;
 if(!strcmp(name,"input.touch.raw"))grant->api=&custom_touch_api;
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY))grant->api=&custom_alarm;
 return ok;
}
static bool custom_release(risc_runtime_capability_v1 *grant) {
 assert(!pending_token);return fx_release(grant);
}
static bool custom_launch(const char *path) {
 io();assert(!pending_token&&launches<4);launch_at[launches++]=ticks;
 if(mode("launch-retry")){if(!launch_refused){assert(!strcmp(path,"one.elf"));launch_refused=true;return false;}assert(!strcmp(path,"two.elf"));return true;}
 if(!strncmp(case_name,"launch-",7)) {
  assert(!strcmp(path,mode("launch-same")?"zero.elf":mode("launch-replace")?"two.elf":"one.elf"));return true;
 }
 return false;
}
static void custom_yield(uint32_t delay) {
 immutable();observe_state();assert(ticks<12000);if(pending_token)++provider_steps;
 if(mode("stalled-clock"))io();else fx_yield(delay);
 if(quick_modal&&!modal_opens)++modal_opens;
}
static void paint_fixture(void) {
 const paper_presentation *v=paper_presentation_get();assert(v);
#ifndef TEST_BASELINE
 assert(portable_paper_frame_ready());
#endif
 v->begin();v->text(20,20,300,"INITIAL",1,false,true);present(false);
}
#ifndef PAPER_FIXTURE_MAIN
#define PAPER_FIXTURE_MAIN main
#endif
int PAPER_FIXTURE_MAIN(int argc,char **argv) {
 assert(argc==2);case_name=argv[1];async_enabled=!mode("sync");
 custom_display=fx_display;custom_display.get_info=custom_info;custom_display.acquire=custom_frame;
 custom_display.release=custom_release_frame;custom_display.submit=custom_submit;custom_display.present_status=custom_status;
 custom_touch_api=fx_touch;custom_touch_api.snapshot=custom_touch;
 custom_alarm=fx_alarm;custom_alarm.status=custom_alarm_status;custom_alarm.step=custom_alarm_step;custom_alarm.acknowledge=custom_alarm_ack;
 fx_runtime.acquire=custom_acquire;fx_runtime.release=custom_release;fx_runtime.request_launch=custom_launch;fx_runtime.yield_ms=custom_yield;
 assert(!app_module_init());
 const paper_presentation *saved_view=paper_presentation_get();assert(saved_view);
 if(mode("fini")||mode("fini-timeout")||mode("fallback")){
  if(mode("fini-timeout"))case_name="timeout";
  paint_fixture();
  if(mode("fallback")){clear();assert(completed==1&&frames==1);present(false);}
  app_module_fini();
 }
#ifndef TEST_BASELINE
 else if(mode("stalled-clock")) {
  paint_fixture();t5_app_input_t input={0};assert(poll(&input,20));
  assert(!ticks&&provider_steps==20&&pending_token);
  assert(!portable_paper_frame_drain()&&retained);
 }
#endif
 else {app_main();if(!retained)assert(!pending_token);}
 observe_state();
 if(!retained)app_module_fini();
 if(retained){assert(barriers==1&&(pending_token||frames));check_retained(saved_view);}
 else assert(!pending_token&&!live&&!frames&&!subscriptions);
 if(mode("latency")){
  assert(first_detected>=40&&first_detected<60&&first_dispatch&&first_visible&&completed==2);
#ifdef TEST_BASELINE
  assert(first_dispatch==241&&first_visible==481);
#else
  assert(first_dispatch==60&&first_visible==480&&submit_at[1]==240&&states[0]&&states[1]&&states[2]);
#endif
 }
 if(mode("repeat")){assert(changes==3&&selected_seen==1&&presents==2&&submit_selected[1]==1);}
 if(mode("sync")){assert(first_dispatch==241&&first_visible==481);}
 if(mode("quick")){assert(modal_opens==1&&kv_writes==1&&dnd_value&&selected_seen==1&&submit_selected[presents-1]==1);}
 if(mode("alarm")){assert(alarm_acks==1&&selected_seen==1&&submit_selected[presents-1]==1);}
 if(mode("launch-retry")){assert(launches==2&&launch_refused&&selected_seen==2&&launch_at[0]==480&&launch_at[1]>=980);}
 if(mode("launch-feedback")||mode("launch-same")||mode("launch-held")) {
  assert(launches==1&&launch_at[0]==480&&launch_at[0]==complete_at[1]&&presents==2&&submit_at[1]==240);
  assert(submitted_damage[1].width&&submitted_damage[1].height&&submitted_damage[1].width*submitted_damage[1].height<800*480/10);
 }
 if(mode("launch-replace"))assert(launches==1&&launch_at[0]==720&&presents==3&&submit_selected[2]==2);
 if(mode("launch-cancel"))assert(!launches&&presents==3&&selected_seen==1);
 if(mode("launch-quick"))assert(!launches&&modal_opens==1&&selected_seen==1);
 if(mode("launch-alarm"))assert(!launches&&alarm_acks==1&&selected_seen==1);
 if(mode("launch-drag")||mode("launch-multi")||mode("launch-held-entry"))assert(!launches&&presents==1);
 if(mode("exit-busy"))assert(ticks==240&&completed==1);
 if(mode("pump-count"))assert(first_completed_at==240&&provider_steps==240&&completed==1);
 if(mode("status-false")||mode("failed")||mode("superseded")||mode("timeout"))assert(retained);
 if(mode("launch-failed")||mode("launch-status-false")||mode("launch-submit-false")||mode("launch-timeout"))assert(retained&&!launches);
 printf("{\"case\":\"%s\",\"feedback_damage\":[%d,%d,%u,%u],\"first_launch_ms\":%u,\"feedback_completed_ms\":%u,\"first_completed_ms\":%u,\"provider_steps\":%u,\"detected_ms\":%u,\"dispatch_ms\":%u,\"first_visible_ms\":%u,\"frames\":%u,\"completed\":%u,\"state_changes\":%u,\"status_polls\":%u,\"launches\":%u,\"retained\":%s}\n",argv[1],submitted_damage[1].x,submitted_damage[1].y,submitted_damage[1].width,submitted_damage[1].height,launch_at[0],complete_at[1],first_completed_at,provider_steps,first_detected,first_dispatch,first_visible,presents,completed,changes,status_polls,launches,retained?"true":"false");
 return 0;
}
