#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneModalV1.h"
void modal_test_capture(void);
void modal_test_restored(void);
int main(int argc,char **argv){
 assert(argc==7);w=(unsigned)strtoul(argv[1],NULL,10);h=(unsigned)strtoul(argv[2],NULL,10);
 format=(unsigned)strtoul(argv[3],NULL,10);unsigned rotation=(unsigned)strtoul(argv[4],NULL,10);
 unsigned continuity=(unsigned)strtoul(argv[5],NULL,10);const char *mode=argv[6];
 prof=(risc_scene_profile_v1){1,sizeof(prof),format,format==5?2:3,format==5?42:88,
     format==5?8:20,format==5?0xffff:0,format==5?0x0841:0xffff,0x05ff,rotation,0,0};
 startup();
 const risc_scene_modal_api_v1 *modal=risc_scene_modal_get_v1(api);assert(modal);
 const risc_scene_checkpoint_api_v1 *cp=risc_scene_checkpoint_get_v1(api);assert(cp);
 const risc_scene_lifecycle_api_v1 *life=risc_scene_lifecycle_get_v1(api);assert(life);
 risc_scene_modal_api_v1 old=*modal;((risc_scene_api_v1*)&old)->struct_size=sizeof(old)-1;
 assert(!risc_scene_modal_get_v1((void*)&old));old=*modal;old.modal_tag=0;
 assert(!risc_scene_modal_get_v1((void*)&old));old=*modal;old.open=NULL;
 assert(!risc_scene_modal_get_v1((void*)&old));
 /* A revealed editor, layer and focused route survive, not just a root page. */
 doc.revision++;doc.node_count=1;doc.route_count=1;
 doc.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_KEYBOARD_NODE,
     .flags=RISC_SCENE_KEYBOARD_PASSWORD|RISC_SCENE_KEYBOARD_REVEALED,
     .action=40,.target=63,.maximum=3,.step=1,.value=2,.text="saved secret draft",.label="PASSWORD"};
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 assert(!api->update(NULL,session,&doc));
 assert(!life->configure(NULL,session,continuity?RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY:0));settle();
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=1};
 d.routes[0]=(risc_scene_route_v1){1,77,"NOTICE"};
 d.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_ACTION,.action=77,.label="ACKNOWLEDGE"};
 uint64_t child=0,parent=session;unsigned before=calls;
 assert(modal->open(NULL,&d,&child)==RISC_SCENE_BUSY&&calls==before);
 assert(!cp->pause(NULL,parent));modal_test_capture();
 risc_components_document_v1 bad=d;bad.revision=0;before=calls;
 assert(modal->open(NULL,&bad,&child)==RISC_SCENE_INVALID&&calls==before);
 if(!strcmp(mode,"retained-open"))loss_callback="subscribe";
 int rc=modal->open(NULL,&d,&child);
 if(!strcmp(mode,"retained-open")){
   assert(rc==RISC_SCENE_RETAINED&&!driver->quiesce());before=calls;native_alive=true;
   assert(modal->open(NULL,&d,&child)==RISC_SCENE_RETAINED);
   assert(modal->close(NULL,parent)==RISC_SCENE_RETAINED&&calls==before);return 0;
 }
 assert(rc==RISC_SCENE_OK&&child!=parent);session=child;settle();
 risc_scene_event_v1 e={.struct_size=sizeof(e)};
 assert(api->next(NULL,parent,&e)==RISC_SCENE_BUSY);
 assert(cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW)==RISC_SCENE_BUSY);
 uint64_t nested=0;before=calls;assert(modal->open(NULL,&d,&nested)==RISC_SCENE_BUSY&&calls==before);
 assert(modal->close(NULL,parent)==RISC_SCENE_BUSY);
 assert(!driver->quiesce());
 if(!strcmp(mode,"retained-close")){
   loss_callback="unsubscribe";assert(modal->close(NULL,child)==RISC_SCENE_RETAINED);
   before=calls;native_alive=true;assert(modal->close(NULL,child)==RISC_SCENE_RETAINED);
   assert(!driver->quiesce()&&calls==before);return 0;
 }
 /* Neither a pending child display token nor a stale token can restore parent. */
 const risc_scene_components_api_v1 *cmp=risc_scene_components_get_v1(api);d.revision++;
 assert(!cmp->update(NULL,child,&d));allow_complete=false;begin_frame();
 assert(modal->close(NULL,child)==RISC_SCENE_AGAIN);
 assert(api->next(NULL,parent,&e)==RISC_SCENE_BUSY);
 allow_complete=true;ms+=100;
 assert(!api->close(NULL,child)); /* base close also observes modal custody */
 modal_test_restored();session=parent;
 assert(modal->close(NULL,child)==RISC_SCENE_STALE);
 assert(!cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED));
 settle();doc.revision++;strcpy(doc.nodes[0].text,"continued draft");assert(!api->update(NULL,parent,&doc));
 settle();finish();
 printf("Modal parent editor/custody %ux%u format=%u continuity=%u PASS\n",w,h,format,continuity);return 0;
}
