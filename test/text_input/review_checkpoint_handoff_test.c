/* Actual scene/text hosts. Opt-in same-focus policy preserves queued input;
 * real focus/sleep changes deliberately neutralize inherited input. */
#define main unused_scene_transport_main
#include "../scene/host_test.c"
#undef main
#include "RiscTextEntryV1.h"
#include "RiscSceneCheckpointV1.h"
extern const risc_driver_v2 *review_text_driver_get(uint32_t);
extern bool review_target(unsigned,int*,int*);
extern uint64_t review_scene_session(void);
static unsigned nav_claims,nav_resets;
static bool overflow_transport;
static unsigned overflow_at;
static uint64_t overflow_start;
static uint16_t overflow_x,overflow_y;
static int32_t review_next(void*c,uint64_t s,risc_touch_event_v1*out){
 if(!overflow_transport)return next(c,s,out);
 (void)c;io("next");assert(subscribed&&s==42);
 if(overflow_at==70)return 0;
 *out=(risc_touch_event_v1){overflow_start+overflow_at+1,ms+overflow_at,overflow_at%2?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN,0,overflow_x,overflow_y};++overflow_at;return 1;
}
static bool review_foreground(void*c,const risc_input_foreground_v1*f,size_t n){++nav_claims;return foreground(c,f,n);}
static bool review_reset(void*c){++nav_resets;return reset(c);}
static void queue_tap(unsigned x,unsigned y){assert(event_count+2<=sizeof(events)/sizeof(events[0]));events[event_count++]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)x,(uint16_t)y};events[event_count++]=(risc_touch_event_v1){++seq,ms+20,3,0,(uint16_t)x,(uint16_t)y};}
static const risc_text_entry_api_v1*text;
static const risc_text_entry_api_v1_checkpoint*cp;
static uint64_t token;
static risc_text_entry_state_v1 value(void){risc_text_entry_state_v1 state={.struct_size=sizeof(state)};assert(!text->poll(NULL,token,&state));return state;}
static void drain(unsigned times){for(unsigned n=0;n<times;n++)(void)value();}
static void retained_fence(const risc_driver_v2*text_driver){
 unsigned before=calls;risc_text_entry_state_v1 state={.struct_size=sizeof(state)};
 assert(text->poll(NULL,token,&state)==RISC_TEXT_ENTRY_RETAINED);
 assert(cp->pause(NULL,token)==RISC_TEXT_ENTRY_RETAINED);
 assert(cp->resume(NULL,token,0)==RISC_TEXT_ENTRY_RETAINED);
 assert(text->close(NULL,token)==RISC_TEXT_ENTRY_RETAINED);
 assert(!text_driver->quiesce());text_driver->stop();
 const risc_scene_checkpoint_api_v1*scene_cp=risc_scene_checkpoint_get_v1(api);uint64_t scene_session=review_scene_session();
 assert(scene_cp->pause(NULL,scene_session)==RISC_SCENE_RETAINED);
 assert(scene_cp->resume(NULL,scene_session,0)==RISC_SCENE_RETAINED);
 assert(api->close(NULL,scene_session)==RISC_SCENE_RETAINED);
 assert(!driver->quiesce());driver->stop();assert(calls==before);
}
static void text_settle(void){risc_text_entry_checkpoint_state_v1 status={.struct_size=sizeof(status)};for(unsigned n=0;;n++){assert(n<1000);(void)value();assert(!cp->checkpoint_snapshot(NULL,token,&status));if(!(status.flags&(RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY|RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)))break;}}
int main(int argc,char**argv){
 assert(argc==3);const char*mode=argv[1];bool paper=!strcmp(argv[2],"paper");
 if(paper){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;risc_input_navigation_api_v1 navigation=nav;navigation.foreground=review_foreground;navigation.reset=review_reset;
 risc_touch_api_v1 touch=raw;touch.next=review_next;
 driver=t5_driver_get(2);api=driver->capability;risc_provider_dependency_v1 scene_deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&touch},{"input.navigation",1,&navigation},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(scene_deps,5));const risc_driver_v2*text_driver=review_text_driver_get(2);text=text_driver->capability;cp=risc_text_entry_checkpoint(text);assert(cp);
 risc_provider_dependency_v1 dep={RISC_SCENE_CAPABILITY,1,api};assert(text_driver->start(&dep,1));risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=25,.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON,.label="NAME",.text="draft"};assert(!text->open(NULL,&q,&token));assert(!cp->checkpoint_enable(NULL,token));text_settle();
 int x,y;assert(review_target('A',&x,&y));risc_text_entry_state_v1 state={0};unsigned before_claims=nav_claims,before_resets=nav_resets,before_unsubs=unsubscribes;int rc;
 if(!strcmp(mode,"configure-continuity")){
  touch_tap((unsigned)x,(unsigned)y);const risc_scene_lifecycle_api_v1*l=risc_scene_lifecycle_get_v1(api);assert(l);
  unsigned before=calls;assert(!l->configure(NULL,review_scene_session(),0));assert(calls==before);
  assert(!l->configure(NULL,review_scene_session(),RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY));assert(calls==before);
  assert(cp->pause(NULL,token)==RISC_TEXT_ENTRY_BUSY);state=value();assert(!strcmp(state.text,"draftA"));
 }
 else if(!strcmp(mode,"before-pause")){touch_tap((unsigned)x,(unsigned)y);assert(cp->pause(NULL,token)==RISC_TEXT_ENTRY_BUSY);state=value();assert(!strcmp(state.text,"draftA"));}
 else if(!strcmp(mode,"presenting-input")){
  allow_complete=false;touch_tap((unsigned)x,(unsigned)y);for(unsigned n=0;!presenting;n++){assert(n<2000);state=value();}
  assert(review_target('B',&x,&y));touch_tap((unsigned)x,(unsigned)y);assert(cp->pause(NULL,token)==RISC_TEXT_ENTRY_BUSY);state=value();assert(!strcmp(state.text,"draftAB"));
  assert(review_target('C',&x,&y));touch_tap((unsigned)x,(unsigned)y);state=value();assert(!strcmp(state.text,"draftABC")&&presenting);allow_complete=true;
 }else if(!strcmp(mode,"pause-again")){
  allow_complete=false;touch_tap((unsigned)x,(unsigned)y);for(unsigned n=0;!presenting;n++){assert(n<2000);state=value();}
  const risc_scene_checkpoint_api_v1*scene_cp=risc_scene_checkpoint_get_v1(api);assert(scene_cp);uint64_t s=review_scene_session();assert(scene_cp->pause(NULL,s)==RISC_SCENE_AGAIN);assert(subscribed&&nav_claims==before_claims&&nav_resets==before_resets);
  event_at=event_count=0;queue_tap((unsigned)x,(unsigned)y);queue_tap((unsigned)x,(unsigned)y);allow_complete=true;assert(!scene_cp->pause(NULL,s));assert(!scene_cp->resume(NULL,s,0));drain(30);state=value();assert(!strcmp(state.text,"draftAAA"));
 }else{
  assert(!cp->pause(NULL,token));assert(!held&&!presenting&&subscribed);assert(nav_claims==before_claims&&nav_resets==before_resets&&unsubscribes==before_unsubs);
  if(!strncmp(mode,"loss-",5)||!strncmp(mode,"fault-",6)){
   bool loss=!strncmp(mode,"loss-",5);const char*point=mode+(loss?5:6);bool closing=!strncmp(point,"close-",6);const char*site=point+6;
   if(loss)loss_callback=site;else if(!strcmp(site,"snapshot"))bad_snapshot=true;else if(!strcmp(site,"unsubscribe"))bad_unsubscribe=true;else assert(0);
   rc=closing?text->close(NULL,token):cp->resume(NULL,token,RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
   assert(rc==RISC_TEXT_ENTRY_RETAINED);retained_fence(text_driver);printf("continuity %s %s PASS (retention fence)\n",argv[2],mode);return 0;
  }
  if(!strcmp(mode,"overflow")){
   overflow_transport=true;overflow_start=seq;seq+=70;overflow_x=(uint16_t)x;overflow_y=(uint16_t)y;
   assert(!cp->resume(NULL,token,0));state.struct_size=sizeof(state);assert(text->poll(NULL,token,&state)==RISC_TEXT_ENTRY_RETAINED);
   retained_fence(text_driver);printf("continuity %s %s PASS (bounded overflow fence)\n",argv[2],mode);return 0;
  }
  unsigned flags=0;
  if(!strcmp(mode,"queued")||!strcmp(mode,"repeat")||!strcmp(mode,"focus-changed")||!strcmp(mode,"gap")){
   event_at=event_count=0;queue_tap((unsigned)x,(unsigned)y);if(!strcmp(mode,"repeat"))queue_tap((unsigned)x,(unsigned)y);
   if(!strcmp(mode,"focus-changed"))flags=RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED;
   if(!strcmp(mode,"gap"))queue_gap=true;
  }else if(!strcmp(mode,"held")){event_at=0;event_count=1;snapshot_contacts=1;events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)x,(uint16_t)y};}
  else if(!strcmp(mode,"home"))nav_pressed=RISC_NAV_HOME;
  else if(!strcmp(mode,"back"))nav_pressed=RISC_NAV_BACK;
  else if(!strcmp(mode,"close-paused")){}
  else assert(0);
  if(strcmp(mode,"close-paused")){
   assert(!cp->resume(NULL,token,flags));
   if(!flags)assert(nav_claims==before_claims&&nav_resets==before_resets&&unsubscribes==before_unsubs);
   else assert(nav_claims==before_claims+1&&nav_resets==before_resets+1);
   drain(30);
   if(!strcmp(mode,"held")){event_at=0;event_count=1;snapshot_contacts=0;events[0]=(risc_touch_event_v1){++seq,ms+20,3,0,(uint16_t)x,(uint16_t)y};drain(20);}
   state=value();
   if(!strcmp(mode,"home")||!strcmp(mode,"back")){assert(state.state==RISC_TEXT_ENTRY_CANCELLED);assert(!!(state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)==!strcmp(mode,"home"));}
   else if(!strcmp(mode,"focus-changed")||!strcmp(mode,"gap")){assert(!strcmp(state.text,"draft"));assert(review_target('A',&x,&y));touch_tap((unsigned)x,(unsigned)y);drain(20);state=value();assert(!strcmp(state.text,"draftA"));}
   else assert(!strcmp(state.text,!strcmp(mode,"repeat")?"draftAA":"draftA"));
  }
 }
 allow_complete=true;while((rc=text->close(NULL,token))==RISC_TEXT_ENTRY_AGAIN)ms+=5;
 assert(!rc&&text_driver->quiesce()&&!subscribed);text_driver->stop();assert(driver->quiesce());driver->stop();
 printf("continuity %s %s PASS (draft=%s claims=%u resets=%u unsubs=%u)\n",argv[2],mode,state.text,nav_claims,nav_resets,unsubscribes);return 0;
}
