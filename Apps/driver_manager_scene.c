/* One native intent application; all dimensions and device behavior are providers. */
#include "driver_manager_controller.h"
#include "RiscRuntimeV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscRealtimeV1.h"
#include "PortableNetworkSession.h"
#include <limits.h>
static const risc_runtime_api_v1 *rt;
static bool retained;
static dm_controller model;
static driver_manager_row_v1 copied_rows[DM_ROWS_MAX];
static risc_components_document_v1 document;
static portable_network_session network;
static bool network_started,network_service;
__attribute__((visibility("default")))
const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={
 1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
static bool live(void){return !retained&&risc_runtime_get_api(1)==rt;}
static bool network_owner(void*unused){(void)unused;return live();}
static void retain(void){
 if(retained)return;
 retained=true;
 if(risc_runtime_get_api(1)==rt&&rt->retain_invocation)(void)rt->retain_invocation();
}
static bool acquire(const char*name,risc_runtime_capability_v1*g,bool*owned){
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};*owned=false;
 bool ok=rt->acquire(name,1,0,g);
 if(!live())return false;
 if(!ok){if(g->api||g->slot||g->generation)retain();return false;}
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation){retain();return false;}
 *owned=true;return true;
}
static bool release(risc_runtime_capability_v1*g){
 if(!live())return false;
 bool ok=rt->release(g);
 if(!live())return false;
 if(!ok||g->api||g->slot||g->generation){retain();return false;}
 return true;
}
static bool sync_model(const driver_manager_v1*p){
 driver_manager_snapshot_v1 s={0};
 bool ok=p->snapshot(p->context,&s);if(!live())return false;
 if(!ok||!driver_manager_snapshot_valid(&s)){retain();return false;}
 /* A retained provider forbids further metadata calls, too. */
 if(s.result==DRIVER_MANAGER_RETAINED||s.custody==DRIVER_MANAGER_CUSTODY_RETAINED){retain();return false;}
 memset(copied_rows,0,sizeof(copied_rows));
 for(unsigned i=0;i<s.count;i++){
  ok=p->row_get(p->context,s.generation,i,&copied_rows[i]);if(!live())return false;
  if(!ok||!driver_manager_row_valid(&copied_rows[i]))return false; /* retry atomic snapshot */
 }
 driver_manager_snapshot_v1 after={0};ok=p->snapshot(p->context,&after);if(!live())return false;
 if(!ok||!driver_manager_snapshot_valid(&after)){retain();return false;}
 if(after.result==DRIVER_MANAGER_RETAINED||after.custody==DRIVER_MANAGER_CUSTODY_RETAINED){retain();return false;}
 if(memcmp(&s,&after,sizeof(s)))return false;
 return dm_controller_copy(&model,&s,copied_rows);
}
static bool result_valid(int rc){
 if(!live())return false;
 if(rc==DRIVER_MANAGER_RETAINED){retain();return false;}
 if(rc!=DRIVER_MANAGER_OK&&rc!=DRIVER_MANAGER_WORKING&&rc!=DRIVER_MANAGER_NEEDS_NETWORK&&
 rc!=DRIVER_MANAGER_UNAVAILABLE&&rc!=DRIVER_MANAGER_INVALID&&rc!=DRIVER_MANAGER_STALE){retain();return false;}
 return true;
}
static bool close_scene(const risc_scene_api_v1*s,uint64_t session){
 for(;;){
  if(!live())return false;
  int rc=s->close(s->context,session);if(!live())return false;
  if(rc==RISC_SCENE_OK)return true;
  if(rc!=RISC_SCENE_AGAIN){retain();return false;}
  rt->yield_ms(2);
 }
}
static bool now_ms(uint32_t*out){
 if(rt->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE&&rt->monotonic_ms){
  bool ok=rt->monotonic_ms(out);return live()&&ok;
 }
 risc_runtime_health_v1 h={.struct_size=sizeof(h)};
 bool ok=rt->health(&h);if(live()&&ok){*out=h.uptime_ms;return true;}return false;
}
static bool network_close(void){
 if(network_service){
  int rc=portable_network_session_service_end(&network);
  if(!live()||rc==PORTABLE_NETWORK_SERVICE_RETAINED){retain();return false;}
  if(rc==PORTABLE_NETWORK_SERVICE_PENDING)return false;
  network_service=false;
 }
 if(network_started){
  int rc=portable_network_session_close(&network);
  if(!live()||rc==PORTABLE_NETWORK_CLOSE_RETAINED){retain();return false;}
  if(rc==PORTABLE_NETWORK_CLOSE_PENDING)return false;
  network_started=false;
 }
 return true;
}
static bool network_ready(const driver_manager_v1*p,const risc_realtime_api_v1*time,uint32_t now){
 if(!network_started){
  int rc=portable_network_session_start(&network,now);
  if(!live()||rc==PORTABLE_NETWORK_RETAINED){retain();return false;}
  network_started=true;
  if(rc<0)return false;
 }
 int rc=portable_network_session_step(&network,now);
 if(!live()||rc==PORTABLE_NETWORK_RETAINED){retain();return false;}
 if(rc!=PORTABLE_NETWORK_READY)return false;
 if(!time||time->api_version!=1||time->struct_size<sizeof(*time)||!time->read)return false;
 risc_realtime_snapshot_v1 utc={.struct_size=sizeof(utc)};
 rc=time->read(time->context,&utc);if(!live())return false;
 if(rc==RISC_REALTIME_CONTEXT){retain();return false;}
 if(rc!=RISC_REALTIME_OK||utc.struct_size!=sizeof(utc)||utc.validity!=RISC_REALTIME_VALID||
 utc.epoch_seconds<=0||utc.nanoseconds>=1000000000u||utc.reserved||utc.monotonic_after_us<utc.monotonic_before_us)return false;
 if(!network_service){
  rc=portable_network_session_service_begin(&network);
  if(!live()||rc==PORTABLE_NETWORK_SERVICE_RETAINED){retain();return false;}
  if(rc!=PORTABLE_NETWORK_SERVICE_OK)return false;
  network_service=true;
 }
 return result_valid(p->network_ready(p->context,(uint64_t)utc.epoch_seconds));
}
__attribute__((visibility("default"))) int app_module_init(void){
 rt=risc_runtime_get_api(1);retained=false;network_started=network_service=false;
 return !rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||
 !rt->acquire||!rt->release||!rt->retain_invocation||!rt->yield_ms||!rt->health?-1:0;
}
__attribute__((visibility("default"))) void app_module_fini(void){}
__attribute__((visibility("default"))) void app_main(void){
 if(!live())return;
 risc_runtime_capability_v1 grants[6]={{0}};bool owned[6]={0};
 const risc_scene_api_v1*scene=NULL;const risc_scene_components_api_v1*components=NULL;
 const driver_manager_v1*provider=NULL;const risc_storage_volume_api_v1*volume=NULL;
 const risc_realtime_api_v1*time=NULL;const wifi_api_v1*wifi=NULL;const risc_key_value_v1*kv=NULL;
 risc_scene_resident_v1 resident={0};uint64_t session=0,sequence=0;uint32_t revision=0;
 bool dirty=true,gate=true,leaving=false,cancel=false;dm_controller_init(&model);
 if(!acquire(RISC_SCENE_CAPABILITY,&grants[0],&owned[0]))goto cleanup;
 scene=grants[0].api;components=risc_scene_components_get_v1(scene);
 if(!components||!scene->next||!scene->snapshot||!scene->close||!components->lifecycle.configure)goto cleanup;
 if(acquire(DRIVER_MANAGER_CAPABILITY,&grants[1],&owned[1])){
  provider=grants[1].api;if(!driver_manager_api_ready(provider))provider=NULL;
 }
 if(!live())return;
 if(provider){
  if(acquire("storage.volume",&grants[2],&owned[2]))volume=grants[2].api;
  if(!live())return;
  if(acquire("net.wifi",&grants[3],&owned[3]))wifi=grants[3].api;
  if(!live())return;
  if(acquire("storage.key-value",&grants[4],&owned[4]))kv=grants[4].api;
  if(!live())return;
  if(acquire(RISC_REALTIME_CAPABILITY,&grants[5],&owned[5]))time=grants[5].api;
  if(!live())return;
  portable_network_session_init(&network,wifi,kv,network_owner,NULL);
  if(!sync_model(provider)&&!live())return;
 }
 if(!risc_scene_resident_bind_v1(rt,&resident)||!live()){retain();return;}
reopen:
 if(revision==UINT32_MAX){leaving=true;goto cleanup;}
 dm_document(&model.view,++revision,&document);
 risc_scene_navigation_v1 nav={.api_version=1,.struct_size=sizeof(nav),.depth=1,.routes={1}};
 int rc=components->open(scene->context,&document,&nav,&session);if(!live())return;
 if(rc!=RISC_SCENE_OK||!session){if(session||rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_RETAINED){retain();return;}goto cleanup;}
 rc=components->lifecycle.configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!live())return;
 if(rc!=RISC_SCENE_OK){retain();return;}gate=true;dirty=false;
 for(;;){
  if(!live())return;
  if(rt->struct_size>=RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE&&rt->exit_requested){
   if(rt->exit_requested()){leaving=true;cancel=true;}if(!live())return;
  }
  risc_scene_navigation_v1 state={.api_version=1,.struct_size=sizeof(state)};uint32_t flags=0;
  rc=scene->snapshot(scene->context,session,&state,&flags);if(!live())return;
  if(rc!=RISC_SCENE_OK){retain();return;}
  bool settled=!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY));
  if(provider){
   if(cancel){rc=provider->cancel(provider->context);if(!result_valid(rc))return;
    if(rc==DRIVER_MANAGER_OK&&sync_model(provider)&&dm_controller_clean(&model))cancel=false;
   }else if(model.snapshot.result==DRIVER_MANAGER_NEEDS_NETWORK){
    uint32_t now=0;
    if(now_ms(&now)&&!network_ready(provider,time,now)){
     if(!live())return;
     snprintf(model.view.status,sizeof(model.view.status),"Network or valid UTC unavailable; cancel keeps existing files.");
    }
   }else if(model.view.busy){
    rc=provider->step(provider->context,volume);if(!result_valid(rc))return;
   }
   driver_manager_snapshot_v1 before=model.snapshot;
   if(sync_model(provider)&&memcmp(&before,&model.snapshot,sizeof(before)))dirty=true;
   if(!live())return;
   if(dm_controller_clean(&model)&&!network_close()){if(!live())return;}
  }
  if(leaving&&(!provider||(!cancel&&dm_controller_clean(&model)))&&network_close())break;
  if(!live())return;
  risc_scene_event_v1 event={.struct_size=sizeof(event)};
  rc=scene->next(scene->context,session,&event);if(!live())return;
  if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE){retain();return;}
  if(rc==RISC_SCENE_IDLE&&settled)gate=false;
  if(rc==RISC_SCENE_OK&&event.struct_size==sizeof(event)&&event.sequence>sequence){
   sequence=event.sequence;
   if(settled&&!gate&&!leaving&&dm_event_allowed(&document,&event)){
    gate=true;dirty=true;unsigned action=event.action;
    if(action==DM_CANCEL){
     if(model.view.busy)cancel=true;
     else model.view.confirming=0;
    }else if(action==DM_BACK){
     if(model.view.busy){cancel=true;leaving=true;}
     else if(model.selected){model.selected=false;model.view.selected=-1;model.view.confirming=0;}
     else leaving=true;
    }else if(action>=DM_SELECT&&action<DM_SELECT+DM_ROWS_MAX){
     (void)dm_controller_select(&model,action-DM_SELECT);
    }else if(action==DM_CONFIRM&&provider){
     driver_manager_snapshot_v1 fresh={0};driver_manager_row_v1 row={0};uint32_t op=0;driver_manager_selection_v1 selection;
     bool ok=provider->snapshot(provider->context,&fresh);if(!live())return;
     if(ok&&(fresh.result==DRIVER_MANAGER_RETAINED||fresh.custody==DRIVER_MANAGER_CUSTODY_RETAINED)){retain();return;}
     if(ok&&model.selected&&driver_manager_snapshot_valid(&fresh)){
      ok=provider->row_get(provider->context,fresh.generation,model.selection.index,&row);if(!live())return;
      if(ok&&dm_controller_confirm(&model,&fresh,&row,&op,&selection)){
       rc=provider->start(provider->context,op,&selection,true);if(!result_valid(rc))return;
      }
     }
    }else if(action==DM_REFRESH||(action>=DM_ONLINE_TAB&&action<=DM_RECOVERY_TAB)){
     unsigned source=action==DM_REFRESH?model.view.source:action-DM_ONLINE_TAB;
     model.selected=false;model.view.selected=-1;model.view.confirming=0;model.view.source=source;
     if(provider){rc=provider->refresh(provider->context,source);if(!result_valid(rc))return;}
    }else (void)dm_controller_prepare(&model,action);
   }
  }
  if(settled&&dirty){
   if(revision==UINT32_MAX){leaving=true;cancel=true;continue;}
   dm_document(&model.view,++revision,&document);
   rc=components->update(scene->context,session,&document);if(!live())return;
   if(rc!=RISC_SCENE_OK){retain();return;}dirty=false;
  }
  if(settled&&!model.view.busy&&!network_started&&!network_service&&!leaving){
   rc=risc_scene_resident_poll_v1(&resident,flags);if(!live()||rc==RISC_RESIDENT_RETAINED){retain();return;}
   if((rc==RISC_RESIDENT_OK&&resident.policy)||(event.kind==RISC_SCENE_CONTROLS_EVENT&&event.document_revision==revision&&rc==RISC_RESIDENT_OK)){
    if(!close_scene(scene,session))return;
    session=0;
    rc=risc_scene_resident_dispatch_v1(&resident,resident.policy?RISC_RESIDENT_CHECKPOINT_POLICY:RISC_RESIDENT_CHECKPOINT_CONTROLS);
    if(!live()||rc==RISC_RESIDENT_RETAINED){retain();return;}
    goto reopen;
   }
  }
  rt->yield_ms(2);
 }
cleanup:
 if(!live())return;
 if(session&&!close_scene(scene,session))return;
 if(provider){
  for(;;){
   rc=provider->cancel(provider->context);if(!result_valid(rc))return;
   if(rc==DRIVER_MANAGER_OK&&sync_model(provider)&&dm_controller_clean(&model))break;
   rt->yield_ms(2);if(!live())return;
  }
 }
 while(!network_close()){if(!live())return;rt->yield_ms(2);}
 for(unsigned n=6;n;n--)if(owned[n-1]&&!release(&grants[n-1]))return;
}
