/* Extracted domain from file_browser_peer.inc; semantic UI/lifecycle supplied by files_scene.c. */
/* Shared Nova/paper peer UI. Commands enqueue work; the ordinary foreground
 * tick owns all BLE, TCP and bounded storage operations. */
static portable_peer_status fpeer_status;
static unsigned fpeer_choice;
static bool fpeer_launch;
static uint32_t fpeer_pair_generation,fpeer_pair_number;
static char fpeer_local_folder[FB_PATH_CAP],fpeer_upload_path[FB_PATH_CAP],fpeer_upload_name[128],fpeer_launch_path[FB_PATH_CAP];
#include "files_scene_mount.inc"
static bool fpeer_close(void){
 if(!fpeer)return true;
 fbm_unmount();
 /* The peer owns its internal leases; cancellation must run outside them. */
 portable_peer_stop(fpeer);
portable_peer_tick(fpeer,fsh_clock());
portable_peer_status_get(fpeer,&fpeer_status);
 if(fpeer_status.state==PEER_RETAINED){fsh_retain();
return false;
}
 if(fpeer_status.state!=PEER_OFF)return false;
 if(!portable_peer_destroy(fpeer)){fsh_retain();
return false;
}fpeer=NULL;
return true;
}
static void fpeer_open(void){
 if(fsh_phase!=FSH_OFF){fbx_notice("Stop the current sharing session before connecting");return;}
 if(!portable_file_browser_safe()||!fb_ready())return;
 fpeer_mode=true;
fpeer_choice=0;
fpeer_launch=false;
 fpeer_pair_generation=0;
memset(&fpeer_status,0,sizeof(fpeer_status));
 strcpy(fpeer_local_folder,fb_path);
fpeer_upload_path[0]=fpeer_upload_name[0]=0;
 if(fb_list_choice<fb_count&&!fb_rows[fb_list_choice].is_directory){
  if(fb_join(fb_path,fb_rows[fb_list_choice].name,fpeer_upload_path,sizeof(fpeer_upload_path)))snprintf(fpeer_upload_name,sizeof(fpeer_upload_name),"%s",fb_rows[fb_list_choice].name);
 }
 fb_mode=FB_SHARING;
fsh_back_pending=false;
fsh_message[0]=0;
fb_dirty=true;
fsh_start();
}
static void fpeer_tick(uint64_t now){
 if(fsh_phase==FSH_CONNECT){
 if(!(fsh_radio_flags&PORTABLE_RADIO_BLUETOOTH)){fsh_error("Enable Bluetooth in Quick Actions first");
fsh_phase=FSH_STOPPING;
return;
}
  const portable_peer_hooks hooks={NULL,fsh_owner,malloc,free};
fpeer=portable_peer_create(fb_runtime,&hooks);
  if(!fpeer||!portable_peer_start(fpeer,now)){fsh_error("Cannot start nearby Files");
fsh_phase=FSH_STOPPING;
return;
}
  fsh_phase=FSH_ACTIVE;
 }
 /* Do not lease over JOIN/poll/cancel. The peer scopes its own I/O. */
 portable_peer_tick(fpeer,now);
portable_peer_status next;
portable_peer_status_get(fpeer,&next);
 if(next.state==PEER_RETAINED){fsh_retain();
return;
}
 if(memcmp(&next,&fpeer_status,sizeof(next))){
  if(next.state!=fpeer_status.state)fpeer_choice=0;
  fpeer_status=next;
fb_dirty=true;
 }
 fbm_tick();
 if(next.state==PEER_OFF){fsh_error(next.message[0]?next.message:"Peer session ended");
fsh_phase=FSH_STOPPING;
}
}
static void fpeer_after_close(void){
 if((!fpeer_launch&&!fbm_dispatch_pending)||fsh_phase!=FSH_OFF)return;
#ifndef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
 if(fbm_dispatch_pending){fbm_dispatch_pending=false;fpeer_mode=false;fbx_dispatch(0);return;}
#endif
fpeer_launch=false;
fpeer_mode=false;
 if(fb_storage_index&&!fbx_swap_local()){fb_message("Primary storage unavailable after download");return;}
 strcpy(fb_file_path,fpeer_launch_path);
memset(&fb_selected,0,sizeof(fb_selected));
 const char*name=strrchr(fb_file_path,'/');
snprintf(fb_selected.name,sizeof(fb_selected.name),"%.*s",(int)sizeof(fb_selected.name)-1,name?name+1:fb_file_path);
 fb_mode=FB_ACTIONS;
fb_choice=0;
fb_dirty=true;
#ifndef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
 fbx_open(fbm_choose_handler);
 /* Existing declared file.open broker, including ELF rejection. */
#endif
}
/* Connection setup ends at mount. All file rows and operations use the normal
 * browser controller and its existing RGB/paper presentation. */

static void fpeer_action(void){
 if(fsh_phase==FSH_OFF){fpeer_open();return;}
 if(fpeer_status.state==PEER_PAIRING){
  if(fpeer_choice<2){if(!portable_file_browser_setup_frame_ready()||fpeer_pair_generation!=fpeer_status.pairing_generation||fpeer_pair_number!=fpeer_status.pairing_number)return;
   (void)portable_peer_confirm(fpeer,fpeer_pair_generation,fpeer_pair_number,fpeer_choice==1);fpeer_pair_generation=0;
  }else fsh_phase=FSH_STOPPING;
 }else if(fpeer_status.state==PEER_DISCOVERING){portable_peer_row row;
  if(fpeer_choice<fpeer_status.count&&portable_peer_row_get(fpeer,fpeer_choice,&row))(void)portable_peer_select(fpeer,row.peer_id);else fsh_phase=FSH_STOPPING;
 }else fsh_phase=FSH_STOPPING;
 fb_dirty=true;
}


