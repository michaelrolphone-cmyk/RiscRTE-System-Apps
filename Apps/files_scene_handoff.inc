/* Copied local handler intent survives checked semantic scene close. The
 * source-transfer state machine already owns its copied handoff arguments. */
static bool files_handler_pending;
static unsigned files_handler_index;
static files_text_identity files_handler_identity;
static t5_file_handler_t files_handler_value;
static char files_handler_path[FB_PATH_CAP];
static bool files_scene_handoff_ready(void){
 if(!files_scene_alive()||!files_scene_background_pause()||!files_scene_alive())return false;
 return files_ui_close(&files_ui)&&files_scene_alive();
}
static bool files_scene_handler_ready(unsigned index){
 if(!files_scene_alive())return false;
 if(!files_ui.token)return files_scene_handoff_ready();
 if(files_handler_pending)return false;
 if(index>=FB_MAX_PAGE_ROWS||!files_text_snapshot(NULL,&files_handler_identity))return false;
 files_handler_index=index;files_handler_value=fb_handlers[index];
 memcpy(files_handler_path,fb_handler_path,sizeof(files_handler_path));
 files_handler_pending=true;return false;
}
static void files_handler_step(void){
 if(!files_handler_pending)return;
 if(!files_scene_handoff_ready())return;
 files_text_identity now={0};
 bool current=files_text_snapshot(NULL,&now)&&
  files_text_identity_equal(&files_handler_identity,&now)&&
  !memcmp(&files_handler_value,&fb_handlers[files_handler_index],sizeof(files_handler_value))&&
  !memcmp(files_handler_path,fb_handler_path,sizeof(files_handler_path));
 files_handler_pending=false;
 if(current)fbx_dispatch(files_handler_index);
 else fb_message("File selection changed. Choose again.");
}
