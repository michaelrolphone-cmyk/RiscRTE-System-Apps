/* Included after the original domain and before root orchestration. */
static bool files_text_snapshot(void *owner,files_text_identity *out){
 (void)owner;if(!files_scene_alive())return false;
 *out=(files_text_identity){.mode=fb_mode,.choice=fb_choice,.storage=fb_storage_index,
 .mount_epoch=files_mount_epoch,.volume=fb_volume,.grant_slot=fb_grant.slot,.grant_generation=fb_grant.generation,
 .selected=fb_selected,.mounted=fbm_mounted,.new_directory=fbm_new_directory,
 .peer=fpeer,.pairing_generation=fpeer_status.pairing_generation,
 .pairing_number=fpeer_status.pairing_number,
 .operation_generation=fpeer_status.operation_generation,.wait_generation=fbm_wait_generation};
 memcpy(out->path,fb_path,sizeof(out->path));
 memcpy(out->file_path,fb_file_path,sizeof(out->file_path));return true;
}
static void files_scene_mount_changed(void){
 if(files_mount_epoch==UINT64_MAX){files_retain(NULL);return;}
 ++files_mount_epoch;
 files_text_pending_cancel(&files_text_queue);
 if(fb_text_active)fb_text_invalidated=true;
}
static bool fb_text_begin(unsigned mode,const char *label){
 if(fb_text_active||!portable_file_browser_safe()||!files_scene_alive())return false;
 return files_text_pending_request(&files_text_queue,mode,label,fb_editor,
  mode==FB_FILTER?sizeof(fb_query):sizeof(fb_editor));
}
static int files_text_drain(void *owner){
 (void)owner;
 if(!files_scene_background_pause())return files_scene_alive()?0:-1;
 if(!files_ui_close(&files_ui))return files_scene_alive()?0:-1;
 return 1;
}
static int files_text_open(void *owner,const files_text_intent *intent){
 (void)owner;
 bool ok=fb_text_open_intent(intent);
 return files_scene_alive()?(ok?1:0):-1;
}

static void files_text_retire(const files_text_intent *completed){
 files_text_identity current={0};
 if(files_text_snapshot(NULL,&current)&&files_text_identity_equal(&completed->identity,&current))
  fbm_new_directory=false;
}
