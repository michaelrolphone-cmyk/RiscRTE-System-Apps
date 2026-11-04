/* Real application loop + real model/view/checked client. Runtime and Settings
 * backend are bounded endpoint doubles, not hardware/native-resolution proof. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/settings_scene.c"
extern bool settings_scene_production_valid(const risc_components_document_v1 *);
enum { SUCCESS,CANCEL_BATCH,CONTROLS,REENTER,LOST_IDENTITY,CLOSE_RETAINED,RELEASE_DIRTY,HOST_EXIT,REENTRY_CLEAN,SUSPEND_PENDING,PUBLISH_AGAIN,ROOT_RELOAD,STORAGE_RELOAD,RELOAD_FAIL,CONTROLS_CLOSE_AGAIN,SAVE_ERROR_SUSPEND,SAVE_ACTIVITY_LATCH };
static unsigned mode,cycles,opens,updates,closes,releases,backend_closes,writes,retains,dispatches,sequence;
static unsigned stage,wait_cycles,again_closes;
static bool identity=true,callback_reentered=false,stored=false,batch_blocked=false;
static unsigned delayed_publications,reload_calls,activity_snapshots,activity_polls;
static uint8_t record[64];static uint32_t record_size,provider_flags;
static risc_components_document_v1 visible;
static risc_scene_event_v1 queue[128];static unsigned head,tail;
static const risc_runtime_api_v1 mock_rt;
static risc_scene_components_api_v1 provider;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&identity?&mock_rt:NULL;}
static bool backend_live(void *p){(void)p;return identity&&!retained;}
static int32_t kv_get(void *p,const char *key,void *out,uint32_t capacity,uint32_t *size){(void)p;*size=0;if(strcmp(key,"time_format")||!stored)return RISC_KEY_VALUE_NOT_FOUND;*size=record_size;if(capacity<record_size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,record,record_size);return RISC_KEY_VALUE_OK;}
static int32_t kv_put(void *p,const char *key,const void *data,uint32_t size){(void)p;assert(!strcmp(key,"time_format"));assert(size<=sizeof(record));assert(model.phase==PSM_COMMITTING);assert(model.save_revision&&scene.committed.revision==view.revision);assert(!provider_flags);assert(head==tail);bool notice=false;for(unsigned i=0;i<visible.node_count;i++)if(strstr(visible.nodes[i].text,"Saving - not confirmed"))notice=true;assert(notice&&view.presented_save_revision==model.save_revision);writes++;if(mode==SAVE_ERROR_SUSPEND)return RISC_KEY_VALUE_IO;memcpy(record,data,size);record_size=size;stored=true;return RISC_KEY_VALUE_OK;}
static risc_key_value_v1 mock_kv={1,sizeof(mock_kv),NULL,kv_get,kv_put};
bool psr_open(PortableSettingsRuntime *b,const risc_runtime_api_v1 *rt,const portable_settings_policy_v1 *p,portable_realtime_phase_guard g,void (*r)(void *),void *c){(void)p;assert(rt==&mock_rt&&g&&r);b->runtime=rt;b->guard=g;b->retain=r;b->context=c;b->opened=true;return true;}
bool psr_init_model(PortableSettingsRuntime *b,PortableSettingsModel *m){(void)b;psm_policy p=psm_default_policy();psm_services services={.live=backend_live};return psm_init(m,&p,&services,&mock_kv,NULL);}
bool psr_reload(PortableSettingsRuntime *b,PortableSettingsModel *m){(void)b;assert(m->phase==PSM_IDLE&&!provider_flags&&head==tail&&view.revision==scene.committed.revision);reload_calls++;if(mode==RELOAD_FAIL)return false;if(mode==ROOT_RELOAD){uint32_t before=m->revision;record[0]=0x54;record[1]=1;record[2]=1;record[3]=0xa4;record_size=4;stored=true;assert(psr_init_model(b,m));assert(m->revision==before);return true;}if(m->route==PSM_STORAGE_ROUTE)psm_cancel(m);return psm_refresh(m);}
bool psr_close(PortableSettingsRuntime *b,const PortableSettingsModel *m){assert(identity&&!retained);assert(!scene.token);assert(!scene.grant.api);if(m)assert(psm_can_handoff(m));if(mode==SAVE_ERROR_SUSPEND)assert(m&&m->dirty&&m->uncertain&&m->status[PSM_FORMAT]==PSM_UNCONFIRMED);if(b->opened){backend_closes++;b->opened=false;}return true;}
static bool mock_retain(void){retains++;return true;}
static void event_label(const char *name){const risc_scene_node_v1 *n=NULL;for(unsigned i=0;i<visible.node_count;i++)if(!strcmp(visible.nodes[i].label,name)){n=&visible.nodes[i];break;}assert(n&&n->action&&!(n->flags&RISC_SCENE_DISABLED));assert(tail<128);queue[tail++]=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=n->kind==RISC_COMPONENT_CHECK_ROW?RISC_SCENE_VALUE_EVENT:RISC_SCENE_ACTION_EVENT,.value=n->kind==RISC_COMPONENT_CHECK_ROW?!n->value:0,.document_revision=visible.revision,.node=n->id,.action=n->action,.sequence=++sequence};}
static void special(unsigned kind){assert(tail<128);queue[tail++]=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=kind,.document_revision=visible.revision,.sequence=++sequence};}
static bool mock_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){assert(identity&&!retained);assert(!strcmp(name,"ui.scene")&&version==1&&!instance);g->api=&provider;g->slot=1;g->generation=1;return true;}
static bool mock_release(risc_runtime_capability_v1 *g){assert(identity&&!retained&&!scene.token);releases++;if(mode!=RELEASE_DIRTY)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static int32_t mock_open(void *p,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *n,uint64_t *token){(void)p;assert(!n&&identity&&!retained);assert(settings_scene_production_valid(d));opens++;visible=*d;*token=7;if(mode==REENTER&&!callback_reentered){callback_reentered=true;app_main();}if(mode==LOST_IDENTITY)identity=false;return RISC_SCENE_OK;}
static int32_t mock_update(void *p,uint64_t token,const risc_components_document_v1 *d){(void)p;assert(token==7&&identity&&!retained);assert(d->revision>visible.revision&&d->root==visible.root);assert(settings_scene_production_valid(d));updates++;if(mode==PUBLISH_AGAIN&&model.phase==PSM_WAIT_PRESENTATION&&delayed_publications<2){delayed_publications++;return RISC_SCENE_AGAIN;}visible=*d;return RISC_SCENE_OK;}
static int32_t mock_configure(void *p,uint64_t token,uint32_t features){(void)p;assert(token==7&&identity&&!retained);assert(features==(mode==CONTROLS||mode==HOST_EXIT||mode==CONTROLS_CLOSE_AGAIN||mode==SAVE_ACTIVITY_LATCH?1u:0u));return RISC_SCENE_OK;}
static int32_t mock_snapshot(void *p,uint64_t token,risc_scene_navigation_v1 *n,uint32_t *flags){(void)p;assert(token==7&&identity&&!retained);n->depth=1;n->routes[0]=visible.root;*flags=provider_flags;if(provider_flags&RISC_SCENE_ACTIVITY){assert(mode==SAVE_ACTIVITY_LATCH&&model.phase==PSM_WAIT_PRESENTATION&&view.presented_save_revision==model.save_revision);activity_snapshots++;}provider_flags&=~RISC_SCENE_ACTIVITY;return RISC_SCENE_OK;}
static int32_t mock_next(void *p,uint64_t token,risc_scene_event_v1 *event){(void)p;assert(token==7&&identity&&!retained);if(head==tail){head=tail=0;return RISC_SCENE_IDLE;}*event=queue[head++];return RISC_SCENE_OK;}
static int32_t mock_close(void *p,uint64_t token){(void)p;assert(token==7&&identity&&!retained);closes++;if(mode==CLOSE_RETAINED)return RISC_SCENE_RETAINED;if(again_closes){again_closes--;if(mode==CONTROLS_CLOSE_AGAIN)provider_flags=RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY;return RISC_SCENE_AGAIN;}provider_flags=0;/* Provider close owns draining input. */head=tail=0;sequence=0;return RISC_SCENE_OK;}
static int32_t checkpoint(uint64_t invocation,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply){assert(invocation==1&&identity&&!retained);reply->flags=0;if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL){if(request->flags&RISC_RESIDENT_POLL_ACTIVITY){assert(mode==SAVE_ACTIVITY_LATCH&&writes==1);activity_polls++;}return RISC_RESIDENT_OK;}assert(request->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);assert(!scene.token&&model.dirty&&model.draft.number==1);dispatches++;return mode==HOST_EXIT?RISC_RESIDENT_EXIT:RISC_RESIDENT_OK;}
static bool mock_resident(risc_resident_client_v1 *c){if(mode!=CONTROLS&&mode!=HOST_EXIT&&mode!=CONTROLS_CLOSE_AGAIN&&mode!=SAVE_ACTIVITY_LATCH)return false;*c=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*c),.invocation=1,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint};return true;}
static void mock_yield(uint32_t ms){assert(ms==1&&identity&&!retained);assert(++cycles<80);
 if(mode>=ROOT_RELOAD&&mode<=RELOAD_FAIL){
  if(stage==0){event_label(mode==STORAGE_RELOAD?"Settings storage":"Reload");if(mode!=STORAGE_RELOAD)provider_flags=RISC_SCENE_PRESENTING;stage=20;return;}
  if(stage==20&&mode==STORAGE_RELOAD){assert(model.route==PSM_STORAGE_ROUTE);event_label("Reload");provider_flags=RISC_SCENE_PRESENTING;stage=21;return;}
  if(stage==20||stage==21){assert(!reload_calls);provider_flags=0;stage=22;return;}
  if(stage==22){assert(mode!=RELOAD_FAIL);assert(reload_calls==1&&model.route==PSM_ROOT);if(mode==ROOT_RELOAD){bool updated=false;for(unsigned i=0;i<visible.node_count;i++)if(!strcmp(visible.nodes[i].label,"Time Format")&&!strcmp(visible.nodes[i].text,"24-hour"))updated=true;assert(updated&&updates>0);}event_label("Done");stage=23;return;}
  return;
 }
 if(mode==REENTRY_CLEAN||mode==CLOSE_RETAINED||mode==RELEASE_DIRTY){if(!stage){event_label("Done");stage=1;}return;}
 if(stage==0){event_label("Time Format");stage=1;return;}
 if(stage==1){assert(model.route==PSM_EDITOR);event_label("24-hour");stage=2;return;}
 if(stage==2){assert(model.dirty&&model.draft.number==1);if(mode==CONTROLS||mode==HOST_EXIT||mode==CONTROLS_CLOSE_AGAIN){special(RISC_SCENE_CONTROLS_EVENT);stage=10;}else{event_label("Save");stage=3;}return;}
 if(stage==10){if(mode==CONTROLS_CLOSE_AGAIN&&!dispatches){assert(scene.closing&&scene.token&&provider_flags==(RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY));assert(closes>=1&&closes<=2);return;}if(mode==HOST_EXIT){assert(!"Host EXIT must not reopen or yield");}assert(dispatches==1&&opens==2&&model.dirty&&model.draft.number==1);event_label("Cancel");stage=6;return;}
 if(stage==3){assert(model.phase==PSM_WAIT_PRESENTATION&&!writes);if(!pss_save_revision(&view,visible.revision)){assert(mode==PUBLISH_AGAIN&&delayed_publications);return;}assert(pss_save_revision(&view,visible.revision)==model.save_revision);provider_flags=RISC_SCENE_PRESENTING|(mode==SAVE_ACTIVITY_LATCH?RISC_SCENE_ACTIVITY:0);stage=4;return;}
 if(stage==4){assert(!writes&&model.phase==PSM_WAIT_PRESENTATION);if(++wait_cycles<3){provider_flags=wait_cycles==1?RISC_SCENE_INPUT_BUSY:RISC_SCENE_CLOSING;return;}provider_flags=0;
  if(mode==CANCEL_BATCH){for(unsigned i=0;i<33;i++){special(RISC_SCENE_ACTION_EVENT);queue[tail-1].document_revision=visible.revision-1;}event_label("Cancel operation");stage=5;}
  else if(mode==SUSPEND_PENDING){special(RISC_SCENE_SUSPEND_EVENT);stage=7;}
  else {event_label("Cancel operation");queue[tail-1].document_revision=visible.revision-1;stage=6;}
  return;
 }
 if(stage==5){assert(!writes);if(head!=tail){assert(model.phase==PSM_WAIT_PRESENTATION);batch_blocked=true;return;}assert(model.route==PSM_ROOT);event_label("Done");stage=7;return;}
 if(stage==6){if(mode==SAVE_ERROR_SUSPEND){assert(model.route==PSM_EDITOR&&model.phase==PSM_IDLE&&model.dirty&&model.uncertain&&writes==1&&!stored);special(RISC_SCENE_SUSPEND_EVENT);stage=7;return;}assert(model.route==PSM_ROOT);assert(writes==((mode==CONTROLS||mode==CONTROLS_CLOSE_AGAIN)?0u:1u));if(writes)assert(model.confirmed[PSM_FORMAT].number==1&&model.has_confirmed[PSM_FORMAT]);event_label("Done");stage=7;return;}
}
static const risc_runtime_api_v1 mock_rt={.api_version=1,.struct_size=sizeof(mock_rt),.acquire=mock_acquire,.release=mock_release,.yield_ms=mock_yield,.retain_invocation=mock_retain,.resident_shell=mock_resident};
int main(int argc,char **argv){assert(argc==2);mode=(unsigned)atoi(argv[1]);provider=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(provider),.snapshot=mock_snapshot,.next=mock_next,.close=mock_close},.configure=mock_configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=mock_open,.update=mock_update};again_closes=(mode==SUCCESS||mode==CONTROLS_CLOSE_AGAIN)?2:0;app_main();
 if(mode==REENTER||mode==CLOSE_RETAINED||mode==RELEASE_DIRTY){assert(retained&&!clean&&retains==1&&!backend_closes);unsigned before=opens+closes+releases;app_main();assert(opens+closes+releases==before);}
 else if(mode==LOST_IDENTITY){assert(retained&&!clean&&!releases&&!backend_closes&&!retains);}
 else {assert(clean&&!retained&&releases==1&&backend_closes==1);if(mode==SUCCESS)assert(writes==1&&closes==3);if(mode==CANCEL_BATCH||mode==SUSPEND_PENDING)assert(!writes);if(mode==CANCEL_BATCH)assert(batch_blocked);if(mode==PUBLISH_AGAIN)assert(delayed_publications==2&&writes==1);if(mode==CONTROLS||mode==CONTROLS_CLOSE_AGAIN)assert(dispatches==1&&opens==2);if(mode==CONTROLS_CLOSE_AGAIN)assert(closes==4&&!writes);if(mode==SAVE_ERROR_SUSPEND)assert(writes==1&&!stored&&model.dirty&&model.uncertain);if(mode==SAVE_ACTIVITY_LATCH)assert(writes==1&&activity_snapshots==1&&activity_polls==1);if(mode==HOST_EXIT)assert(dispatches==1&&opens==1);if(mode>=ROOT_RELOAD&&mode<=RELOAD_FAIL)assert(reload_calls==1&&!writes);if(mode==REENTRY_CLEAN){stage=0;app_main();assert(clean&&releases==2&&backend_closes==2);}}
 printf("Settings scene app scenario %u passed\n",mode);return 0;}
