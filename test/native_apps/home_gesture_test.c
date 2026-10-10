/* Production sparse Home + raw-touch adapter; no synthetic Confirm fallback. */
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
#define TEST_SHARED_QUICK_REFERENCE
#endif
#define SPARSE_FIXTURE_MAIN unused_home_gesture_base_main
#include "sparse_clock_startup_test.c"
extern bool home_gesture_quick_visible(void);
extern void home_gesture_finish(void);
static const char *gesture_case,*gesture_expected;
static unsigned gesture_step,quick_seen,up_poll;
static bool last_contact;
static int origin_x,origin_y,delta_x,delta_y;
static bool is_case(const char *name){return !strcmp(gesture_case,name);}
static bool gesture_health(risc_runtime_health_v1 *out) {
 io();out->uptime_ms=ms;assert(polls<250);return true;
}
static bool gesture_nav(void *c,risc_input_navigation_frame_v1 *out) {
 (void)c;safe();*out=(risc_input_navigation_frame_v1){0};
 if(!strcmp(gesture_expected,"quick")&&gesture_step==70)out->pressed=out->released=RISC_NAV_BACK;
 return true;
}
static bool gesture_obtain(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out) {
 bool ok=obtain(name,version,id,out);
 if(ok&&!strcmp(name,"input.navigation")) {
  static risc_input_navigation_api_v1 quiet;quiet=navigation;quiet.poll=gesture_nav;out->api=&quiet;
 }
 return ok;
}
static void gesture_contact(unsigned step,bool *down,int *x,int *y) {
 *down=step>=3&&step<=5;*x=origin_x;*y=origin_y;
 if(is_case("held")){*down=step<=5;}
 if(step>=4){*x+=delta_x;*y+=delta_y;}
 if(is_case("return")){if(step==4)*x+=20;}
 if(is_case("short")){if(step>=4)*x+=20;}
 if(is_case("release")){*down=step==3;if(step==3){*x=origin_x;*y=origin_y;}}
 if(is_case("repeat")&&step>=40){*down=step<=42;*x=240;*y=400;}
 /* Dismiss the opened sheet, then verify a new Points tap is independent. */
 if(is_case("quick-return")) {
  if(step>=45){*down=step==45;*x=240;*y=675;}
  if(step>=75){*down=step<=77;*x=240;*y=400;}
 }
}
static int32_t gesture_next(void *c,uint64_t sub,risc_touch_event_v1 *out) {
 (void)c;assert(sub==1);
 bool down;int x,y;gesture_contact(gesture_step+1,&down,&x,&y);
 if(home_idle_saved && last_contact && !down && up_poll!=polls) {
  up_poll=polls;*out=(risc_touch_event_v1){.kind=RISC_TOUCH_EVENT_UP,.id=1,.x=(uint16_t)x,.y=(uint16_t)y};return 1;
 }
 return 0;
}
static bool gesture_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(!home_idle_saved) {
  if(is_case("held")){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)origin_x,.y=(uint16_t)origin_y};}
  return true;
 }
 bool down;int x,y;gesture_contact(++gesture_step,&down,&x,&y);
 if(gesture_step==100)home_gesture_finish();
 if(home_gesture_quick_visible())quick_seen++;
 if(is_case("cancel")&&gesture_step==4){out->contact_count=2;last_contact=false;return true;}
 last_contact=down;
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
 return true;
}
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
static bool __attribute__((unused)) reference_sparse_ready(void){return true;}
static bool reference_sparse_touch(void *c,risc_touch_snapshot_v1 *out){return gesture_snapshot(c,out);}
#endif
int main(int argc,char **argv) {
 assert(argc==7);gesture_case=argv[1];origin_x=atoi(argv[2]);origin_y=atoi(argv[3]);delta_x=atoi(argv[4]);delta_y=atoi(argv[5]);
 const char *expected=argv[6];gesture_expected=expected;test="home-gesture";state_path="unused-home-gesture-state";
 raw_async=getenv("RAW_ASYNC")!=NULL;epoch=1791369720;native_valid=true;
 runtime.health=gesture_health;runtime.acquire=gesture_obtain;
 panel.history.base=d;panel.history.base.get_info=raw_display_info;
 if(raw_async)panel.history.base.present_status=raw_present_status;
 panel.history.base.struct_size=sizeof(panel);panel.history.base.acquire=frame_acquire;
 panel.history.base.release=frame_release;panel.history.base.submit=frame_submit;
 panel.history.base.wait_present=frame_wait;panel.history.base.set_brightness=bright;
 panel.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;panel.history.extension_version=1;panel.history.seed_previous=seed_previous;
 panel.power_tag=RISC_DISPLAY_POWER_TAG;panel.power_version=1;panel.prepare=panel_prepare;panel.resume=panel_resume;
 touch_power.base=t;touch_power.base.snapshot=gesture_snapshot;touch_power.base.next=gesture_next;
 touch_power.base.struct_size=sizeof(touch_power);touch_power.power_tag=RISC_TOUCH_POWER_TAG;touch_power.power_version=1;
 touch_power.prepare=touch_prepare;touch_power.resume=touch_resume;
 sd.terminal.power.volume.base=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(sd)};
 sd.terminal.extension_tag=RISC_STORAGE_POWER_COMMIT_TAG;sd.terminal.extension_version=1;sd.terminal.commit_power_down=legacy_sd;
 sd.sleep_tag=RISC_STORAGE_SLEEP_TAG;sd.sleep_version=1;sd.prepare_sleep=prepare_sd;sd.commit_sleep=commit_sd;sd.resume_sleep=resume_sd;
 power=(x4_power_deep_v1){{1,sizeof(power),NULL,read_key,NULL},X4_POWER_DEEP_TAG,1,deep};
 assert(app_module_init()==0);in_main=true;start_ms=ms;app_main();
 assert(!terminal&&!entries&&!portable_app_sleep_retained());
 bool quick_open=home_gesture_quick_visible();app_module_fini();
 assert(!grants&&!subs&&!frames&&!native_live&&!pending&&!panel_off&&!touch_off&&!sd_off);
 if(!strcmp(expected,"quick")){assert(!launches&&quick_seen&&!quick_open);}
 else if(!strcmp(expected,"none")){assert(!launches&&!quick_seen);}
 else {assert(launches==1&&!strcmp(launched,expected));if(is_case("quick-return"))assert(quick_seen&&!quick_open);else assert(!quick_seen);}
 if(!is_case("tap")&&!is_case("repeat")&&!is_case("quick-return"))assert(!(home_feedback_bits&(1u<<3)));
 assert(!(home_feedback_bits&(1u<<2))); /* Time is informational, never a button. */
 if(is_case("tap")||is_case("repeat")||is_case("quick-return")) {
  assert(home_feedback_bits&(1u<<3));
  assert(gesture_step>=(is_case("quick-return")?78u:is_case("repeat")?43u:6u));
 }
 printf("Home %s (%d,%d) delta=(%d,%d) => %s; async=%u PASS\n",gesture_case,origin_x,origin_y,delta_x,delta_y,expected,raw_async);
 return 0;
}
