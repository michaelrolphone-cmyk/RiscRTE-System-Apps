#include "../../Apps/driver_manager_scene.c"
#include <assert.h>
static risc_runtime_api_v1 runtime;
static risc_scene_components_api_v1 ui;
static driver_manager_v1 packages;
static driver_manager_snapshot_v1 package_state;
static driver_manager_row_v1 package_row;
static unsigned scenario,ticks,events,starts,steps,cancels,closes,releases,fences,live_grants;
static bool idle_turn,scene_open;
static uint64_t emitted;
static risc_components_document_v1 shown;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
static void yield(uint32_t ms){assert(ms);assert(++ticks<1000);}
static bool health(risc_runtime_health_v1*out){out->uptime_ms=ticks;return true;}
static bool fence(void){++fences;return true;}
static bool get(const char*cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out){
 assert(version==1&&!instance);
 const void*api=NULL;
 if(!strcmp(cap,"ui.scene"))api=&ui;
 if(!strcmp(cap,DRIVER_MANAGER_CAPABILITY)&&scenario)api=&packages;
 if(!api)return false;
 out->api=api;out->slot=++live_grants;out->generation=1;return true;
}
static bool drop(risc_runtime_capability_v1*g){assert(!scene_open);assert(!fences);++releases;
 g->slot=g->generation=0;g->api=NULL;return true;}
static int open_ui(void*ctx,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*out){
 (void)ctx;assert(n->depth==1);shown=*d;*out=9;scene_open=true;return 0;}
static int update_ui(void*ctx,uint64_t session,const risc_components_document_v1*d){
 (void)ctx;assert(session==9&&scene_open);assert(d->revision>shown.revision);shown=*d;return 0;}
static int configure(void*ctx,uint64_t session,uint32_t flags){(void)ctx;assert(session==9&&!flags);return 0;}
static int snapshot_ui(void*ctx,uint64_t session,risc_scene_navigation_v1*n,uint32_t*flags){
 (void)ctx;(void)n;assert(session==9&&scene_open);*flags=0;return 0;}
static int close_ui(void*ctx,uint64_t session){
 (void)ctx;assert(session==9);++closes;
 if(scenario==4&&closes<4)return RISC_SCENE_AGAIN;
 if(scenario==5)return RISC_SCENE_RETAINED;
 scene_open=false;return 0;
}
static int next_ui(void*ctx,uint64_t session,risc_scene_event_v1*e){
 (void)ctx;assert(session==9);
 if(!idle_turn){idle_turn=true;return RISC_SCENE_IDLE;}idle_turn=false;
 unsigned action=DM_BACK;
 if(scenario==1||scenario==2||scenario==3){
  const unsigned actions[]={DM_SELECT,DM_INSTALL,DM_CONFIRM,DM_BACK};
  action=actions[events<4?events:3];
  if(scenario==2&&events==3)action=DM_CANCEL;
 }else if(scenario==6)action=DM_BACK;
 ++events;e->kind=RISC_SCENE_ACTION_EVENT;e->document_revision=shown.revision;e->sequence=++emitted;
 e->action=action;e->node=0;
 for(unsigned i=0;i<shown.node_count;i++)if(shown.nodes[i].action==action){e->node=shown.nodes[i].id;break;}
 return 0;
}
static bool snap(void*ctx,driver_manager_snapshot_v1*out){
 (void)ctx;
 if(scenario==2&&model.view.confirming&&events>=3)++package_state.generation;
 *out=package_state;return true;
}
static bool row(void*ctx,uint32_t gen,uint32_t index,driver_manager_row_v1*out){
 (void)ctx;if(gen!=package_state.generation||index)return false;*out=package_row;return true;}
static int refresh(void*ctx,uint32_t source){(void)ctx;package_state.source=source;++package_state.generation;return 0;}
static int start(void*ctx,uint32_t op,const driver_manager_selection_v1*k,bool confirmed){
 (void)ctx;assert(op==DRIVER_MANAGER_INSTALL&&confirmed&&k->token==44);++starts;
 package_state.result=DRIVER_MANAGER_WORKING;package_state.custody=DRIVER_MANAGER_CUSTODY_PENDING;
 ++package_state.generation;return DRIVER_MANAGER_WORKING;}
static int step(void*ctx,const risc_storage_volume_api_v1*v){
 (void)ctx;assert(!v);++steps;
 if(scenario==3)return DRIVER_MANAGER_RETAINED;
 package_state.result=0;package_state.custody=0;return 0;}
static int cancel_package(void*ctx){
 (void)ctx;++cancels;if(scenario==6&&cancels<4)return DRIVER_MANAGER_WORKING;
 package_state.result=0;package_state.custody=0;return 0;}
static int ready(void*ctx,uint64_t utc){(void)ctx;(void)utc;assert(false);return -1;}
static void run(unsigned which){
 scenario=which;ticks=events=starts=steps=cancels=closes=releases=fences=live_grants=0;
 idle_turn=scene_open=false;emitted=0;
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE,
 .health=health,.yield_ms=yield,.acquire=get,.release=drop,.retain_invocation=fence};
 ui=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(ui),
 .next=next_ui,.snapshot=snapshot_ui,.close=close_ui},.configure=configure},
 .tag=RISC_COMPONENTS_TAG,.version=1,.open=open_ui,.update=update_ui};
 packages=(driver_manager_v1){1,sizeof(packages),NULL,snap,row,refresh,start,step,cancel_package,ready};
 package_state=(driver_manager_snapshot_v1){.generation=1,.count=1,.source=DRIVER_MANAGER_INBOX,.available_sources=7};
 package_row=(driver_manager_row_v1){.token=44,.flags=DRIVER_MANAGER_VALID|DRIVER_MANAGER_CAN_INSTALL};
 strcpy(package_row.id,"driver");strcpy(package_row.version,"1");
 if(which==6){package_state.result=DRIVER_MANAGER_WORKING;package_state.custody=DRIVER_MANAGER_CUSTODY_PENDING;}
 assert(app_module_init()==0);app_main();app_module_fini();
 if(which==3||which==5){assert(fences==1&&!releases);return;}
 assert(!fences&&!scene_open&&releases==(which?2u:1u));
 if(which==1)assert(starts==1&&steps==1);
 if(which==2)assert(!starts);
 if(which==4)assert(closes==4);
 if(which==6)assert(cancels>=4);
}
int main(void){for(unsigned i=0;i<7;i++)run(i);puts("Driver Manager actual native owner lifecycle PASS");return 0;}
