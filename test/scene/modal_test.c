#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneModalV1.h"
#include "SceneKeyboardV1.h"
void modal_test_capture(void);
void modal_test_restored(void);
void modal_test_allocation_failure(void);
void modal_test_allocation_unchanged(void);
uint32_t modal_test_logical_digest(void);
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
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=1,.screen_key=1};
 d.routes[0]=(risc_scene_route_v1){1,77,"NOTICE"};
 d.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_ACTION,.action=77,.label="ACKNOWLEDGE"};
 uint64_t child=0,parent=session;unsigned before=calls;
 assert(modal->open(NULL,&d,&child)==RISC_SCENE_BUSY&&calls==before);
 assert(!cp->pause(NULL,parent));modal_test_capture();
 modal_test_allocation_failure();before=calls;
 assert(modal->open(NULL,&d,&child)==RISC_SCENE_UNAVAILABLE&&calls==before);
 modal_test_allocation_unchanged();
 risc_components_document_v1 bad=d;bad.revision=0;before=calls;
 assert(modal->open(NULL,&bad,&child)==RISC_SCENE_INVALID&&calls==before);
 /* Ordinary open failure after detach must restore continuity-enabled parents. */
 if(!strcmp(mode,"success")){
   unsigned physical_w=w;w=159;
   assert(modal->open(NULL,&d,&child)==RISC_SCENE_UNAVAILABLE);w=physical_w;
   modal_test_restored();
   assert(!cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED));
   settle();assert(!cp->pause(NULL,parent));modal_test_capture();
 }
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
 /* Every mutable extension must reject the parent before touching child state. */
 uint32_t digest=modal_test_logical_digest();before=calls;
 const risc_scene_components_api_v1 *components=risc_scene_components_get_v1(api);
 const risc_scene_page_api_v1 *pages=risc_scene_page_get_v1(api);
 const risc_scene_page_refresh_api_v1 *refresh=risc_scene_page_refresh_get_v1(api);
 const risc_scene_reading_api_v1 *reading=risc_scene_reading_get_v1(api);
 const risc_scene_reading_links_api_v1 *links=risc_scene_reading_links_get_v1(api);
 const risc_scene_telemetry_api_v1 *telemetry=risc_scene_telemetry_get_v1(api);
 assert(api->update(NULL,parent,NULL)==RISC_SCENE_BUSY);
 assert(components->update(NULL,parent,NULL)==RISC_SCENE_BUSY);
 assert(pages->present_page(NULL,parent,NULL,NULL,0)==RISC_SCENE_BUSY);
 assert(refresh->present_page_with_refresh(NULL,parent,NULL,NULL,0,0)==RISC_SCENE_BUSY);
 assert(reading->present_reading(NULL,parent,NULL,NULL,0,0)==RISC_SCENE_BUSY);
 assert(reading->present_preview(NULL,parent,NULL,0,NULL,0)==RISC_SCENE_BUSY);
 assert(links->present_linked(NULL,parent,NULL,NULL,NULL,0,0)==RISC_SCENE_BUSY);
 if(telemetry)assert(telemetry->present(NULL,parent,NULL,NULL)==RISC_SCENE_BUSY);
 assert(modal_test_logical_digest()==digest&&calls==before);
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
 if(!strcmp(mode,"abandon")){
   assert(!api->close(NULL,parent));assert(driver->quiesce());
   assert(!api->open(NULL,&doc,NULL,&session));
   assert(!life->configure(NULL,session,RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY));settle();
   assert(!cp->pause(NULL,session));assert(!cp->resume(NULL,session,0));settle();finish();return 0;
 }
 assert(!cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED));
 settle();
 /* No intervening update may be required to restore editor input. */
 nav_pressed=RISC_NAV_BACK;e=(risc_scene_event_v1){.struct_size=sizeof(e)};
 assert(api->next(NULL,parent,&e)==RISC_SCENE_OK);
 assert(e.kind==RISC_SCENE_VALUE_EVENT&&e.document_revision==doc.revision&&
        e.node==1&&e.action==40&&e.value==RISC_SCENE_KEY_CANCEL);
 doc.revision++;strcpy(doc.nodes[0].text,"continued draft");assert(!api->update(NULL,parent,&doc));
 settle();
 /* A non-keyboard parent with no nodes still needs a current route mapping.
  * Exercise both ordinary failed-open rollback and a completed modal without
  * an update between restoration and the physical Back edge. */
 if(!strcmp(mode,"success")){
   assert(!api->close(NULL,parent));
   doc=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(doc),
       .revision=17,.root=1,.route_count=1,.routes={{.id=1,.back_action=91,.title="EMPTY"}}};
   expected_intent=RISC_DISPLAY_PRESENT_DEFAULT;
   assert(!api->open(NULL,&doc,NULL,&session));parent=session;
   assert(!life->configure(NULL,parent,continuity?RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY:0));settle();
   for(unsigned restored=0;restored<2;restored++){
     assert(!cp->pause(NULL,parent));modal_test_capture();child=0;
     if(!restored){
       unsigned physical_w=w;w=159;
       assert(modal->open(NULL,&d,&child)==RISC_SCENE_UNAVAILABLE);w=physical_w;
     }else{
       expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
       assert(!modal->open(NULL,&d,&child));session=child;settle();
       assert(!modal->close(NULL,child));session=parent;
     }
     modal_test_restored();
     e=(risc_scene_event_v1){.struct_size=sizeof(e)};
     assert(api->next(NULL,parent,&e)==RISC_SCENE_BUSY);
     expected_intent=RISC_DISPLAY_PRESENT_DEFAULT;
     assert(!cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED));settle();
     nav_pressed=RISC_NAV_BACK;e=(risc_scene_event_v1){.struct_size=sizeof(e)};
     assert(api->next(NULL,parent,&e)==RISC_SCENE_OK);
     assert(e.kind==RISC_SCENE_ACTION_EVENT&&!e.node&&e.action==91&&e.document_revision==17);
   }
 }
 finish();
 printf("Modal parent editor/custody %ux%u format=%u continuity=%u PASS\n",w,h,format,continuity);return 0;
}
