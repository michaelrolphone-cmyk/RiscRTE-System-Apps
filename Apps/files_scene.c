/* Files semantic root. No device, pixel, backend or fixed-resolution adapter.
 * Background integration remains an explicit link dependency; see its header. */
#include "files_scene_ui.h"
#include "files_scene_text_pending.h"
#include "files_scene_background.h"
#include "files_scene_resident.h"
#include "files_scene_effects.h"
#include "PortablePeerFiles.h"
#include "RiscResidentShellV1.h"
#include "RiscAppCatalogV1.h"
static const risc_runtime_api_v1 *files_runtime;
static bool files_retained,files_invoked,files_clean,files_background_started,files_scene_exit_requested;
static files_scene_ui files_ui;
static files_text_pending files_text_queue;
static portable_peer_row files_scene_peer_rows[32];
static unsigned files_scene_peer_count;
static uint32_t files_revision,files_settled_at;
static bool files_cosmetic_pending,files_settled_seen;
static uint64_t files_mount_epoch;
static files_resident files_host;
static files_effects files_settings;
static uint32_t files_idle_at,files_idle_ms;
void files_scene_background_idle_update(uint32_t ms){files_idle_ms=ms;}
static risc_components_document_v1 files_candidate;
static bool files_scene_alive(void){
 if(files_retained)return false;
 if(risc_runtime_get_api(1)!=files_runtime){files_retained=true;return false;}
 return true;
}
static bool files_alive(void *p){(void)p;return files_scene_alive();}
static void files_retain(void *p){
 (void)p;if(files_retained)return;
 files_retained=true;
 if(risc_runtime_get_api(1)==files_runtime&&files_runtime&&files_runtime->retain_invocation)
  (void)files_runtime->retain_invocation();
}
bool portable_adapter_retained(void){return !files_scene_alive();}
void portable_adapter_retain_silent(void){files_retain(NULL);}
void portable_adapter_retain(void){files_retain(NULL);}
static uint32_t files_scene_millis(void){
 uint32_t now=0;if(!files_scene_alive())return 0;
 bool ok=files_runtime->monotonic_ms(&now);
 if(!files_scene_alive())return 0;
 if(!ok)files_retain(NULL);
 return now;
}
static void files_scene_mount_changed(void);
static bool fb_draw(void);
static bool files_scene_handoff_ready(void),files_scene_handler_ready(unsigned);
#include "files_scene_domain.h"
#include "files_scene_presenter.inc"
#include "files_scene_text_bridge.inc"
#include "files_scene_handoff.inc"
#include "files_scene_reset.inc"
static bool files_copy_peers(void){
 files_scene_peer_count=0;
 if(!fpeer||fpeer_status.state!=PEER_DISCOVERING)return true;
 if(fpeer_status.count>32){files_retain(NULL);return false;}
 for(unsigned i=0;i<fpeer_status.count;i++){
  bool ok=portable_peer_row_get(fpeer,i,&files_scene_peer_rows[i]);
  if(!files_scene_alive())return false;
  if(!ok){fb_dirty=true;return false;}
 }
 files_scene_peer_count=fpeer_status.count;return true;
}
static bool files_publish(void){
 if(!files_scene_alive()||fb_text_active||files_text_queue.pending||files_handler_pending)return false;
 if(!fb_dirty&&files_ui.token)return true;
 if(files_revision==UINT32_MAX){files_retain(NULL);return false;}
 if(!files_copy_peers())return false;
 if(!files_presenter_build(&files_candidate,files_revision+1)){files_retain(NULL);return false;}
 int rc=files_ui_publish(&files_ui,&files_candidate,files_host.enabled);
 if(rc!=1)return false;
 if(!files_presenter_commit(&files_candidate)){files_retain(NULL);return false;}
 files_revision=files_candidate.revision;fb_dirty=false;files_cosmetic_pending=files_settled_seen=false;return true;
}
/* Synchronous progress publication cannot be skipped before a mutation.
 * Pending display/input is drained by the provider; no intent is dispatched
 * here. Every return to the mutation caller is followed by the liveness fence. */
