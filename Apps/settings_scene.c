/* One semantic Settings application. Providers own pixels, input and product
 * policy. No target identity, display size or presentation backend is linked. */
#include "PortableSettingsRuntime.h"
#include "PortableSettingsScene.h"
#include "RiscComponentSceneClient.h"
#include "RiscSceneResidentV1.h"
static const risc_runtime_api_v1 *runtime;
static bool retained,invoked,clean,exiting,model_ready;
static PortableSettingsRuntime backend;
static PortableSettingsModel model;
static pss_scene view,candidate;
static risc_components_document_v1 document;
static risc_component_scene_client scene;
static risc_scene_resident_v1 resident;
static uint32_t handoff;
static bool alive(void *unused){
 (void)unused;
 if(retained)return false;
 if(risc_runtime_get_api(1)!=runtime){retained=true;return false;}
 return true;
}
static void retain(void *unused){
 (void)unused;if(retained)return;retained=true;
 if(runtime&&risc_runtime_get_api(1)==runtime&&runtime->retain_invocation)
  (void)runtime->retain_invocation();
}
static int guard(void *unused){return alive(unused)?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_RETAINED;}
static void diagnostic(const char *text){if(alive(NULL)&&runtime->diagnostic)runtime->diagnostic(text);}
static bool publish(void){
 if(scene.token&&!pss_needs_publish(&view))return true;
 candidate=view;
 if(!pss_build(&candidate,&document)){retain(NULL);return false;}
 int rc=risc_component_scene_publish(&scene,&document,resident.enabled);
 if(rc==1)view=candidate;
 return rc==1;
}
/* Exhausting the bounded batch is not an idle queue. In that case neither a
 * mutation nor a shell handoff may begin until later calls drain the rest. */
static bool events(void){
 for(unsigned i=0;i<32&&alive(NULL);i++){
  risc_scene_event_v1 event={0};int rc=risc_component_scene_next(&scene,&event);
  if(rc<0)return false;
  if(!rc)return true;
  if(event.document_revision!=scene.committed.revision)continue;
  if(!event.node&&!event.action&&!event.value){
   if(event.kind==RISC_SCENE_SUSPEND_EVENT){exiting=true;return true;}
   if(event.kind==RISC_SCENE_CONTROLS_EVENT){
    if(resident.enabled&&psm_can_handoff(&model))handoff=RISC_RESIDENT_CHECKPOINT_CONTROLS;
    continue;
   }
  }
  /* Once a shell transition is accepted, drain inherited app input without
   * opening a new editor or mutation behind that transition. */
  if(handoff)continue;
  (void)pss_action(&view,&event);
  if(view.exit_requested){exiting=true;return true;}
 }
 return false;
}
static bool host_step(void){
 if(scene.activity){resident.activity=true;scene.activity=false;}
 if(!psm_can_handoff(&model))return true;
 if(!handoff){
  int rc=risc_scene_resident_poll_work_v1(&resident,scene.flags,0);
  if(!alive(NULL)||rc==RISC_RESIDENT_RETAINED){retain(NULL);return false;}
  if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){exiting=true;return true;}
  if(resident.policy)handoff=RISC_RESIDENT_CHECKPOINT_POLICY;
 }
 if(!handoff)return true;
 if(!scene.closing&&scene.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY))return true;
 if(!risc_component_scene_close(&scene))return alive(NULL);
 uint32_t flags=0;int rc=risc_scene_resident_dispatch_reply_v1(&resident,handoff,&flags);
 if(!alive(NULL)||rc==RISC_RESIDENT_RETAINED){retain(NULL);return false;}
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF)exiting=true;
 handoff=0;
 /* The model and draft remain in this invocation; reopening always rebuilds
  * semantic content. Configuration is never inferred from physical identity. */
 (void)flags;
 return true;
}
static bool close_app(void){
 if(model_ready&&!psm_can_handoff(&model))return false;
 if(!risc_component_scene_release(&scene))return false;
 if(!psr_close(&backend,model_ready?&model:NULL))return false;
 clean=true;return true;
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1=
 {1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||
 !rt->acquire||!rt->release||!rt->yield_ms||!rt->retain_invocation)return;
 if(retained||(invoked&&!clean)){retain(NULL);return;}
 runtime=rt;invoked=true;clean=false;exiting=false;handoff=0;model_ready=false;
 memset(&backend,0,sizeof(backend));memset(&model,0,sizeof(model));
 risc_component_scene_init(&scene,rt,alive,retain,NULL);
 if(!psr_open(&backend,rt,NULL,guard,retain,NULL)){
  diagnostic("Settings policy or required capabilities unavailable");
  if(alive(NULL)&&psr_close(&backend,NULL))clean=true;
  return;
 }
 if(!psr_init_model(&backend,&model)){retain(NULL);return;}
 model_ready=true;
 pss_init(&view,&model);
 int available=risc_component_scene_bind(&scene);
 if(available!=1){
  diagnostic("Settings semantic scene provider unavailable");
  if(alive(NULL)&&risc_component_scene_release(&scene)&&psr_close(&backend,&model))clean=true;
  return;
 }
 if(!risc_scene_resident_bind_v1(rt,&resident)||!alive(NULL)){retain(NULL);return;}
 while(alive(NULL)&&!exiting){
  bool drained=events();if(!alive(NULL)||exiting)break;
  if(model.phase==PSM_RETAINED){retain(NULL);break;}
  if(drained&&view.reload_requested&&psm_can_handoff(&model)&&
     risc_component_scene_presented(&scene,view.revision)==1){
   view.reload_requested=false;
   if(!psr_reload(&backend,&model)){
    diagnostic("Settings storage reload unavailable");
    if(alive(NULL)){model_ready=false;exiting=true;}
    break;
   }
   /* Reinitialized model revisions can equal the old value even when a
    * reconciled namespace changed. Retire old intents and force publication. */
   view.count=0;view.back_action=0;
  }
  if(drained&&!handoff){
   uint32_t revision=pss_save_revision(&view,scene.committed.revision);
   if(revision&&risc_component_scene_presented(&scene,view.revision)==1)
    (void)psm_presented(&model,revision);
  }
  if(!alive(NULL))break;
  psm_tick(&model);
  if(model.phase==PSM_RETAINED){retain(NULL);break;}
  if(drained&&risc_component_scene_snapshot(&scene)&&!host_step())break;
  if(!handoff&&!exiting)(void)publish();
  if(alive(NULL)&&!exiting)rt->yield_ms(1);
 }
 if(!alive(NULL))return;
 if(model_ready)psm_cancel(&model);
 while(alive(NULL)){
  if(model_ready)psm_tick(&model);
  if(model_ready&&model.phase==PSM_RETAINED){retain(NULL);return;}
  if(close_app())return;
  if(alive(NULL))rt->yield_ms(1);
 }
}
