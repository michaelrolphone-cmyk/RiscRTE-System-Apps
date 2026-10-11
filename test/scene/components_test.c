#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneComponentsV1.h"
static risc_components_document_v1 rich;
static const risc_scene_components_api_v1 *components;
static void component_document(void){
 rich=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(rich),.revision=1,.root=1,.route_count=1,.node_count=60,.screen_key=1};rich.routes[0]=(risc_scene_route_v1){1,99,"CHECKLIST"};
 for(unsigned i=0;i<rich.node_count;i++){rich.nodes[i]=(risc_scene_node_v1){.id=i+1,.route=1,.kind=RISC_COMPONENT_CHECK_ROW,.action=10,.maximum=1,.step=1};snprintf(rich.nodes[i].label,40,"TASK %u",i+1);rich.details[i].secondary_action=20;rich.details[i].marker=6;}
}
static void drag(unsigned x,unsigned from,unsigned to){event_at=0;event_count=3;events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)x,(uint16_t)from};events[1]=(risc_touch_event_v1){++seq,ms+20,2,0,(uint16_t)x,(uint16_t)to};events[2]=(risc_touch_event_v1){++seq,ms+40,3,0,(uint16_t)x,(uint16_t)to};}
int main(int argc,char **argv){
 assert(argc==2);bool paper=!strcmp(argv[1],"paper");if(paper){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};}
 startup();assert(!api->close(NULL,session));components=risc_scene_components_get_v1(api);assert(components);component_document();expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!components->open(NULL,&rich,NULL,&session));settle();
 unsigned scale=paper?2:1;touch_tap(25*scale,70*scale);risc_scene_event_v1 e;assert(tick(&e)==0&&e.node==1&&e.kind==RISC_SCENE_VALUE_EVENT&&e.action==10&&e.value==1);
 rich.revision++;rich.nodes[0].value=1;assert(!components->update(NULL,session,&rich));
 touch_tap(100*scale,70*scale);assert(tick(&e)==0&&e.node==1&&e.action==20);
 /* Finger-follow scrolling changes logical hit targets before a physical
  * pending frame completes. A swipe must not toggle a crossed checkbox. */
 allow_complete=false;begin_frame();drag(100*scale,200*scale,80*scale);assert(tick(&e)==RISC_SCENE_IDLE);touch_tap(25*scale,80*scale);assert(tick(&e)==0&&e.node>1&&e.action==10);allow_complete=true;settle();
 unsigned calls_before=calls;risc_components_document_v1 bad=rich;bad.revision++;
 bad.nodes[0].value=2;assert(components->update(NULL,session,&bad)==RISC_SCENE_INVALID);assert(calls==calls_before);
 /* Caption of a control row is inert, unlike its own controls. */
 unsigned row_y=(paper?44+21:68+20)*scale;
 rich.revision++;rich.screen_key++;rich.node_count=1;rich.nodes[0].kind=RISC_COMPONENT_SWITCH;rich.details[0].secondary_action=0;assert(!components->update(NULL,session,&rich));touch_tap(30*scale,row_y);assert(tick(&e)==RISC_SCENE_IDLE);touch_tap(180*scale,row_y);assert(tick(&e)==0&&e.action==10);
 /* Shared segmented tabs: three choices, nonzero minimum, live hit maps
  * during refresh, disabled state and the inset gap remain independent. */
 rich.revision++;rich.screen_key++;rich.nodes[0].kind=RISC_COMPONENT_SEGMENTS;
 rich.nodes[0].minimum=2;rich.nodes[0].maximum=4;rich.nodes[0].value=3;
 strcpy(rich.details[0].choices,"PLACES|EVENTS|TRIGGERS");assert(!components->update(NULL,session,&rich));
 allow_complete=false;begin_frame();
 for(unsigned i=0;i<3;i++){touch_tap((45+i*70)*scale,row_y);assert(tick(&e)==0&&e.action==10&&e.value==(int)i+2);}
 touch_tap(86*scale,row_y);assert(tick(&e)==RISC_SCENE_IDLE);
 rich.revision++;rich.nodes[0].flags=RISC_SCENE_DISABLED;assert(!components->update(NULL,session,&rich));touch_tap(45*scale,row_y);assert(tick(&e)==RISC_SCENE_IDLE);
 allow_complete=true;settle();
 /* Ordinary documents cannot masquerade as a component table or read suffix
  * fields on older providers. */
 risc_scene_api_v1 old=*api;old.struct_size=sizeof(old);assert(!risc_scene_components_get_v1(&old));
 finish();printf("Shared NOVA component scrolling, control targets, validation %s PASS\n",argv[1]);return 0;
}
