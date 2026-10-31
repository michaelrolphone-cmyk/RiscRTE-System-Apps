/* One semantic foreground application. Providers own pixels, input and power. */
#include "app_store_controller.h"
#include "app_store_inventory.h"
#include "app_store_background.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
static const risc_runtime_api_v1 *as_runtime;
static bool as_terminal,as_leave;
static app_store_controller asc;
static app_store_inventory as_inventory;
static app_store_background as_background;
static bool as_background_initialized;
static app_store_inventory_selection as_selection;
static bool as_inventory_opened,as_selection_valid,as_can_launch,as_feed_available,as_feed_enabled;
static char as_launch[193];
static char as_selected_identity[96],as_selected_version[64],as_selected_route[193];
static risc_runtime_capability_v1 as_scene_grant;
static const risc_scene_api_v1 *as_scene;
static const risc_scene_components_api_v1 *as_components;
static const risc_scene_lifecycle_api_v1 *as_lifecycle;
static risc_scene_resident_v1 as_resident;
static risc_scene_navigation_v1 as_nav;
static risc_components_document_v1 as_document,as_candidate;
static uint64_t as_session,as_sequence;
static uint32_t as_revision,as_flags;
static bool as_explicit;
#include "app_store_presenter.inc"
static bool as_alive(void *context){
 (void)context;
 if(as_terminal||!as_runtime||risc_runtime_get_api(1)!=as_runtime){as_terminal=true;return false;}
 return true;
}
static void as_retain(void *context){
 (void)context;
 if(!as_alive(NULL))return;
 as_terminal=true;
 if(as_runtime->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&as_runtime->retain_invocation)
  (void)as_runtime->retain_invocation();
}
static bool as_result(int rc){if(!as_alive(NULL))return false;if(rc==RISC_SCENE_RETAINED){as_retain(NULL);return false;}return true;}
static bool as_close_scene(void){
 if(!as_alive(NULL))return false;
 for(unsigned i=0;as_session&&i<256;i++){
  int rc=as_scene->close(as_scene->context,as_session);
  if(!as_result(rc))return false;
  if(rc==RISC_SCENE_OK){as_session=0;as_sequence=0;as_flags=0;return true;}
  if(rc!=RISC_SCENE_AGAIN){as_retain(NULL);return false;}
  as_runtime->yield_ms(1);if(!as_alive(NULL))return false;
 }
 if(as_session){as_retain(NULL);return false;}return true;
}
/* Context workflow integration is still a separate unfinished boundary. */
static bool as_pause(void *context){
 (void)context;
 if(as_background_initialized){int rc=app_store_background_pause(&as_background);if(!as_alive(NULL))return false;if(rc==BATTERY_BROADCAST_RETAINED){as_retain(NULL);return false;}if(rc!=BATTERY_BROADCAST_OK&&rc!=BATTERY_BROADCAST_UNAVAILABLE){as_retain(NULL);return false;}}
 return as_close_scene();
}
static bool as_network_needed(void *context){(void)context;return as_alive(NULL)&&as_inventory_opened&&app_store_inventory_network_needed(&as_inventory);}
static bool as_network_ready(void *context,uint64_t utc){
 (void)context;int rc=app_store_inventory_network_ready(&as_inventory,utc);
 if(!as_alive(NULL))return false;
 if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return false;}
 return rc==APP_STORE_INVENTORY_OK||rc==APP_STORE_INVENTORY_PENDING;
}
static bool as_snapshot(void){
 if(!as_alive(NULL)||!as_session)return false;
 risc_scene_navigation_v1 navigation={.api_version=1,.struct_size=sizeof(navigation)};uint32_t flags=0;
 int rc=as_scene->snapshot(as_scene->context,as_session,&navigation,&flags);
 if(!as_result(rc))return false;
 if(rc!=RISC_SCENE_OK||navigation.api_version!=1||navigation.struct_size!=sizeof(navigation)||navigation.depth!=1||navigation.reserved||navigation.routes[0]!=1||(flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))){as_retain(NULL);return false;}
 for(unsigned i=1;i<RISC_SCENE_MAX_DEPTH;i++)if(navigation.routes[i]||navigation.focus[i]){as_retain(NULL);return false;}
 as_nav=navigation;as_flags=flags;return true;
}
static bool as_publish(void){
 if(!as_alive(NULL))return false;
 if(as_session&&!as_snapshot())return false;
 if(as_session&&(as_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING)))return true;
 if(as_session&&!as_lifecycle&&!as_explicit)return true;
 asp_build(&as_candidate);if(!as_alive(NULL)||!asc_live(&asc))return false;
 as_candidate.routes[0].back_action=ASP_BACK;
 as_candidate.revision=++as_revision;if(!as_revision){as_retain(NULL);return false;}
 int rc;
 if(!as_session){
  uint32_t focus=as_nav.focus[0];bool valid=as_document.screen_key==as_candidate.screen_key;
  if(valid&&focus){valid=false;for(unsigned i=0;i<as_candidate.node_count;i++)if(as_candidate.nodes[i].id==focus&&as_candidate.nodes[i].action)valid=true;}
  as_nav=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(as_nav),.depth=1,.routes={1},.focus={valid?focus:0}};
  rc=as_components->open(as_scene->context,&as_candidate,&as_nav,&as_session);
  if(!as_result(rc))return false;
  if(rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN)return true;
  if(rc!=RISC_SCENE_OK||!as_session){as_leave=true;return false;}
  as_sequence=0;
  if(as_lifecycle){rc=as_lifecycle->configure(as_scene->context,as_session,as_resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!as_result(rc))return false;if(rc!=RISC_SCENE_OK){as_retain(NULL);return false;}}
 }else{
  rc=as_components->update(as_scene->context,as_session,&as_candidate);
  if(!as_result(rc))return false;
  if(rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN)return true;
  if(rc!=RISC_SCENE_OK){as_retain(NULL);return false;}
 }
 as_document=as_candidate;asp_commit();asc.dirty=false;as_explicit=false;return true;
}
static bool as_cancel(void){
 if(!as_pause(NULL))return false;
 if(as_inventory_opened){
  int rc=app_store_inventory_cancel(&as_inventory);if(!as_alive(NULL))return false;
  if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return false;}
  if(rc!=APP_STORE_INVENTORY_OK){asc.cleanup_pending=true;asc_notice(&asc,"Finishing inventory cleanup; retry");return false;}
 }
 bool ok=asc_cancel(&asc);if(!as_alive(NULL))return false;as_selection_valid=false;as_explicit=true;return ok;
}
static void as_dispatch(uint32_t action){
 if(!asp_action(action))return;
 as_explicit=true;
 if(action>=ASP_SELECT_BASE&&action<ASP_SELECT_BASE+SOFTWARE_UPDATE_ROWS_MAX){
  unsigned index=action-ASP_SELECT_BASE;as_selection_valid=as_can_launch=false;as_selected_identity[0]=as_selected_version[0]=as_selected_route[0]=0;
  if(asc_select(&asc,index)&&as_inventory_opened){int rc=app_store_inventory_choose(&as_inventory,index,&as_selection);if(!as_alive(NULL))return;if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return;}as_selection_valid=rc==APP_STORE_INVENTORY_OK;as_can_launch=as_selection_valid&&as_selection.kind==APP_STORE_INSTALLED&&as_selection.app.available;
   if(as_selection_valid&&as_selection.kind!=APP_STORE_RELEASE){snprintf(as_selected_identity,sizeof(as_selected_identity),"%s",as_selection.app.id);snprintf(as_selected_version,sizeof(as_selected_version),"%s",as_selection.app.version);snprintf(as_selected_route,sizeof(as_selected_route),"%s",as_selection.app.launch_path);}
   else if(!as_selection_valid){asc.confirming=false;asc_notice(&asc,"Catalog changed; select again");}
  }
 }else if(action==ASP_CONFIRM){
  if(as_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING))return;
  if(as_selection_valid&&as_selection.kind!=APP_STORE_RELEASE){
   if(!as_pause(NULL))return;
   int rc=app_store_inventory_confirm(&as_inventory,&as_selection);if(!as_alive(NULL))return;
   if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return;}
   asc.confirming=false;asc.dirty=true;
   if(rc!=APP_STORE_INVENTORY_OK&&rc!=APP_STORE_INVENTORY_PENDING)asc_notice(&asc,"App changed or storage unavailable; select again");
   (void)asc_status(&asc);
  }else (void)asc_confirm(&asc);
 }else if(action==ASP_OPEN){
  if(!as_can_launch||!as_selection_valid||!as_pause(NULL))return;
  int rc=app_store_inventory_launch_route(&as_inventory,&as_selection,as_launch,sizeof(as_launch));
  if(!as_alive(NULL))return;
  if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return;}
  if(rc!=APP_STORE_INVENTORY_OK){as_launch[0]=0;asc_notice(&asc,"Installed app changed; select again");return;}
  if(as_cancel())as_leave=true;else as_launch[0]=0;
 }else if(action==ASP_SOURCE){
  if(!as_feed_available||!as_cancel())return;
  int rc=app_store_inventory_set_feed(&as_inventory,!as_feed_enabled);if(!as_alive(NULL))return;
  if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return;}
  if(rc==APP_STORE_INVENTORY_OK){as_feed_enabled=!as_feed_enabled;asc.page=0;asc_notice(&asc,as_feed_enabled?"Check will refresh verified package feed":"Check will refresh release updates");(void)asc_status(&asc);}
 }else if(action==ASP_RETRY_NETWORK){asc.network_wait=false;(void)asc_connect_mode(&asc,false,true);}
 else if(action==ASP_CHECK)(void)asc_connect(&asc,false);
 else if(action==ASP_CANCEL)(void)as_cancel();
 else if(action==ASP_RESTART)(void)asc_restart(&asc);
 else if(action==ASP_BACK){
  if(asc.confirming){asc.confirming=false;as_selection_valid=false;asc.dirty=true;}
  else if(as_cancel())as_leave=true;
 }
}
static void as_event(const risc_scene_event_v1 *event){
 if(event->struct_size!=sizeof(*event)||!event->sequence||event->sequence<=as_sequence)return;
 as_sequence=event->sequence;
 if(event->document_revision!=as_document.revision)return;
 if(event->kind==RISC_SCENE_SUSPEND_EVENT){if(as_cancel())as_leave=true;return;}
 if(event->kind==RISC_SCENE_CONTROLS_EVENT){
  if(!as_resident.enabled||asc.restart_pending)return;
  if(!as_cancel())return;
  uint32_t flags=0;int rc=risc_scene_resident_dispatch_reply_v1(&as_resident,RISC_RESIDENT_CHECKPOINT_CONTROLS,&flags);
  if(!as_alive(NULL))return;
  if(rc==RISC_RESIDENT_RETAINED){as_retain(NULL);return;}
  if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF)as_leave=true;
  asc.dirty=as_explicit=true;return;
 }
 if(event->kind!=RISC_SCENE_ACTION_EVENT)return;
 bool valid=event->node==0&&(event->action==ASP_BACK||(as_document.cancel_action&&event->action==as_document.cancel_action));
 for(unsigned i=0;i<as_document.node_count;i++){const risc_scene_node_v1*n=&as_document.nodes[i];if(n->id==event->node&&n->action==event->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)))valid=true;}
 if(valid)as_dispatch(event->action);
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) int app_main(void){
 as_runtime=risc_runtime_get_api(1);as_terminal=as_leave=false;as_scene=NULL;as_components=NULL;as_lifecycle=NULL;as_session=as_sequence=as_revision=as_flags=0;as_explicit=true;as_background_initialized=false;as_inventory_opened=as_selection_valid=as_can_launch=as_feed_available=as_feed_enabled=false;as_launch[0]=0;as_selected_identity[0]=as_selected_version[0]=as_selected_route[0]=0;
 memset(&asc,0,sizeof(asc));memset(&as_inventory,0,sizeof(as_inventory));memset(&as_scene_grant,0,sizeof(as_scene_grant));memset(&as_document,0,sizeof(as_document));memset(&as_nav,0,sizeof(as_nav));
 if(!as_runtime||as_runtime->api_version!=1||as_runtime->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!as_runtime->acquire||!as_runtime->release||!as_runtime->yield_ms||!as_runtime->monotonic_ms)return 0;
 asc.runtime=as_runtime;asc.hooks=(app_store_hooks){.alive=as_alive,.retain=as_retain,.pause=as_pause,.network_needed=as_network_needed,.network_ready=as_network_ready};
 if(!asc_acquire(&asc,RISC_SCENE_CAPABILITY,1,0,&as_scene_grant))return 0;
 as_scene=as_scene_grant.api;as_components=risc_scene_components_get_v1(as_scene);as_lifecycle=risc_scene_lifecycle_get_v1(as_scene);
 if(!as_components||!as_scene->next||!as_scene->snapshot||!as_scene->close){(void)asc_release(&asc,&as_scene_grant);return 0;}
 bool bound=risc_scene_resident_bind_v1(as_runtime,&as_resident);if(!as_alive(NULL))return 0;if(!bound){as_retain(NULL);return 0;}
 app_store_hooks hooks=asc.hooks;
 if(!asc_init(&asc,as_runtime,hooks)){if(!as_alive(NULL)||asc.terminal)return 0;as_leave=true;}
 if(!as_leave){app_store_inventory_init(&as_inventory,as_runtime,(app_store_inventory_hooks){NULL,as_alive,as_retain});int rc=app_store_inventory_open(&as_inventory,asc.release);if(!as_alive(NULL))return 0;if(rc==APP_STORE_INVENTORY_RETAINED){as_retain(NULL);return 0;}as_inventory_opened=rc==APP_STORE_INVENTORY_OK||rc==APP_STORE_INVENTORY_UNAVAILABLE;if(as_inventory_opened){asc.service=app_store_inventory_view(&as_inventory);as_feed_available=as_inventory.available_feed!=NULL;(void)asc_status(&asc);}}
 if(!as_leave){int rc=app_store_background_init(&as_background,as_runtime,as_alive,as_retain,NULL);if(!as_alive(NULL))return 0;if(rc==BATTERY_BROADCAST_RETAINED){as_retain(NULL);return 0;}as_background_initialized=true;}
 while(as_alive(NULL)&&!as_leave){
  if(!as_session||asc.dirty){if(!as_publish()&&!as_alive(NULL))break;}
  bool queue_drained=true;
  if(as_session){
   if(!as_snapshot())break;
   int rc;
   queue_drained=false;
   for(unsigned i=0;i<32&&as_session&&as_alive(NULL)&&!as_leave;i++){
    risc_scene_event_v1 event={.struct_size=sizeof(event)};rc=as_scene->next(as_scene->context,as_session,&event);if(!as_result(rc))break;
    if(rc==RISC_SCENE_OK)as_event(&event);
    else if(rc==RISC_SCENE_IDLE){queue_drained=true;break;}
    else if(rc==RISC_SCENE_AGAIN)break;
    else {as_retain(NULL);break;}
   }
   if(!as_session)queue_drained=true;
  }
  if(!as_alive(NULL)||as_leave)break;
  /* User cancellation is consumed before any VERIFYING -> READY progress. */
  if(queue_drained&&!(as_flags&RISC_SCENE_INPUT_BUSY)&&!asc_tick(&asc)&&(!as_alive(NULL)||asc.terminal))break;
  if(!as_alive(NULL))break;
  if(as_inventory_opened&&as_inventory.result&&!as_inventory.operation&&!as_inventory.draining&&!asc.connecting&&!asc.cleanup_pending&&!asc.restart_pending&&strcmp(asc.message,as_inventory.result))asc_notice(&asc,as_inventory.result);
  uint32_t work=(!queue_drained||asc.confirming||asc.connecting||asc.cleanup_pending||asc.restart_pending||(asc.status.state>=SOFTWARE_UPDATE_CATALOG&&asc.status.state<=SOFTWARE_UPDATE_READY&&asc.status.state!=SOFTWARE_UPDATE_LIST))?(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY):0;
  if(as_background_initialized){int rc=app_store_background_step(&as_background,!work&&!(as_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING)));if(!as_alive(NULL))break;if(rc==BATTERY_BROADCAST_RETAINED){as_retain(NULL);break;}}
  int rr=risc_scene_resident_poll_work_v1(&as_resident,as_flags,work);if(!as_alive(NULL))break;if(rr==RISC_RESIDENT_RETAINED){as_retain(NULL);break;}
  if(as_resident.policy&&!work&&as_cancel()){uint32_t reply=0;rr=risc_scene_resident_dispatch_reply_v1(&as_resident,RISC_RESIDENT_CHECKPOINT_POLICY,&reply);if(!as_alive(NULL))break;if(rr==RISC_RESIDENT_RETAINED){as_retain(NULL);break;}if(rr==RISC_RESIDENT_EXIT||rr==RISC_RESIDENT_HANDOFF)as_leave=true;as_explicit=asc.dirty=true;}
  if(as_runtime->struct_size>=RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE&&as_runtime->exit_requested&&as_runtime->exit_requested()){if(!as_alive(NULL))break;if(as_cancel())as_leave=true;}
  if(as_alive(NULL)&&!as_leave)as_runtime->yield_ms(8);
 }
 if(!as_alive(NULL))return 0;
 if(!as_pause(NULL))return 0;
 if(as_background_initialized){int rc=app_store_background_close(&as_background);if(!as_alive(NULL))return 0;if(rc!=BATTERY_BROADCAST_OK&&rc!=BATTERY_BROADCAST_UNAVAILABLE){as_retain(NULL);return 0;}as_background_initialized=false;}
 if(as_inventory_opened){int rc=app_store_inventory_close(&as_inventory);if(!as_alive(NULL))return 0;if(rc!=APP_STORE_INVENTORY_OK){as_retain(NULL);return 0;}asc.service=asc.release;}
 if(!asc_close(&asc)){if(as_alive(NULL))as_retain(NULL);return 0;}
 if(!asc_release(&asc,&as_scene_grant))return 0;
 if(as_launch[0]&&as_alive(NULL)&&as_runtime->request_launch){(void)as_runtime->request_launch(as_launch);(void)as_alive(NULL);}
 return 0;
}
