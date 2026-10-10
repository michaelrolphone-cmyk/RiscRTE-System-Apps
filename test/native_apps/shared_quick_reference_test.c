/* Production Runtime/Graph/ELFs; deterministic display, input and storage.
 * Reuse strict actual-System peripheral callbacks without replacing Runtime. */
#ifdef POLICY_PROVIDER
#include "resident_system_test.c"
#else
#define policy_fixture_api reference_base_api
#define policy_fixture_setup reference_base_setup
#define policy_fixture_verify reference_base_verify
#include "resident_system_test.c"
#undef policy_fixture_api
#undef policy_fixture_setup
#undef policy_fixture_verify
#include "shared_quick_reference.h"
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
 if(event==14){assert(terminal);++retained_returns[kind];return;}
 if(event==10){lifecycle_role=kind?2:1;if(kind)assert(!focus&&!subs[2]&&!frame&&!pending_present);++app_maps[kind];return;}
 if(event==11){++app_unmaps[kind];if(kind)lifecycle_role=1;return;}
 if(event==12){++app_entries[kind];if(kind){active_kind=kind;client_since=ms;}return;}
 assert(event==13);++app_returns[kind];if(kind){assert(!frame&&!pending_present);active_kind=0;child_return_at=ms;}return;
}
static bool ref_health(risc_runtime_health_v1*out){live();assert(ms<12000);out->uptime_ms=ms;if(finished&&role()==1&&finish_home)finish_home();return true;}
static bool ref_info(void*c,risc_display_info_v1*out){bool okay=display_info(c,out);if(contains("no-clean"))out->flags&=~RISC_DISPLAY_INFO_CLEAN_PRESENT;return okay;}
static bool ref_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){
 reference_ui state=ui();
 if(role()==1&&state.modal&&o->intent==RISC_DISPLAY_PRESENT_CLEAN&&contains("submit-refused")){live();assert(f==1&&frame==1&&!n);++clean_frames;reference_terminal();return false;}
 bool okay=submit_frame(c,f,r,n,o,out);memcpy(submitted_pixels,pixels,sizeof(pixels));submitted_ui=state;
 if(role()==1&&state.modal){++overlay_frames;if(!observed_open){observed_open=true;open_since=ms;}
  if(o->intent==RISC_DISPLAY_PRESENT_CLEAN){assert(!n&&contains("clean")&&!contains("no-clean"));++clean_frames;clean_token=*out;}
 }
 if(role()==1&&!state.modal&&app_returns[1]+app_returns[2])++host_restored;
 return okay;
}
static bool ref_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 assert(!memcmp(submitted_pixels,pixels,sizeof(pixels)));
 if(t==clean_token&&contains("status-refused")){live();assert(pending_present==t);reference_terminal();return false;}
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
static bool ref_brightness(void*c,uint16_t value,uint16_t max){bright_level=value;return brightness(c,value,max);}
static void ref_progress(void){
 reference_ui state=ui();
 if(role()!=1)return;
 if(active_kind==0&&app_returns[1]+app_returns[2]&&host_restored&&!state.modal)finished=true;
 if(!child_mode()&&!contains("usb")&&closing&&!state.modal&&!pending_present)finished=true;
}
static bool ref_nav(void*c,risc_input_navigation_frame_v1*out){(void)c;live();assert(focus==role());*out=(risc_input_navigation_frame_v1){0};ref_progress();
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
static bool ref_touch(void*c,risc_touch_snapshot_v1*out){(void)c;live();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 reference_ui state=ui();unsigned since=ms-(child_mode()?client_since:0);
 bool opening=!contains("home-closed")&&(child_mode()?active_kind==1:true)&&!observed_open;
 if(opening&&since>=80){if(since<120)contact(out,3,240,20);else if(since<200)contact(out,3,240,160);}
 if(role()!=1||!state.modal||!observed_open||ms-open_since<400||contains("home-open"))return true;
 if(!action_since)action_since=ms;
 unsigned at=ms-action_since;int x=0,y=0;
 if(contains("frontlight")){
  unsigned target=at/240;assert(state.modal);
  if(target>action_stage&&target<=2){unsigned saved=101;assert(pqa_preference_load(&preferences,PQA_BRIGHTNESS_KEY,40,0,&saved));unsigned expected=target==1?0:40;assert(state.brightness==expected&&bright_level==expected&&saved==expected);action_stage=target;}
  if(target<3){unsigned control=target<2?5:100;assert(point(control,&x,&y));if(at%240<50)contact(out,5+target,x,y);}
  else {assert(state.brightness==100&&bright_level==100&&brightness_writes>=3);
   unsigned saved=0;assert(pqa_preference_load(&preferences,PQA_BRIGHTNESS_KEY,40,0,&saved)&&saved==100);
   if(!captured_frontlight){assert(!pending_present&&completed_ui.modal&&completed_ui.position==240u*256u&&completed_ui.brightness==100);capture_completed("frontlight-persisted");captured_frontlight=true;}actions_open=3;closing=true;}
 }else if(contains("no-clean")){
  assert(!state.clean&&!point(8,&x,&y));assert(!clean_frames);closing=true;
 }else if(contains("clean")){
  assert(state.clean&&point(8,&x,&y));if(at<50)contact(out,5,x,y);
  else if(at>=240){assert(clean_frames==1&&clean_completed==1&&state.modal);actions_open=1;closing=true;}
 }else if(contains("usb")){
  assert(point(7,&x,&y));if(at<50)contact(out,5,x,y);
 }else if(at>100)closing=true;
 return true;
}
static int32_t ref_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*used){return kv_get(c,k,out,cap,used);}
static int32_t ref_put(void*c,const char*k,const void*data,uint32_t n){if(!strcmp(k,PQA_BRIGHTNESS_KEY)){assert(n==1);++brightness_writes;}return kv_put(c,k,data,n);}
/* Runtime's namespace backend enters these shared test functions directly. */
const void *policy_fixture_api(unsigned index){
 if(index==1){static risc_display_output_api_v1_snapshot table;table=display;table.metrics.power.history.base.get_info=ref_info;table.metrics.power.history.base.submit=ref_submit;table.metrics.power.history.base.present_status=ref_status;table.metrics.power.history.base.set_brightness=ref_brightness;return &table;}
 if(index==2){static risc_touch_api_v1 table;table=touch;table.snapshot=ref_touch;return &table;}
 if(index==6&&contains("audio")){static alarm_service_descriptor_v2 table;table=alarm;table.output_modes=ALARM_MODE_SOUND|ALARM_MODE_VISUAL;return &table;}
 if(index==3){static risc_input_navigation_api_v1 table;table=navigation;table.poll=ref_nav;return &table;}
 return reference_base_api(index);
}
void policy_fixture_setup(const char*name){mode=name;lifecycle_role=1;}
void policy_fixture_verify(bool was_retained){
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
 if(contains("usb"))assert(app_entries[2]==1&&app_returns[2]==1&&app_unmaps[2]==1);
 if(contains("home-closed"))assert(!overlay_frames);
 else assert(overlay_frames&&observed_open);
 if(getenv("REFERENCE_CAPTURE_DIR")&&!contains("home-closed"))assert(captured_open);
 if(contains("frontlight")){assert(captured_frontlight);assert(actions_open==3&&bright_level==100);unsigned b=0;assert(pqa_preference_load(&preferences,PQA_BRIGHTNESS_KEY,40,0,&b)&&b==100);}
 if(contains("clean")&&!contains("no-clean"))assert(clean_frames==1&&clean_completed==1);
 printf("Real Paper Clock + Files/USB + Runtime/Graph %s: host=%u files=%u usb=%u overlays=%u clean=%u/%u restored=%u PASS\n",mode,app_entries[0],app_entries[1],app_entries[2],overlay_frames,clean_frames,clean_completed,host_restored);
}
/* These are used by the generated Runtime harness to preserve its real binding
 * and lifecycle machinery while observing fixture-only hardware callbacks. */
bool reference_health(risc_runtime_health_v1*out){return ref_health(out);}
int32_t reference_kv_get(uint32_t ns,const char*k,void*out,uint32_t cap,uint32_t*used){assert(ns==1||ns==6);return ref_get(NULL,k,out,cap,used);}
int32_t reference_kv_put(uint32_t ns,const char*k,const void*data,uint32_t n){assert(ns==1||ns==6);return ref_put(NULL,k,data,n);}
#endif