static bool fb_draw(void){
 while(files_scene_alive()){
  if(files_publish())return files_scene_alive();
  if(!files_scene_alive())return false;
  if(fb_text_active||files_text_queue.pending){files_retain(NULL);return false;}
  /* Progress input while waiting for presentation, but never recursively
   * dispatch an intent from inside a storage transaction. Any new intent
   * cancels this not-yet-started mutation; the user may retry explicitly. */
  risc_scene_event_v1 event={0};int rc=files_ui_next(&files_ui,&event);
  if(rc<0)return false;
  if(rc==1&&event.document_revision==files_ui.committed.revision){
   fb_message("Operation not started. Review and retry.");return false;
  }
  if(!files_scene_background_capture()){files_retain(NULL);return false;}
  files_runtime->yield_ms(1);
 }
 return false;
}
bool portable_file_browser_setup_service_supported(void){return files_ui.components!=NULL;}
bool portable_file_browser_setup_frame_ready(void){
 if(!files_ui.token||!files_ui_snapshot(&files_ui))return false;
 if(files_ui.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING))return false;
 return files_presenter_frame_ready(files_ui.committed.revision);
}
static bool files_events(void){
 for(unsigned i=0;i<32&&files_scene_alive();i++){
  risc_scene_event_v1 event={0};int rc=files_ui_next(&files_ui,&event);
  if(rc<0)return false;
  if(!rc)return true;
  if(event.document_revision!=files_ui.committed.revision)continue;
  files_idle_at=files_scene_millis();
  if(event.kind==RISC_SCENE_CONTROLS_EVENT&&!event.node&&!event.action&&!event.value){(void)files_resident_manual(&files_host,RISC_RESIDENT_CHECKPOINT_CONTROLS);return true;}
  if(event.kind==RISC_SCENE_SUSPEND_EVENT&&!event.node&&!event.action&&!event.value){files_scene_exit_requested=true;return true;}
  (void)files_presenter_action(&event);
  if(files_text_queue.pending||files_handler_pending||files_scene_exit_requested||fb_terminal)return true;
 }
 return false;
}
/* Select authority, not media readiness. An admitted empty/unready volume
 * stays selected so its original status/retry behavior remains visible. */
