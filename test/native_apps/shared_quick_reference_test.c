/* Production Runtime/Graph/ELFs; deterministic display, input and storage.
 * Reuse strict actual-System peripheral callbacks without replacing Runtime. */
#ifdef POLICY_PROVIDER
#include "resident_system_test.c"
#else
#define portable_app_idle_sleep_with_ui reference_base_idle_sleep_with_ui
#define policy_fixture_log reference_base_log
#define policy_fixture_api reference_base_api
#define policy_fixture_setup reference_base_setup
#define policy_fixture_verify reference_base_verify
#include "resident_system_test.c"
#undef portable_app_idle_sleep_with_ui
#undef policy_fixture_log
#undef policy_fixture_api
#undef policy_fixture_setup
#undef policy_fixture_verify
#include "shared_quick_reference.h"
#ifdef PORTABLE_FRONTLIGHT_TONE
#include "RiscDisplayOutputFrontlightV1.h"
static unsigned current_tone=50,tone_writes,tone_reads;
static bool captured_tone;
#endif
/* The selected Home now exposes this private helper boundary too. This
 * foreground-routing fixture must fail if either idle entry is reached. */
int portable_app_idle_sleep_with_ui(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,
 const risc_battery_gauge_api_v1*g,const alarm_service_v1*a,const portable_idle_sleep_ui*ui){
 (void)ui;return portable_app_idle_sleep(rt,d,g,a);
}
static reference_observe observe;
static reference_point point;
static void (*finish_home)(void);
static unsigned app_maps[3],app_unmaps[3],app_entries[3],app_returns[3];
static unsigned active_kind,client_since,open_since,action_since,action_stage,close_since;
static unsigned clean_frames,clean_completed,clean_token,overlay_frames,brightness_writes,bright_level=40;
static unsigned child_return_at,host_restored,actions_open,checkpoint_calls,resume_polls;
static bool launch_sent,home_sent,closing,finished,observed_open;
static uint8_t submitted_pixels[48000];
static reference_ui submitted_ui,completed_ui;
static bool captured_open,captured_frontlight;
static bool clean_live_failure;
static unsigned live_failure_logs,live_retain_logs;
extern bool reference_runtime_retained(void);
static unsigned reopen_since;
/* Additive stress profiles leave every original case and trigger unchanged. */
static unsigned pending_pull_mode,child_submissions,initial_child_token;
static unsigned pending_pull_polls,pending_pull_host_opens;
static bool pending_pull_move,pending_pull_release,pending_pull_up;
static unsigned retained_returns[3],terminal_calls,terminal_focus_changes,free_calls,terminal_free_calls;
void reference_free(void*p){assert(!terminal);++free_calls;free(p);}
static void capture_completed(const char*name){
 const char*directory=getenv("REFERENCE_CAPTURE_DIR");if(!directory)return;
 char path[1024];assert(snprintf(path,sizeof(path),"%s/%s.pbm",directory,name)>0);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P4\n480 800\n");assert(fwrite(completed,1,sizeof(completed),f)==sizeof(completed));assert(!fclose(f));
}
static void reference_terminal(void){terminal=true;terminal_calls=provider_calls;terminal_focus_changes=focus_changes;terminal_free_calls=free_calls;}
static bool child_mode(void){return !strncmp(mode,"child-",6);}
static bool contains(const char*s){return strstr(mode,s)!=NULL;}
static reference_ui ui(void){reference_ui out={0};if(observe)observe(&out);return out;}
void reference_hooks(reference_observe read,reference_point locate,void(*finish)(void)){observe=read;point=locate;finish_home=finish;}
const char *reference_mode(void){return mode;}
void reference_event(unsigned event,unsigned kind){
 assert(kind<3);
 if(event==14){if(clean_live_failure){assert(reference_runtime_retained());reference_terminal();}assert(terminal);++retained_returns[kind];return;}
 if(event==10){lifecycle_role=kind?2:1;if(kind)assert(!focus&&!subs[2]&&!frame&&!pending_present);++app_maps[kind];return;}
 if(event==11){++app_unmaps[kind];if(kind)lifecycle_role=1;return;}
 if(event==12){++app_entries[kind];if(kind){active_kind=kind;client_since=ms;if(kind==1)pending_pull_host_opens=opens[1];if(kind==2)home_sent=false;}return;}
 assert(event==13);++app_returns[kind];if(kind){assert(!frame&&!pending_present);active_kind=0;child_return_at=ms;}
 if(kind==2&&contains("repeat")&&app_returns[kind]<2){observed_open=false;action_since=0;open_since=0;reopen_since=ms;}
 return;
}
bool policy_fixture_log(const char *text){
 if(!strncmp(text,"APP ",4))assert(!reference_runtime_retained()&&!terminal);
 if(clean_live_failure&&!strncmp(text,"APP ",4)){if(strstr(text,"display-failed"))++live_failure_logs;if(strstr(text,"app-retain"))++live_retain_logs;}
 return reference_base_log(text);
}
static bool ref_health(risc_runtime_health_v1*out){assert(!reference_runtime_retained());live();assert(ms<12000);out->uptime_ms=ms;if(finished&&role()==1&&finish_home)finish_home();return true;}
static bool ref_info(void*c,risc_display_info_v1*out){bool okay=display_info(c,out);if(contains("no-clean"))out->flags&=~RISC_DISPLAY_INFO_CLEAN_PRESENT;if(contains("wait-refused"))out->flags&=~RISC_DISPLAY_INFO_ASYNC_PRESENT;return okay;}
static bool ref_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){
 reference_ui state=ui();
 if(role()==1&&state.modal&&o->intent==RISC_DISPLAY_PRESENT_CLEAN&&contains("submit-refused")){live();assert(f==1&&frame==1&&!n);++clean_frames;reference_terminal();return false;}
 bool okay=submit_frame(c,f,r,n,o,out);memcpy(submitted_pixels,pixels,sizeof(pixels));submitted_ui=state;
 if(role()==2&&active_kind==1){if(!child_submissions)initial_child_token=*out;++child_submissions;}
 if(role()==1&&state.modal){++overlay_frames;if(!observed_open){observed_open=true;open_since=ms;}
  if(o->intent==RISC_DISPLAY_PRESENT_CLEAN){assert(!n&&contains("clean")&&!contains("no-clean"));++clean_frames;clean_token=*out;}
  else assert(o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 }
 if(role()==1&&!state.modal&&app_returns[1]+app_returns[2])++host_restored;
 return okay;
}
static bool ref_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 assert(!memcmp(submitted_pixels,pixels,sizeof(pixels)));
 if(t==initial_child_token&&((pending_pull_mode==2&&ms-client_since<320)||(pending_pull_mode==3&&!pending_pull_release))){
  live();assert(pending_present==t);out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;
 }
 if(t==clean_token&&contains("status-refused")){live();assert(pending_present==t);reference_terminal();return false;}
 if(t==clean_token&&(contains("failed-live")||contains("superseded-live"))){
  live();assert(pending_present==t);clean_live_failure=true;out->state=contains("failed-live")?RISC_DISPLAY_PRESENT_FAILED:RISC_DISPLAY_PRESENT_SUPERSEDED;return true;
 }
 bool okay=present_status(c,t,out);
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){
  completed_ui=submitted_ui;
  if(t==clean_token){++clean_completed;clean_token=0;}
  if(submitted_ui.modal&&submitted_ui.position==240u*256u){
   if(!captured_open&&!action_since&&!submitted_ui.neutral){capture_completed("drawer-open");captured_open=true;}

  }
 }
 return okay;
}
static bool ref_wait(void*c,risc_display_present_token_v1 t,uint32_t timeout,risc_display_present_status_v1*out){
 assert(timeout);if(t==clean_token&&contains("wait-refused")){live();assert(pending_present==t);reference_terminal();return false;}return ref_status(c,t,out);
}
static bool ref_brightness(void*c,uint16_t value,uint16_t max){bright_level=value;return brightness(c,value,max);}
#ifdef PORTABLE_FRONTLIGHT_TONE
static int32_t ref_get_tone(void*c,uint16_t*value,uint16_t*maximum){
 (void)c;live();assert(!frame&&value&&maximum);++tone_reads;
 if(contains("tone-unavailable"))return RISC_DISPLAY_TONE_UNAVAILABLE;
 if(contains("tone-failed-get")||(contains("tone-failed-open-get")&&tone_reads>1)){reference_terminal();return RISC_DISPLAY_TONE_FAILED;}
 *value=(uint16_t)current_tone;*maximum=100;return RISC_DISPLAY_TONE_OK;
}
static bool ref_set_tone(void*c,uint16_t value,uint16_t maximum){
 (void)c;live();assert(!frame&&maximum==100&&value<=100);++tone_writes;
 if((value==100&&contains("tone-failed-set"))||(contains("tone-failed-open-set")&&tone_writes>1)){reference_terminal();return false;}
 current_tone=value;return true;
}
#endif
static void ref_progress(void){
 reference_ui state=ui();
 if(role()!=1)return;
 if(active_kind==0&&app_returns[1]+app_returns[2]&&host_restored&&!state.modal&&
    (!contains("repeat")||app_returns[2]==2))finished=true;
 if(!child_mode()&&!contains("usb")&&closing&&!state.modal&&!pending_present)finished=true;
}
static bool ref_touch_sample(void*,risc_touch_snapshot_v1*);
static bool ref_touch_validate;
static bool ref_nav(void*c,risc_input_navigation_frame_v1*out){(void)c;live();assert(focus==role());*out=(risc_input_navigation_frame_v1){0};ref_progress();
 if(pending_pull_mode&&role()==2&&active_kind==1&&pending_pull_move&&!observed_open){
  if(pending_pull_mode==1&&!child_submissions&&!frame&&!pending_present)++pending_pull_polls;
  if(pending_pull_mode>=2&&initial_child_token&&pending_present==initial_child_token){
   assert(focus==2&&opens[1]==pending_pull_host_opens&&!ui().modal);
   ++pending_pull_polls;
   /* The provider never completes by time. Release it only after proving the
    * owner keeps reducing input across a sustained, unchanged busy token. */
   if(pending_pull_mode==3&&pending_pull_polls>=128)pending_pull_release=true;
  }
 }
 /* Logical polling, not capture-only raster sampling, owns fixture storage
  * assertions and completed-image checks. Keep those checks non-reentrant. */
 if(!frame){risc_touch_snapshot_v1 ignored;ref_touch_validate=true;assert(ref_touch_sample(NULL,&ignored));ref_touch_validate=false;}
 if(role()==1&&!active_kind&&child_mode()&&!launch_sent&&ms>=80){out->pressed=RISC_NAV_CONFIRM;launch_sent=true;}
 if(role()==2&&active_kind==2&&!home_sent&&ms-client_since>=80){out->pressed=RISC_NAV_HOME;home_sent=true;}
 if(role()==2&&active_kind==1&&((closing&&!ui().modal&&++resume_polls>=2)||contains("home-closed"))&&!home_sent&&ms-client_since>=100){out->pressed=RISC_NAV_HOME;home_sent=true;}
 reference_ui state=ui();
 if(role()==1&&state.modal&&observed_open&&ms-open_since>=400){
  if(contains("home-open")&&!home_sent){out->pressed=RISC_NAV_HOME;home_sent=true;}
  if(closing&&!close_since){out->pressed=RISC_NAV_BACK;close_since=ms;}
 }
 return true;
}
static void contact(risc_touch_snapshot_v1*out,unsigned id,int x,int y){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};}
static bool ref_touch_sample(void*c,risc_touch_snapshot_v1*out){(void)c;live();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 reference_ui state=ui();unsigned since=ms-(child_mode()?client_since:reopen_since);
 bool opening=!contains("home-closed")&&(child_mode()?active_kind==1:true)&&!observed_open;
 unsigned down_at=pending_pull_mode==1?1:80,move_at=pending_pull_mode==1?2:120;
 if(opening&&since>=down_at){if(since<move_at)contact(out,3,240,20);else if(since<200)contact(out,3,240,160);}
 if(role()!=1||!state.modal||!observed_open||ms-open_since<400||contains("home-open"))return true;
 if(!action_since)action_since=ms;
 unsigned at=ms-action_since;int x=0,y=0;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(contains("tone")){
  if(contains("absent")||contains("unavailable")){assert(!state.tone_controls&&!tone_writes);closing=true;return true;}
  assert(state.tone_controls&&state.tone_valid);
  unsigned stage=at/240;
  if(ref_touch_validate&&stage>action_stage){unsigned saved=255;assert(pqa_preference_load(&preferences,PQA_TONE_KEY,50,0,&saved));
   unsigned expected=stage<=2?100:stage<=6?0:50;
   assert(state.tone==expected&&current_tone==expected&&saved==expected);
   if(stage==2||stage==3)assert(state.brightness==0&&bright_level==0);
   if(stage==4)assert(state.brightness==40&&bright_level==40);
   if(stage>=5)assert(state.brightness==100&&bright_level==100);
   action_stage=stage;
  }
  if(stage<7){
   unsigned control=stage==0?102:stage==1?5:stage==2?101:stage==3?5:stage==4?100:103;
   assert(point(control,&x,&y));
   if(at%240<50)contact(out,20+stage,x,y);
   else if(stage==5&&at%240<100){contact(out,20+stage,x,y);out->contact_count=2;out->contacts[1]=out->contacts[0];out->contacts[1].id++;}
  }else {
   if(!captured_tone&&ref_touch_validate){assert(!pending_present&&completed_ui.tone==50);capture_completed("tone-confirmed");captured_tone=true;}
   closing=true;
  }
 }else
#endif
 if(contains("frontlight")){
  unsigned target=at/240;assert(state.modal);
  if(ref_touch_validate&&target>action_stage&&target<=2){unsigned saved=101;assert(pqa_preference_load(&preferences,PQA_BRIGHTNESS_KEY,40,0,&saved));unsigned expected=target==1?0:40;assert(state.brightness==expected&&bright_level==expected&&saved==expected);action_stage=target;}
  if(target<3){unsigned control=target<2?5:100;assert(point(control,&x,&y));if(at%240<50)contact(out,5+target,x,y);}
  else if(ref_touch_validate){assert(state.brightness==100&&bright_level==100&&brightness_writes>=3);
   unsigned saved=0;assert(pqa_preference_load(&preferences,PQA_BRIGHTNESS_KEY,40,0,&saved)&&saved==100);
   if(!captured_frontlight){assert(!pending_present&&completed_ui.modal&&completed_ui.position==240u*256u&&completed_ui.brightness==100);capture_completed("frontlight-persisted");captured_frontlight=true;}actions_open=3;closing=true;}
 }else if(contains("no-clean")){
  assert(!state.clean&&!point(8,&x,&y));assert(!clean_frames);closing=true;
 }else if(contains("clean")){
  assert(state.clean&&point(8,&x,&y));if(at<50)contact(out,5,x,y);
  else if(at>=240){assert(clean_frames==1&&clean_completed==1&&state.modal);actions_open=1;closing=true;}
 }else if(contains("usb")){
  assert(point(7,&x,&y));assert(x==(state.audio?134:346)&&y==(state.audio?660:460));
  if(!state.audio){int hidden_x,hidden_y;assert(!point(0,&hidden_x,&hidden_y));}
  if(contains("cancel-then-open")&&at<480){
   assert(!app_entries[2]&&!usb_begins);unsigned cycle=at%240;
   if(cycle<50)contact(out,5+at/240,x,y);
   else if(cycle<100)contact(out,5+at/240,x+24,y);
   return true;
  }
  if(contains("old-row")&&at<240){
   assert(!state.audio&&!app_entries[2]&&!usb_begins);
   if(at<50)contact(out,5,440,550);return true;
  }
  unsigned delay=contains("cancel-then-open")?480:contains("old-row")?240:0;
  if(at-delay<50)contact(out,9,x,y);
 }else if(at>100)closing=true;
 return true;
}
/* Each Runtime owner has its own ordered raw subscription. The level script
 * supplies physical state; changing that state must also enqueue edges. A
 * snapshot and its sequence describe the same report, including resets and
 * inherited held contacts when the resident host takes over the drawer. */
