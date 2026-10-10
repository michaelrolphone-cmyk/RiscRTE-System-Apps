#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "SceneKeyboardV1.h"
#include <assert.h>
#include <stdio.h>
extern void scene_timing_begin(void);
extern bool scene_timing_finished(void);
extern void scene_timing_action(unsigned);
extern void scene_timing_complete(unsigned);
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};assert(r->acquire("ui.scene",1,0,&g));
 const risc_scene_api_v1*a=g.api;
 risc_scene_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=1};
 d.routes[0]=(risc_scene_route_v1){1,0,"Typing"};
 d.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_KEYBOARD_NODE,.action=77,.maximum=3,.step=1,.label="Name"};
 uint64_t s=0;assert(!a->open(a->context,&d,0,&s));scene_timing_begin();unsigned count=0;
 while(!scene_timing_finished()){
  risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=a->next(a->context,s,&e);assert(rc==0||rc==1);
  if(rc==0){assert(e.kind==RISC_SCENE_VALUE_EVENT);scene_timing_action((unsigned)e.value);++count;
   ++d.revision;snprintf(d.nodes[0].text,sizeof(d.nodes[0].text),"%u",count);assert(!a->update(a->context,s,&d));}
  r->yield_ms(1);
 }
 scene_timing_complete(count);int rc;while((rc=a->close(a->context,s))==RISC_SCENE_AGAIN)r->yield_ms(1);
 assert(rc==RISC_SCENE_OK);assert(r->release(&g));
}
