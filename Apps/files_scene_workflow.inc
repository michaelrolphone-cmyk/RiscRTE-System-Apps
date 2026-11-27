/* Copied workflow payloads and shared-editor focus custody. No raster adapter. */
static bool files_text_suspended,files_message_pending,files_message_closing,files_launch_pending;
static char files_workflow_message[33],files_workflow_launch[RISC_APP_CATALOG_ROUTE_MAX];
static uint32_t files_message_revision;
static risc_components_document_v1 files_message_document;
bool files_scene_background_workflow_ready(void){
 return !files_message_pending&&!files_launch_pending&&!files_text_queue.pending&&!files_handler_pending&&!files_settings.phase;
}
bool files_scene_background_message(const char text[33]){
 if(!files_scene_alive()||!text||!memchr(text,0,33)||!files_scene_background_workflow_ready())return false;
 memcpy(files_workflow_message,text,33);files_message_pending=true;files_message_revision=0;files_message_closing=false;return true;
}
/* Caller is inside paused Context progression; catalog never owns storage or
 * physical focus. Every external callback is fenced before another call. */
bool files_scene_background_launch(const char name[33]){
 if(!files_scene_alive()||!name||!memchr(name,0,33)||!files_scene_background_workflow_ready())return false;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 bool acquired=files_runtime->acquire(RISC_APP_CATALOG_CAPABILITY,1,0,&grant);
 if(!files_scene_alive())return false;
 if(grant.struct_size!=sizeof(grant)||(acquired?(!grant.api||!grant.slot||!grant.generation):(grant.api||grant.slot||grant.generation))){files_retain(NULL);return false;}
 if(!acquired)return false;
 const risc_app_catalog_v1*api=grant.api;
 if(api->api_version!=1||api->struct_size<sizeof(*api)||!api->snapshot||!api->get){files_retain(NULL);return false;}
 risc_app_catalog_snapshot_v1 snapshot={.struct_size=sizeof(snapshot)},after={.struct_size=sizeof(after)};
 int rc=api->snapshot(api->context,&snapshot);if(!files_scene_alive())return false;
 bool found=false;
 if(rc==RISC_APP_CATALOG_OK){
  if(snapshot.struct_size!=sizeof(snapshot)||!snapshot.generation||snapshot.count>RISC_APP_CATALOG_MAX){files_retain(NULL);return false;}
  for(unsigned i=0;i<snapshot.count;i++){
   risc_app_catalog_entry_v1 e={.struct_size=sizeof(e)};rc=api->get(api->context,snapshot.generation,i,&e);
   if(!files_scene_alive())return false;
   if(rc)break;
   if(e.struct_size!=sizeof(e)||(e.flags&~RISC_APP_CATALOG_AVAILABLE)||!memchr(e.file_name,0,sizeof(e.file_name))){files_retain(NULL);return false;}
   if((e.flags&RISC_APP_CATALOG_AVAILABLE)&&!strcmp(name,e.file_name)){memcpy(files_workflow_launch,e.file_name,sizeof(e.file_name));found=true;break;}
  }
  if(!rc){rc=api->snapshot(api->context,&after);if(!files_scene_alive())return false;
   if(!rc&&(after.struct_size!=sizeof(after)||after.generation!=snapshot.generation||after.count!=snapshot.count))found=false;}
 }
 if(rc<RISC_APP_CATALOG_STALE||rc>RISC_APP_CATALOG_NOT_FOUND){files_retain(NULL);return false;}
 risc_runtime_capability_v1 released=grant;bool ok=files_runtime->release(&released);
 if(!files_scene_alive())return false;
 if(!ok||released.struct_size!=sizeof(released)||released.api||released.slot||released.generation){files_retain(NULL);return false;}
 files_launch_pending=!rc&&found;return files_launch_pending;
}
/* 0 pending, 1 finished, -1 retained. This document never mutates a Files
 * selection/draft and cannot dispatch a domain action from dismissal input. */
