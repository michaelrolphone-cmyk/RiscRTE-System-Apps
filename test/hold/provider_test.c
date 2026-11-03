#define main existing_scene_main
#include "../scene/host_test.c"
#undef main
#include "RiscSceneHoldV1.h"
extern int hold_test_local_fault(void);
extern void hold_test_queue_full(void);
extern void hold_test_hit(uint32_t,int*,int*);
static const risc_scene_hold_api_v1 *ha;
static risc_components_document_v1 hd;
static risc_scene_hold_event_v1 he;
static bool recurse_clock,recurse_base;
static unsigned acquire_count;
static bool counted_acquire(void*c,uint32_t f,risc_display_surface_v1*out){++acquire_count;return acquire(c,f,out);}
static uint64_t guarded_now(void *c){
 if(recurse_clock){recurse_clock=false;
  if(recurse_base)assert(api->close(NULL,session)==RISC_SCENE_RETAINED);
  else {risc_scene_hold_event_v1 e={.struct_size=sizeof(e)};assert(ha->next(NULL,session,&e)==RISC_SCENE_RETAINED);}
  return ms;
 }
 return now(c);
}
static int ht(void){he=(risc_scene_hold_event_v1){.struct_size=sizeof(he)};return ha->next(NULL,session,&he);}
static void hs(void){for(unsigned i=0;i<2000;i++){assert(ht()==RISC_SCENE_IDLE);risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;assert(!api->snapshot(NULL,session,&n,&flags));if(!(flags&RISC_SCENE_PRESENTING))return;}assert(0);}
static void edge(unsigned kind,int x,int y){event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms,kind,0,(uint16_t)x,(uint16_t)y};snapshot_contacts=kind==RISC_TOUCH_EVENT_UP?0:1;}
static void press(int *x,int *y){hold_test_hit(1,x,y);assert(*x>=0);edge(RISC_TOUCH_EVENT_DOWN,*x,*y);assert(ht()==RISC_SCENE_OK);assert(he.phase==RISC_HOLD_PRESS&&he.event.node==1&&he.event.action==10&&he.generation);}
static void end_clean(void){snapshot_contacts=0;nav_buttons=0;event_count=event_at=0;for(unsigned i=0;i<2000;i++){int r=api->close(NULL,session);if(r==RISC_SCENE_OK){assert(driver->quiesce());driver->stop();return;}assert(r==RISC_SCENE_BUSY);}assert(0);}
int main(int argc,char **argv){
 assert(argc==3);unsigned shape=(unsigned)atoi(argv[1]);const char* mode=argv[2];
 w=shape==0?240:shape==1?466:480;h=shape==2?800:w;format=shape==2?1:5;
 poll_step=0;prof.preferred_format=format;prof.font_scale=1;prof.row_height=36;expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 driver=t5_driver_get(2);api=driver->capability;ha=risc_scene_hold_get_v1(api);assert(ha);
 assert(risc_scene_components_get_v1(api)&&risc_scene_canvas_get_v1(api));
 risc_platform_clock_api_v1 guarded_clock=clk;guarded_clock.monotonic_ms=guarded_now;
 risc_display_output_api_v1 measured=disp;measured.acquire=counted_acquire;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&measured},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&guarded_clock},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));
 hd=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(hd),.revision=1,.root=1,.route_count=1,.node_count=2,.screen_key=1};hd.routes[0]=(risc_scene_route_v1){.id=1,.title="HELD INPUT"};
 hd.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_HOLD_KEY,.action=10,.label="BUTTON1",.text="CTRL SHIFT A"};
 hd.nodes[1]=(risc_scene_node_v1){.id=2,.route=1,.kind=RISC_SCENE_ACTION,.action=11,.label="OTHER"};
 if(!strcmp(mode,"confirm")||!strcmp(mode,"timeout")||!strcmp(mode,"confirm_resume"))hd.nodes[0].step=RISC_HOLD_CONFIRM;
 if(!strcmp(mode,"long_labels")){strcpy(hd.nodes[0].label,"NUMBERS MATCH - CONFIRM EXPLICITLY");strcpy(hd.nodes[0].text,"CTRL SHIFT ALT SUPER A - FULL COPIED ASSIGNMENT WITHOUT APP GEOMETRY");}
 if(!strcmp(mode,"invalid")){hd.nodes[0].step=2;assert(ha->open(NULL,&hd,NULL,&session)==RISC_SCENE_INVALID);driver->stop();puts("invalid passed");return 0;}
 assert(risc_scene_components_get_v1(api)->open(NULL,&hd,NULL,&session)==RISC_SCENE_INVALID);
 risc_scene_navigation_v1 initial={.api_version=1,.struct_size=sizeof(initial),.depth=1,.routes={1},.focus={1}};assert(!ha->open(NULL,&hd,&initial,&session));hs();int x,y;
 if(!strcmp(mode,"resume")||!strcmp(mode,"confirm_resume")){
  const risc_scene_checkpoint_api_v1 *cp=risc_scene_checkpoint_get_v1(api);assert(cp);unsigned a=acquire_count,s=submits;
  assert(!cp->pause(NULL,session));assert(!cp->resume(NULL,session,0));
  for(unsigned i=0;i<3;i++){assert(ht()==RISC_SCENE_IDLE);risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;assert(!api->snapshot(NULL,session,&n,&flags));assert(!(flags&RISC_SCENE_PRESENTING)&&a==acquire_count&&s==submits);}
  press(&x,&y);ms+=20;edge(RISC_TOUCH_EVENT_UP,x,y);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_RELEASE);end_clean();return 0;
 }
 if(!strcmp(mode,"mixed_start")){hold_test_hit(2,&x,&y);assert(x>=0);edge(RISC_TOUCH_EVENT_DOWN,x,y);nav_buttons=nav_pressed=RISC_NAV_CONFIRM;assert(ht()==RISC_SCENE_IDLE);end_clean();return 0;}
 if(!strcmp(mode,"mixed_press")){hold_test_hit(1,&x,&y);edge(RISC_TOUCH_EVENT_DOWN,x,y);nav_buttons=nav_pressed=RISC_NAV_CONFIRM;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_AMBIGUOUS);end_clean();return 0;}
 if(!strcmp(mode,"navigation")){
   nav_buttons=nav_pressed=RISC_NAV_CONFIRM;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_PRESS);ms+=20;nav_buttons=0;nav_released=RISC_NAV_CONFIRM;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_RELEASE&&he.held_ms>=20);end_clean();return 0;
 }
 press(&x,&y);uint64_t generation=he.generation;
 if(!strcmp(mode,"release")||!strcmp(mode,"confirm")||!strcmp(mode,"long_labels")){ms+=20;edge(RISC_TOUCH_EVENT_UP,x,y);int rc=ht();assert(rc==RISC_SCENE_OK&&he.phase==RISC_HOLD_RELEASE&&he.held_ms==20&&he.generation==generation);}
 else if(!strcmp(mode,"motion")){ms+=10;edge(RISC_TOUCH_EVENT_MOVE,x+9,y);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_MOTION);}
 else if(!strcmp(mode,"gap")){ms+=101;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_SERVICE_GAP);}
 else if(!strcmp(mode,"timeout")){for(unsigned i=0;i<8;i++){ms+=100;assert(ht()==RISC_SCENE_IDLE);}ms++;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_TIMEOUT);}
 else if(!strcmp(mode,"long")){for(unsigned i=0;i<700;i++){ms+=100;assert(ht()==RISC_SCENE_IDLE);}edge(RISC_TOUCH_EVENT_UP,x,y);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_RELEASE&&he.held_ms==70000);}
 else if(!strcmp(mode,"update")){hd.revision++;assert(ha->update(NULL,session,&hd)==RISC_SCENE_BUSY);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL);assert(!ha->update(NULL,session,&hd));}
 else if(!strcmp(mode,"configure")){const risc_scene_lifecycle_api_v1 *lc=risc_scene_lifecycle_get_v1(api);assert(lc);unsigned before=calls;assert(lc->configure(NULL,session,RISC_SCENE_FEATURE_SHARED_CONTROLS)==RISC_SCENE_BUSY&&calls==before);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL);assert(!lc->configure(NULL,session,RISC_SCENE_FEATURE_SHARED_CONTROLS));}
 else if(!strcmp(mode,"pause")){const risc_scene_checkpoint_api_v1 *cp=risc_scene_checkpoint_get_v1(api);assert(cp);assert(cp->pause(NULL,session)==RISC_SCENE_BUSY);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL);int r=cp->pause(NULL,session);assert(r==RISC_SCENE_OK||r==RISC_SCENE_BUSY);}
 else if(!strcmp(mode,"close")){assert(api->close(NULL,session)==RISC_SCENE_BUSY);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL);}
 else if(!strcmp(mode,"missing_up")){snapshot_contacts=0;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_AMBIGUOUS);}
 else if(!strcmp(mode,"sequence_gap")){++seq;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_AMBIGUOUS);}
 else if(!strcmp(mode,"resync")){queue_gap=true;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_AMBIGUOUS);}
 else if(!strcmp(mode,"local_fault")){unsigned before=calls;assert(hold_test_local_fault()==RISC_SCENE_BUSY);assert(calls==before);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.flags==RISC_HOLD_LOCAL_FAULT_PENDING);assert(calls==before);assert(ht()==RISC_SCENE_RETAINED&&calls==before);assert(!driver->quiesce());return 0;}
 else if(!strcmp(mode,"queue_fault")){hold_test_queue_full();edge(RISC_TOUCH_EVENT_MOVE,x,y);assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.flags==RISC_HOLD_LOCAL_FAULT_PENDING);unsigned before=calls;assert(ht()==RISC_SCENE_RETAINED&&calls==before);return 0;}
 else if(!strcmp(mode,"reentry")||!strcmp(mode,"base_reentry")){recurse_clock=true;recurse_base=!strcmp(mode,"base_reentry");assert(ht()==RISC_SCENE_RETAINED);unsigned before=calls;assert(ht()==RISC_SCENE_RETAINED&&calls==before);return 0;}
 else if(!strcmp(mode,"mixed")){nav_pressed=nav_buttons=RISC_NAV_CONFIRM;assert(ht()==RISC_SCENE_OK&&he.phase==RISC_HOLD_CANCEL&&he.reason==RISC_HOLD_REASON_AMBIGUOUS);}
 else if(!strcmp(mode,"clock_loss")){loss_callback="now";assert(ht()==RISC_SCENE_RETAINED);unsigned before=calls;assert(ht()==RISC_SCENE_RETAINED&&calls==before);return 0;}
 else if(!strcmp(mode,"local_fault_native")){assert(hold_test_local_fault()==RISC_SCENE_BUSY);native_alive=false;unsigned before=calls;assert(ht()==RISC_SCENE_RETAINED&&calls==before);return 0;}
 else if(!strcmp(mode,"native_loss")){native_alive=false;unsigned before=calls;assert(ht()==RISC_SCENE_RETAINED&&calls==before);assert(!driver->quiesce());return 0;}
 else if(!strcmp(mode,"dependency_loss")){driver_fault=true;assert(ht()==RISC_SCENE_RETAINED);unsigned before=calls;assert(ht()==RISC_SCENE_RETAINED&&calls==before);assert(!driver->quiesce());return 0;}
 else assert(0);
 end_clean();printf("%s shape %u passed\n",mode,shape);return 0;
}