static bool files_select_root(void){
 if(!files_scene_background_pause()||!files_scene_alive())return false;
 for(unsigned index=0;index<FB_LOCAL_ROOT_COUNT;index++){
  int rc=fb_acquire_root(index,&fb_grant);
  if(rc<0)return false;
  if(rc==1){fb_storage_index=index;fb_volume=fb_grant.api;return true;}
 }
 fb_message("No storage root is available or granted");return false;
}
static int files_resident_settle(void *owner){
 (void)owner;
 if(!files_scene_alive())return -1;
 if(portable_file_browser_sharing_active()||fsource_busy()||fbm_busy()||(fb_text_active&&!fb_text_policy_boundary)||files_text_queue.pending||files_handler_pending)return 0;
 if(!files_scene_background_pause())return files_scene_alive()?0:-1;
 if(!portable_file_browser_safe())return files_scene_alive()?0:-1;
 return files_ui_close(&files_ui)?1:files_scene_alive()?0:-1;
}
static int files_settings_call(void*owner,uint32_t reason){
 (void)owner;
 if(!files_host.enabled)return RISC_RESIDENT_BUSY;
 uint32_t flags=0;int rc=files_resident_call(&files_host,reason,&flags);
 if(flags){files_retain(NULL);return RISC_RESIDENT_RETAINED;}
 return rc;
}
static bool files_settings_done(void*owner,unsigned effect,unsigned outcome){
 (void)owner;return files_scene_background_effect_done(effect,outcome);
}
static bool files_settings_step(bool settled){
 uint32_t pending=files_scene_background_effects();
 int rc=files_effects_step(&files_settings,pending,settled&&!portable_file_browser_sharing_active());
 if(rc<0)return false;
 if(rc==2)fb_message("Host settings effects unavailable");
 if(rc==1)fb_dirty=true;
 return files_scene_alive();
}
static bool files_host_step(bool settled){
 uint32_t now=files_scene_millis();if(!files_scene_alive())return false;
 if(files_ui.flags&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))files_idle_at=now;
 bool due=!files_scene_background_capture_active()&&!files_scene_background_effects()&&files_idle_ms&&(uint32_t)(now-files_idle_at)>=files_idle_ms;
 int rc=files_resident_step(&files_host,settled,portable_file_browser_sharing_active(),due);
 if(rc<0)return false;
 if(rc==1){
  files_idle_at=now;fb_dirty=true;
  if(files_host.exit_requested)files_scene_exit_requested=true;
  if(files_host.reply_flags&RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED){
   if(!files_scene_background_idle_timeout(&files_idle_ms))files_idle_ms=0;
  }
  if(files_host.unsupported)fb_message("Host lifecycle service unavailable");
 }
 return files_scene_alive();
}
/* Reentry may reset only after the previous call proved full cleanup. */
static bool files_open(void){
 if(files_invoked&&!files_clean){files_retain(NULL);return false;}
 files_invoked=true;files_clean=false;fb_runtime=files_runtime;
 files_reset_domain();
 files_ui_init(&files_ui,files_runtime,files_alive,files_retain,NULL);
 files_text_pending_init(&files_text_queue,NULL,files_alive,files_retain,files_text_snapshot,files_text_drain,files_text_open);
 int available=files_ui_bind(&files_ui);
 if(available!=1){if(available==0&&files_ui_release(&files_ui))files_clean=true;return false;}
 if(!files_scene_background_init(files_runtime,files_alive,files_retain,NULL)){files_retain(NULL);return false;}
 files_background_started=true;
 if(!files_resident_init(&files_host,files_runtime,files_alive,files_retain,files_resident_settle,NULL))return false;
 files_effects_init(&files_settings,files_alive,files_retain,files_resident_settle,files_settings_call,files_settings_done,NULL);
 files_idle_at=files_scene_millis();files_idle_ms=0;
 (void)files_scene_background_idle_timeout(&files_idle_ms);
 if(files_select_root())(void)fb_load(0);
 if(!files_scene_alive())return false;
 fbx_resume_result();return files_scene_alive();
}
static bool files_close(void){
 files_text_pending_cancel(&files_text_queue);
 if(!files_scene_background_pause())return false;
 if(!files_ui_close(&files_ui))return false;
 if(!portable_file_browser_close())return false;
 if(files_background_started&&!files_scene_background_close())return false;
 files_background_started=false;
 if(!files_ui_release(&files_ui))return false;
 files_clean=true;return true;
}
#include "files_scene_workflow.inc"
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!rt->acquire||!rt->release||!rt->yield_ms||!rt->monotonic_ms||!rt->retain_invocation)return;
 if(files_retained||(files_invoked&&!files_clean)){files_retain(NULL);return;}
 files_runtime=rt;
 files_text_suspended=files_message_pending=files_message_closing=files_launch_pending=false;
 files_message_revision=0;
 if(!files_open())return;
 while(files_scene_alive()&&!files_scene_exit_requested&&!fb_terminal){
  if(!files_scene_background_capture())break;
  if(!files_resident_capture(&files_host,files_scene_millis()))break;
  fsh_tick();if(!files_scene_alive())break;
  fsource_tick();if(!files_scene_alive())break;
  if(portable_file_browser_cleanup_only()){rt->yield_ms(1);continue;}
  if(files_handler_pending){files_handler_step();if(!files_scene_alive())break;rt->yield_ms(1);continue;}
  if(files_text_queue.pending){
   files_text_intent completed=files_text_queue.intent;
   int rc=files_text_pending_step(&files_text_queue);
   if(rc==FILES_TEXT_RETAINED)break;
   if(rc==FILES_TEXT_CANCELLED||rc==FILES_TEXT_UNAVAILABLE){
    files_text_retire(&completed);
    fb_dirty=true;
   }
   rt->yield_ms(1);continue;
  }
  if(fb_text_active){fb_text_step();if(!files_scene_alive())break;rt->yield_ms(1);continue;}
  if(fb_text_home){files_scene_exit_requested=true;break;}
  if(files_launch_pending){files_scene_exit_requested=true;break;}
  if(files_message_pending){if(files_message_step()<0)break;rt->yield_ms(1);continue;}
  if(!files_events()){if(!files_scene_alive())break;rt->yield_ms(1);continue;}
  if(files_scene_exit_requested||fb_terminal||files_text_queue.pending||files_handler_pending)continue;
  bool settled=true;
  if(files_ui.token){
   if(!files_ui_snapshot(&files_ui))break;
   settled=!(files_ui.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING));
   if(settled)(void)files_presenter_frame_ready(files_ui.committed.revision);
  }
  if(files_host.phase){
   if(!files_host_step(settled&&!(files_ui.flags&RISC_SCENE_INPUT_BUSY)))break;
   if(files_scene_alive())rt->yield_ms(1);
   continue;
  }
  if(!portable_file_browser_sharing_active()&&!files_scene_background_step(settled))break;
  if(!files_settings_step(settled&&!(files_ui.flags&RISC_SCENE_INPUT_BUSY)))break;
  if(files_settings.phase){rt->yield_ms(1);continue;}
  if(!files_host_step(settled&&!(files_ui.flags&RISC_SCENE_INPUT_BUSY)))break;
  if(files_host.phase||files_scene_exit_requested){rt->yield_ms(1);continue;}
  /* A countdown must not continuously replace a slow frame. Wait for one
   * second of settled, neutral presentation; substantive changes bypass this. */
  uint32_t now=files_scene_millis();if(!files_scene_alive())break;
  if(!settled||(files_ui.flags&(RISC_SCENE_INPUT_BUSY|RISC_SCENE_ACTIVITY)))files_settled_seen=false;
  else if(!files_settled_seen){files_settled_seen=true;files_settled_at=now;}
  else if(files_cosmetic_pending&&(uint32_t)(now-files_settled_at)>=1000u)fb_dirty=true;
  (void)files_publish();
  if(files_scene_alive())rt->yield_ms(1);
 }
 if(!files_scene_alive())return;
 while(files_scene_alive()&&!files_close())rt->yield_ms(1);
 if(files_scene_alive()&&files_clean&&files_launch_pending){
  files_launch_pending=false;
  bool launched=rt->request_launch&&rt->request_launch(files_workflow_launch);
  if(!files_scene_alive())return;
  if(!launched&&rt->diagnostic)rt->diagnostic("Files workflow launch refused after cleanup");
 }
}