static int files_message_step(void){
 if(!files_message_pending)return 1;
 if(!files_scene_alive())return -1;
 if(!files_scene_background_pause())return files_scene_alive()?0:-1;
 if(!files_message_revision){
  if(!files_ui_close(&files_ui))return files_scene_alive()?0:-1;
  if(files_revision==UINT32_MAX){files_retain(NULL);return -1;}
  files_message_revision=++files_revision;
  files_message_document=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(files_message_document),.revision=files_message_revision,.root=1,.route_count=1,.node_count=2,.screen_key=UINT32_C(0x464d0001)};
  files_message_document.routes[0]=(risc_scene_route_v1){.id=1,.back_action=1};strcpy(files_message_document.routes[0].title,"CONTEXT MESSAGE");
  files_message_document.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE};memcpy(files_message_document.nodes[0].text,files_workflow_message,33);
  files_message_document.nodes[1]=(risc_scene_node_v1){.id=2,.route=1,.kind=RISC_SCENE_ACTION,.action=1};strcpy(files_message_document.nodes[1].label,"Dismiss");
 }
 if(!files_message_closing){
  if(!files_ui.token){int rc=files_ui_publish(&files_ui,&files_message_document,files_host.enabled);if(rc!=1)return rc;}
  /* next advances the provider's presentation; snapshot alone never pumps it. */
  risc_scene_event_v1 event={0};int rc=files_ui_next(&files_ui,&event);if(rc<0)return -1;
  if(!files_ui_snapshot(&files_ui))return -1;
  /* A delivered current-revision action is already accepted by the provider.
   * Do not discard it because the physical key is still held; checked close
   * owns the remaining input/presentation drain. */
  if(!rc)return 0;
  if(event.document_revision!=files_message_revision)return 0;
  if(event.kind==RISC_SCENE_CONTROLS_EVENT){(void)files_resident_manual(&files_host,RISC_RESIDENT_CHECKPOINT_CONTROLS);files_message_closing=true;}
  else if(event.kind==RISC_SCENE_SUSPEND_EVENT){files_scene_exit_requested=true;files_message_closing=true;}
  else if(event.action==1&&(event.node==0||event.node==2))files_message_closing=true;
  if(!files_message_closing)return 0;
 }
 if(!files_ui_close(&files_ui))return files_scene_alive()?0:-1;
 files_message_pending=false;files_message_revision=0;fb_dirty=true;return 1;
}
void portable_text_adapter_retain(void){files_retain(NULL);}
bool portable_text_adapter_retained(void){return !files_scene_alive();}
int portable_text_adapter_suspend(void){
 if(!files_scene_alive())return RISC_TEXT_ENTRY_RETAINED;
 if(files_text_suspended||files_ui.token){files_retain(NULL);return RISC_TEXT_ENTRY_RETAINED;}
 if(!files_scene_background_pause())return RISC_TEXT_ENTRY_AGAIN;
 files_text_suspended=true;return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_resume(void){
 if(!files_scene_alive())return RISC_TEXT_ENTRY_RETAINED;
 if(!files_text_suspended||files_ui.token){files_retain(NULL);return RISC_TEXT_ENTRY_RETAINED;}
 files_text_suspended=false;fb_dirty=true;return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_exit_pending(void){
 if(!files_scene_alive())return -1;
 return files_scene_exit_requested||files_launch_pending||!!(files_host.pending_attention&RISC_RESIDENT_ATTENTION_EXIT);
}
int portable_text_adapter_attention(void){
 if(!files_scene_alive()||!files_text_suspended)return -1;
 if(!files_resident_capture(&files_host,files_scene_millis()))return -1;
 return portable_text_adapter_exit_pending()||!!(files_host.pending_attention&(RISC_RESIDENT_ATTENTION_ALARM|RISC_RESIDENT_ATTENTION_CONTROLS));
}
int portable_text_adapter_policy_capture(void){
 return files_scene_alive()&&files_text_suspended&&files_scene_background_capture()&&files_resident_capture(&files_host,files_scene_millis())?0:-1;
}
int portable_text_adapter_policy_prepare(uint32_t flags,bool*continuity){
 if(!files_scene_alive()||!files_text_suspended||!continuity)return -1;
 *continuity=false;
 if(flags&(RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY))files_idle_at=files_scene_millis();
 if(flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)return 0;
 /* Service progression precedes held-input deferral, as in the predecessor. */
 if(!files_scene_background_step(true))return -1;
 if(flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)return 0;
 bool due=files_idle_ms&&!files_scene_background_capture_active()&&(uint32_t)(files_scene_millis()-files_idle_at)>=files_idle_ms;
 return files_message_pending||files_launch_pending||files_scene_background_effects()||files_host.phase||files_host.pending_attention||due;
}
int portable_text_adapter_policy_apply(bool continuity,uint32_t*resume_flags){
 if(!files_scene_alive()||!files_text_suspended||!fb_text_checkpoint.paused||!resume_flags||continuity)return -1;
 *resume_flags=RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED;
 if(files_launch_pending||files_scene_exit_requested)return 1;
 int modal=files_message_step();if(modal<0)return -1;if(!modal)return 2;
 if(!files_settings_step(true))return -1;
 if(files_settings.phase)return 2;
 if(!files_host_step(true))return -1;
 if(files_host.phase)return 2;
 return files_scene_exit_requested?1:0;
}