typedef struct {
 risc_touch_snapshot_v1 snapshot;
 risc_touch_event_v1 events[RISC_TOUCH_QUEUE_LENGTH];
 unsigned head,count;
 bool initialized;
} reference_touch_stream;
static reference_touch_stream ref_touch_streams[3];
static int ref_touch_find(const risc_touch_snapshot_v1*s,uint8_t id){
 for(unsigned i=0;i<s->contact_count;i++)if(s->contacts[i].id==id)return (int)i;
 return -1;
}
static void ref_touch_edge(reference_touch_stream*s,unsigned kind,const risc_touch_contact_v1*p){
 assert(s->count<RISC_TOUCH_QUEUE_LENGTH);
 risc_touch_event_v1 event={.sequence=++s->snapshot.sequence,.timestamp_ms=ms,
  .kind=kind,.id=p->id,.x=p->x,.y=p->y};
 s->events[(s->head+s->count)%RISC_TOUCH_QUEUE_LENGTH]=event;++s->count;
 if(pending_pull_mode&&s==&ref_touch_streams[2]&&p->id==3&&kind==RISC_TOUCH_EVENT_MOVE){
  assert(!observed_open&&!frame);
  if(pending_pull_mode==1)assert(!child_submissions&&!pending_present);
  else assert(initial_child_token&&pending_present==initial_child_token);
  pending_pull_move=true;
 }
}
static bool ref_touch_update(void){
 unsigned owner=role();assert(owner>0&&owner<3&&subs[owner]);
 risc_touch_snapshot_v1 report={0};if(!ref_touch_sample(NULL,&report))return false;
 /* A poll services the same physical report for every live subscription. */
 for(unsigned id=1;id<3;++id){
 if(!subs[id])continue;
 reference_touch_stream*s=&ref_touch_streams[id];
 risc_touch_snapshot_v1 next=report;
 if(s->initialized){
  for(unsigned i=0;i<s->snapshot.contact_count;i++)
   if(ref_touch_find(&next,s->snapshot.contacts[i].id)<0)
    ref_touch_edge(s,RISC_TOUCH_EVENT_UP,&s->snapshot.contacts[i]);
  for(unsigned i=0;i<next.contact_count;i++){
   int prior=ref_touch_find(&s->snapshot,next.contacts[i].id);
   if(prior<0)ref_touch_edge(s,RISC_TOUCH_EVENT_DOWN,&next.contacts[i]);
   else if(s->snapshot.contacts[prior].x!=next.contacts[i].x||s->snapshot.contacts[prior].y!=next.contacts[i].y)
    ref_touch_edge(s,RISC_TOUCH_EVENT_MOVE,&next.contacts[i]);
  }
 }
 next.sequence=s->snapshot.sequence;next.timestamp_ms=ms;s->snapshot=next;s->initialized=true;
 }
 return true;
}
static uint64_t ref_subscribe(void*c){
 uint64_t id=subscribe(c);assert(id>0&&id<3);
 ref_touch_streams[id].head=ref_touch_streams[id].count=0;ref_touch_streams[id].initialized=false;
 return id;
}
static bool ref_unsubscribe(void*c,uint64_t id){
 bool okay=unsubscribe(c,id);
 if(okay){ref_touch_streams[id].head=ref_touch_streams[id].count=0;ref_touch_streams[id].initialized=false;}
 return okay;
}
static bool ref_poll_touch(void*c,size_t count){return poll_touch(c,count)&&ref_touch_update();}
static int32_t ref_next_touch(void*c,uint64_t id,risc_touch_event_v1*out){
 (void)c;live();assert(id==role()&&id>0&&id<3&&subs[id]);
 reference_touch_stream*s=&ref_touch_streams[id];if(!s->count)return 0;
 *out=s->events[s->head];s->head=(s->head+1)%RISC_TOUCH_QUEUE_LENGTH;--s->count;
 if(pending_pull_mode>=2&&id==2&&out->id==3&&out->kind==RISC_TOUCH_EVENT_UP){
  assert(initial_child_token&&pending_present==initial_child_token);
  assert(focus==2&&opens[1]==pending_pull_host_opens&&!ui().modal);pending_pull_up=true;
 }
 return 1;
}
static bool ref_touch(void*c,risc_touch_snapshot_v1*out){
 (void)c;if(!ref_touch_update())return false;*out=ref_touch_streams[role()].snapshot;return true;
}
static int32_t ref_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*used){return kv_get(c,k,out,cap,used);}
static int32_t ref_put(void*c,const char*k,const void*data,uint32_t n){if(!strcmp(k,PQA_BRIGHTNESS_KEY)){assert(n==1);++brightness_writes;}return kv_put(c,k,data,n);}
/* Runtime's namespace backend enters these shared test functions directly. */
const void *policy_fixture_api(unsigned index){
 if(index==1){static risc_display_output_api_v1_snapshot table;table=display;table.metrics.power.history.base.get_info=ref_info;table.metrics.power.history.base.submit=ref_submit;table.metrics.power.history.base.present_status=ref_status;if(contains("wait-refused"))table.metrics.power.history.base.wait_present=ref_wait;table.metrics.power.history.base.set_brightness=ref_brightness;
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(contains("tone")&&!contains("absent")){
   static risc_display_output_api_v1_frontlight ext;ext=(risc_display_output_api_v1_frontlight){.snapshot=table,.frontlight_tag=RISC_DISPLAY_FRONTLIGHT_TAG,.frontlight_version=1,.set_tone=ref_set_tone,.get_tone=ref_get_tone};
   ext.snapshot.metrics.power.history.base.struct_size=sizeof(ext);return &ext;
  }
#endif
  return &table;}
 if(index==2){static risc_touch_api_v1 table;table=touch;table.subscribe=ref_subscribe;table.unsubscribe=ref_unsubscribe;table.poll=ref_poll_touch;table.next=ref_next_touch;table.snapshot=ref_touch;return &table;}
 if(index==6&&contains("audio")){static alarm_service_descriptor_v2 table;table=alarm;table.output_modes=ALARM_MODE_SOUND|ALARM_MODE_VISUAL;return &table;}
 if(index==3){static risc_input_navigation_api_v1 table;table=navigation;table.poll=ref_nav;return &table;}
 return reference_base_api(index);
}
void policy_fixture_setup(const char*name){
 mode=name;lifecycle_role=1;
 const char*probe=getenv("REFERENCE_PENDING_PULL");
 if(probe){assert(!strcmp(name,"child-home-open"));
  if(!strcmp(probe,"software"))pending_pull_mode=1;
  else if(!strcmp(probe,"token"))pending_pull_mode=2;
  else {assert(!strcmp(probe,"busy"));pending_pull_mode=3;}
 }
}
void policy_fixture_verify(bool was_retained){
 if(pending_pull_mode){assert(pending_pull_move&&pending_pull_polls>=2);
  if(pending_pull_mode>=2)assert(pending_pull_up);
  if(pending_pull_mode==3)assert(pending_pull_release&&pending_pull_polls>=128);
  printf("Pending %s pull: ordered move and %u continuing logical polls PASS\n",pending_pull_mode==1?"software":pending_pull_mode==2?"provider-token":"persistently-busy-token",pending_pull_polls);
 }
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(contains("tone-failed")){
  assert(was_retained&&terminal&&!after_terminal&&provider_calls==terminal_calls&&focus_changes==terminal_focus_changes&&free_calls==terminal_free_calls);
  assert(!app_returns[0]&&!app_unmaps[0]);
  if(contains("tone-failed-get"))assert(!app_entries[0]&&!retained_returns[0]);
  else assert(app_entries[0]==1&&retained_returns[0]==1);
  printf("Real Paper Clock + Runtime/Graph %s: terminal tone failure, no later I/O/focus/free PASS\n",mode);return;
 }
#endif
 if(clean_live_failure){
  assert(was_retained&&terminal&&!after_terminal);
#ifdef TEST_STAGE_LOGS
  assert(live_failure_logs==1&&live_retain_logs==1);
#else
  assert(!live_failure_logs&&!live_retain_logs);
#endif
  assert(!app_unmaps[0]&&!app_unmaps[1]&&!app_unmaps[2]&&retained_returns[0]==1&&clean_frames==1&&!clean_completed);
  printf("Real Paper Clock + Runtime/Graph %s: live failure logged once; no post-retention I/O PASS\n",mode);return;
 }
 if(contains("refused")){
  assert(was_retained&&terminal&&!after_terminal&&provider_calls==terminal_calls&&focus_changes==terminal_focus_changes&&free_calls==terminal_free_calls&&focus==1);
  assert(clean_frames==1&&!clean_completed&&!app_unmaps[0]&&!app_unmaps[1]&&!app_unmaps[2]&&retained_returns[0]==1);
  assert(!app_returns[0]&&!app_returns[1]&&!app_returns[2]);
  assert(contains("submit-refused")?frame==1:pending_present!=0);
  if(child_mode())assert(app_entries[1]==1&&retained_returns[1]==1);
  printf("Real Paper Clock + Files + Runtime/Graph %s: terminal custody, no later provider/focus/free calls PASS\n",mode);return;
 }
 assert(!was_retained&&!terminal&&!frame&&!pending_present&&!focus&&!subs[1]&&!subs[2]);
 assert(app_entries[0]==1&&app_returns[0]==1&&app_maps[0]==1&&app_unmaps[0]==1&&finished);
 if(child_mode()){assert(app_entries[1]==1&&app_returns[1]==1&&app_unmaps[1]==1&&host_restored);}
 if(contains("usb")){unsigned expected=contains("repeat")?2:1;
  assert(app_entries[2]==expected&&app_returns[2]==expected&&app_unmaps[2]==expected);
  /* The tile opens the existing transfer app; its explicit Start remains
   * separate, so entry/dismissal must never acquire the USB device. */
  assert(!usb_begins&&!usb_ends&&!usb_owned);
 }
 if(contains("home-closed"))assert(!overlay_frames);
 else assert(overlay_frames&&observed_open);
 if(getenv("REFERENCE_CAPTURE_DIR")&&!contains("home-closed"))assert(captured_open);
 if(contains("frontlight")){assert(captured_frontlight);assert(actions_open==3&&bright_level==100);unsigned b=0;assert(pqa_preference_load(&preferences,PQA_BRIGHTNESS_KEY,40,0,&b)&&b==100);}
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(contains("tone")&&!contains("absent")&&!contains("unavailable")){assert(captured_tone&&tone_writes&&current_tone==50&&bright_level==100);}
#endif
 if(contains("clean")&&!contains("no-clean"))assert(clean_frames==1&&clean_completed==1);
 printf("Real Paper Clock + Files/USB + Runtime/Graph %s: host=%u files=%u usb=%u overlays=%u clean=%u/%u restored=%u PASS\n",mode,app_entries[0],app_entries[1],app_entries[2],overlay_frames,clean_frames,clean_completed,host_restored);
}
/* These are used by the generated Runtime harness to preserve its real binding
 * and lifecycle machinery while observing fixture-only hardware callbacks. */
bool reference_health(risc_runtime_health_v1*out){return ref_health(out);}
int32_t reference_kv_get(uint32_t ns,const char*k,void*out,uint32_t cap,uint32_t*used){assert(ns==1||ns==6);return ref_get(NULL,k,out,cap,used);}
int32_t reference_kv_put(uint32_t ns,const char*k,const void*data,uint32_t n){assert(ns==1||ns==6);return ref_put(NULL,k,data,n);}
#endif
