#define main baseline_main
#include "../scene/host_test.c"
#undef main
#include "RiscScenePointerV1.h"
extern void pointer_test_hit(uint32_t,int*,int*);
extern int pointer_test_fault(void);
static risc_touch_contact_v1 positions[2];
static const risc_scene_pointer_api_v1 *pa;
static risc_components_document_v1 pd;
static risc_scene_pointer_event_v1 pe;
static bool pointer_snapshot(void *c,risc_touch_snapshot_v1 *s){
 if(!snapshot(c,s))return false;
 for(unsigned i=0;i<s->contact_count&&i<2;i++)s->contacts[i]=positions[i];
 return true;
}
static int pt(void){pe=(risc_scene_pointer_event_v1){.struct_size=sizeof(pe)};return pa->next(NULL,session,&pe);}
static void ps(void){for(unsigned i=0;i<2000;i++){
 int rc=pt();assert(rc==RISC_SCENE_IDLE);
 risc_scene_navigation_v1 p={.struct_size=sizeof(p)};uint32_t flags;
 assert(!api->snapshot(NULL,session,&p,&flags));if(!(flags&RISC_SCENE_PRESENTING)){assert(pt()==RISC_SCENE_IDLE);return;}
 }assert(0);}
static void edge(unsigned kind,unsigned id,int x,int y){
 event_at=0;event_count=1;ms+=20;
 events[0]=(risc_touch_event_v1){++seq,ms,(uint8_t)kind,(uint8_t)id,(uint16_t)x,(uint16_t)y};
 if(kind==RISC_TOUCH_EVENT_DOWN){assert(snapshot_contacts<2);positions[snapshot_contacts++]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};}
 else for(unsigned i=0;i<snapshot_contacts;i++)if(positions[i].id==id){if(kind==RISC_TOUCH_EVENT_UP)positions[i]=positions[--snapshot_contacts];else {positions[i].x=x;positions[i].y=y;}break;}
}
static void finish(void){
 snapshot_contacts=0;event_count=event_at=0;nav_buttons=0;
 for(unsigned i=0;i<2000;i++){int r=api->close(NULL,session);if(!r){assert(driver->quiesce());driver->stop();return;}
 assert(r==RISC_SCENE_BUSY||r==RISC_SCENE_AGAIN);if(r==RISC_SCENE_BUSY){int rc=pt();assert(rc==RISC_SCENE_OK||rc==RISC_SCENE_IDLE);}}
 assert(0);
}
int main(int argc,char **argv){
 assert(argc==3);unsigned shape=atoi(argv[1]);const char *mode=argv[2];
 const unsigned sizes[][2]={{160,160},{240,240},{410,502},{480,800},{502,410},{320,180}};
 w=sizes[shape][0];h=sizes[shape][1];format=shape==3?1:5;poll_step=0;
 prof.preferred_format=format;prof.font_scale=1;prof.row_height=36;
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 driver=t5_driver_get(2);api=driver->capability;pa=risc_scene_pointer_get_v1(api);assert(pa&&risc_scene_hold_get_v1(api));
 risc_scene_api_v1 short_api=*api;short_api.struct_size=sizeof(risc_scene_hold_api_v1);assert(!risc_scene_pointer_get_v1(&short_api));
 risc_touch_api_v1 provider=raw;provider.snapshot=pointer_snapshot;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&provider},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));
 pd=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(pd),.revision=1,.root=1,.route_count=1,.node_count=1,.screen_key=1};
 pd.routes[0]=(risc_scene_route_v1){.id=1,.title="POINTER"};
 pd.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_POINTER,.action=42,.label="Move pointer",.text="Tap and drag"};
 assert(risc_scene_components_get_v1(api)->open(NULL,&pd,NULL,&session)==RISC_SCENE_INVALID);
 if(!strcmp(mode,"invalid")){pd.nodes[0].maximum=1;assert(pa->open(NULL,&pd,NULL,&session)==RISC_SCENE_INVALID);driver->stop();return 0;}
 assert(!pa->open(NULL,&pd,NULL,&session));int x,y;pointer_test_hit(1,&x,&y);assert(x>=0);
 if(!strcmp(mode,"previsible")){edge(1,1,x,y);assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET);edge(3,1,x,y);assert(pt()==RISC_SCENE_IDLE);ps();finish();return 0;}
 ps();
 edge(1,1,x,y);assert(pt()==RISC_SCENE_IDLE);
 if(!strcmp(mode,"tap")){edge(3,1,x,y);assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_REPORT&&pe.click==1);}
 else if(!strcmp(mode,"move")){edge(2,1,x+10,y);assert(pt()==RISC_SCENE_OK&&pe.dx==10&&!pe.click&&!pe.buttons);}
 else if(!strcmp(mode,"right")){edge(1,2,x+3,y);assert(pt()==RISC_SCENE_IDLE);edge(3,2,x+3,y);assert(pt()==RISC_SCENE_IDLE);edge(3,1,x,y);assert(pt()==RISC_SCENE_OK&&pe.click==2);}
 else if(!strcmp(mode,"scroll")){
  edge(1,2,x+3,y);assert(pt()==RISC_SCENE_IDLE);
  event_at=event_count=0;ms+=20;
  for(unsigned i=0;i<2;i++){positions[i].x+=32;events[event_count++]=(risc_touch_event_v1){++seq,ms,RISC_TOUCH_EVENT_MOVE,positions[i].id,positions[i].x,positions[i].y};}
  assert(pt()==RISC_SCENE_OK&&pe.wheel_x>0&&!pe.dx&&!pe.wheel_y);
 }
 else if(!strcmp(mode,"drag")){
  edge(3,1,x,y);assert(pt()==RISC_SCENE_OK&&pe.click==1);
  edge(1,1,x,y);assert(pt()==RISC_SCENE_IDLE);
  edge(2,1,x+20,y);assert(pt()==RISC_SCENE_OK&&pe.buttons==1&&pe.dx==20);
  edge(3,1,x+20,y);assert(pt()==RISC_SCENE_OK&&!pe.buttons&&!pe.click);
 }
 else if(!strcmp(mode,"gap")){ms+=101;assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET);}
 else if(!strcmp(mode,"resync")){queue_gap=true;assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET);}
 else if(!strcmp(mode,"update")){pd.revision++;assert(pa->update(NULL,session,&pd)==RISC_SCENE_BUSY);assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET);assert(!pa->update(NULL,session,&pd));}
 else if(!strcmp(mode,"close")){assert(api->close(NULL,session)==RISC_SCENE_BUSY);assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET);}
 else if(!strcmp(mode,"pause")){const risc_scene_checkpoint_api_v1 *cp=risc_scene_checkpoint_get_v1(api);assert(cp->pause(NULL,session)==RISC_SCENE_BUSY);assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET);assert(!cp->pause(NULL,session));assert(!cp->resume(NULL,session,RISC_SCENE_RESUME_FOCUS_CHANGED));}
 else if(!strcmp(mode,"outside")){edge(2,1,0,0);assert(pt()==RISC_SCENE_IDLE);}
 else if(!strcmp(mode,"local_fault")){unsigned before=calls;assert(pointer_test_fault()==RISC_SCENE_BUSY&&calls==before);assert(pt()==RISC_SCENE_OK&&pe.kind==RISC_POINTER_RESET&&(pe.intent.flags&RISC_HOLD_LOCAL_FAULT_PENDING));assert(calls==before);assert(pt()==RISC_SCENE_RETAINED&&calls==before);return 0;}
 else if(!strcmp(mode,"native_loss")){native_alive=false;unsigned before=calls;assert(pt()==RISC_SCENE_RETAINED&&calls==before);return 0;}
 else assert(0);
 finish();printf("%s shape %u PASS\n",mode,shape);
}
