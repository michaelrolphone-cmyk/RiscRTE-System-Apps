static unsigned fbm_operation;
static uint32_t fbm_wait_generation;
static char fbm_requested_path[FB_PATH_CAP],fbm_replace_path[FB_PATH_CAP];
static uint64_t fbm_preview_offset;
static bool fbm_preview_hex;
static void fbm_queued(void){portable_peer_status status;portable_peer_status_get(fpeer,&status);fbm_wait_generation=status.operation_generation;}
enum { FBM_IDLE,FBM_PREVIEW,FBM_COPY,FBM_MUTATE,FBM_OPEN,FBM_CANCEL,FBM_REFRESH,FBM_RETURN,FBM_SOURCE };
static bool fbm_choose_handler,fbm_dispatch_pending;
/* An authenticated peer is a browser mount. Directory callbacks inspect the
 * completed copied listing only; requests run on later foreground ticks. They
 * never turn the synchronous volume ABI into a blocking network operation. */
static const risc_storage_volume_api_v1 *fbm_local_volume;
static risc_runtime_capability_v1 fbm_local_grant;
static unsigned fbm_local_index,fbm_directory_cursor;
static bool fbm_cached(void*context){(void)context;return fpeer&&fpeer_status.state==PEER_READY;}
static bool fbm_label(void*context,char*out,size_t n){(void)context;return snprintf(out,n,"Nearby files")>0;}
static bool fbm_stat(void*context,const char*path,uint64_t*size,bool*directory){
 (void)context;if(!fbm_cached(NULL))return false;
 if(!strcmp(path,fpeer_status.path)){*size=0;*directory=true;return true;}
 for(unsigned i=0;i<fpeer_status.count;i++){portable_peer_row row;if(portable_peer_row_get(fpeer,i,&row)&&!strcmp(path,row.path)){*size=row.size;*directory=row.directory;return true;}}
 return false;
}
static risc_storage_dir_t fbm_dir_open(void*context,const char*path){(void)context;if(!fbm_cached(NULL)||strcmp(path,fpeer_status.path))return 0;fbm_directory_cursor=0;return 1;}
static bool fbm_dir_next(void*context,risc_storage_dir_t handle,risc_storage_dirent_v1*out){
 (void)context;if(handle!=1||!fbm_cached(NULL)||fbm_directory_cursor>=fpeer_status.count)return false;
 portable_peer_row row;if(!portable_peer_row_get(fpeer,fbm_directory_cursor++,&row))return false;
 memset(out,0,sizeof(*out));snprintf(out->name,sizeof(out->name),"%s",row.name);out->size=row.size;out->is_directory=row.directory;return true;
}
static void fbm_dir_close(void*context,risc_storage_dir_t handle){(void)context;(void)handle;}
static bool fbm_error(void*context,char*out,size_t n){(void)context;if(n)out[0]=0;return false;}
static risc_storage_volume_api_v1 fbm_volume={.api_version=1,.struct_size=sizeof(fbm_volume),.refresh=fbm_cached,.ready=fbm_cached,.label=fbm_label,.stat=fbm_stat,.dir_open=fbm_dir_open,.dir_next=fbm_dir_next,.dir_close=fbm_dir_close,.last_error=fbm_error};
static void fbm_enter(void){
 fbm_local_volume=fb_volume;fbm_local_grant=fb_grant;fbm_local_index=fb_storage_index;
 snprintf(fbm_local_path,sizeof(fbm_local_path),"%s",fb_path);
 fb_volume=&fbm_volume;memset(&fb_grant,0,sizeof(fb_grant));fb_storage_index=2;
}
static void fbm_leave(void){
 if(fb_storage_index!=2)return;
 fb_volume=fbm_local_volume;fb_grant=fbm_local_grant;fb_storage_index=fbm_local_index;
 fbm_local_volume=NULL;memset(&fbm_local_grant,0,sizeof(fbm_local_grant));
}
static void fbm_unmount(void){
 if(!fbm_mounted)return;
 bool was_remote=fb_storage_index==2;
 bool remote_destination=fb_mode==FB_DESTINATION&&fb_source_index==2;
 fbm_leave();fbm_mounted=fbm_loading=fbm_applying=false;fbm_operation=FBM_IDLE;
 if(was_remote)snprintf(fb_path,sizeof(fb_path),"%s",fbm_local_path);
 if(was_remote||remote_destination){fb_mode=FB_LIST;fb_count=0;fb_loaded=false;fsh_back_pending=true;}
 if(fb_source_index==2){fb_source_index=fb_storage_index;fb_source_path[0]=fb_source_base[0]=fb_source_query[0]=0;}
}
static bool fbm_swap(void){
 if(fbm_loading||fbm_operation){fb_message("Folder is loading; Back cancels");return false;}
 if(fb_storage_index==2){fbm_leave();
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
  if(fb_storage_index&&!fbx_swap_local())return false;
#endif
  return true;
 }
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
 if(!fb_storage_index)return fbx_swap_local();
#endif
 fbm_enter();return true;
}
static bool fbm_before_load(int direction){
 if(fbm_applying||(direction&&fb_loaded&&!strcmp(fb_path,fpeer_status.path)))return true;
 fb_count=0;fb_loaded=false;
 if(!fbm_loading){
  if(!portable_peer_list(fpeer,fb_path)){fb_message("Remote folder unavailable. Refresh retries.");return false;}
  fbm_queued();strcpy(fbm_requested_path,fb_path);fbm_loading=true;
 }
 fb_message("Loading folder; Back cancels");return false;
}
static void fbm_tick(void){
 if(fpeer_status.state==PEER_READY){
  if((fbm_operation||fbm_loading)&&fpeer_status.completed_generation!=fbm_wait_generation)return;
  if(!fbm_mounted){fbm_mounted=true;fbm_enter();strcpy(fb_path,fpeer_status.path);fb_query[0]=0;fb_mode=FB_LIST;strcpy(fbm_requested_path,fb_path);fbm_loading=true;}
  if(fbm_operation){
   unsigned completed=fbm_operation;fbm_operation=FBM_IDLE;
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
   if(completed==FBM_SOURCE){fsource_remote_done(fpeer_status.operation_ok);return;}
   if(completed==FBM_CANCEL&&fsource_remote&&fsource_state==FSOURCE_CANCEL){fsource_remote_done(false);return;}
#endif
   if(completed==FBM_CANCEL){fb_mode=FB_LIST;fb_loaded=false;fb_count=0;fb_message(fpeer_status.operation_ok?"Operation completed. Refresh folder to inspect.":fpeer_status.uncertain?"Publication unknown. Refresh folder to inspect.":"Request cancelled. Refresh folder to inspect.");return;}
   if(!fpeer_status.operation_ok){fb_message(fpeer_status.message[0]?fpeer_status.message:"Remote operation failed. Retry.");return;}
   if(completed==FBM_PREVIEW){fb_offset=fbm_preview_offset;fb_hex=fbm_preview_hex;size_t n=0;if(!portable_peer_preview_get(fpeer,fb_preview,sizeof(fb_preview),&n)){fb_message("Preview unavailable");return;}fb_preview_count=(unsigned)n;fb_size=fpeer_status.preview_total;fb_message(n?"Preview only / no execution":"Empty file");return;}
   if(completed==FBM_OPEN){if(!fpeer_status.published_path[0]||strlen(fpeer_status.published_path)>=sizeof(fpeer_launch_path)){fb_message("File was not published within the supported path bound");return;}strcpy(fpeer_launch_path,fpeer_status.published_path);fpeer_launch=true;fsh_phase=FSH_STOPPING;return;}
   if(completed==FBM_COPY||completed==FBM_RETURN){if(!fbm_select(fb_source_index)){if(fb_io_pending)fbm_operation=FBM_RETURN;return;}strcpy(fb_path,fb_source_base);strcpy(fb_query,fb_source_query);}
   fb_mode=FB_LIST;if(!fb_load(0)&&fb_io_pending&&fb_storage_index!=2)fbm_operation=FBM_REFRESH;return;
  }
  if(fbm_loading){fbm_loading=false;if(!fpeer_status.operation_ok){fb_message(fpeer_status.message[0]?fpeer_status.message:"Folder read failed. Refresh retries.");return;}if(fb_storage_index==2){strcpy(fb_path,fbm_requested_path);fbm_applying=true;(void)fb_load(0);fbm_applying=false;}}
 }else if(fbm_mounted&&fpeer_status.state==PEER_ERROR){fbm_loading=false;fb_loaded=false;fb_count=0;fb_message(fpeer_status.message[0]?fpeer_status.message:"Remote folder failed. Refresh retries.");}
}
static bool fbm_busy(void){if(fbm_loading||fbm_operation){fb_message("Operation in progress; Back cancels");return true;}return false;}
static bool fbm_cap(uint32_t cap,const char*message){if(!(fpeer_status.capabilities&cap)){fbx_notice(message);return false;}return true;}
static bool fbm_can_rename(void){return (fpeer_status.capabilities&PEER_CAP_MOVE)!=0;}
static bool fbm_can_delete(void){return (fpeer_status.capabilities&PEER_CAP_DELETE)!=0;}
static bool fbm_select(unsigned index){for(unsigned i=0;i<3&&fb_storage_index!=index;i++)if(!fbx_swap())return false;return fb_storage_index==index;}
static bool fbm_preview(void){
 if(fbm_busy()||!fbm_cap(PEER_CAP_READ,"Reading unavailable on this volume"))return false;
 fb_preview_count=0;
 if(!portable_peer_preview_range(fpeer,fb_file_path,fb_offset,fb_hex?64u:FB_PREVIEW_BYTES)){fb_message("Remote file unavailable. Retry.");return false;}
 fbm_queued();fbm_preview_offset=fb_offset;fbm_preview_hex=fb_hex;fbm_operation=FBM_PREVIEW;fb_message("Loading preview; Back cancels");return false;
}
static void fbm_rename(void){
 if(fbm_busy())return;
 if(!fb_name_valid(fb_editor,sizeof(fb_editor))){fb_message("Invalid name. No slash, backslash, . or ..");return;}
 char path[FB_PATH_CAP];if(!fb_join(fb_path,fb_editor,path,sizeof(path))){fb_message("Path too long");return;}
 if(!fbm_cap(fbm_new_directory?PEER_CAP_MKCOL:PEER_CAP_MOVE,fbm_new_directory?"New folder unavailable on this volume":"Rename unavailable on this volume"))return;
 bool queued=portable_peer_mutate(fpeer,fbm_new_directory?"MKCOL":"MOVE",fbm_new_directory?path:fb_file_path,fbm_new_directory?NULL:path);
 if(!queued){fb_message("Peer is busy or the session ended");return;}
 fbm_new_directory=false;fbm_queued();fbm_operation=FBM_MUTATE;fb_mode=FB_LIST;fb_message("Saving change; Back cancels");
}
static void fbm_new_folder(void){
 if(fbm_busy())return;
 if(fb_storage_index==2){if(!fbm_cap(PEER_CAP_MKCOL,"New folder unavailable on this volume"))return;}
 else {if(!fb_ready())return;const risc_storage_volume_api_v1_ext*ext=risc_storage_volume_extension(fb_volume);if(!ext||!ext->mkdir){fbx_notice("New folder unavailable on this volume");return;}}
 fbm_new_directory=true;fb_editor[0]=0;fb_key_page=PWK_INITIAL_PAGE;fbp_key_page=0;fb_choice=0;fb_mode=FB_RENAME;fb_message("Enter new folder name");
}
static void fbm_delete(void){
 if(fbm_busy()||!fbm_cap(PEER_CAP_DELETE,"Delete unavailable on this volume"))return;
 if(!portable_peer_mutate(fpeer,"DELETE",fb_file_path,NULL)){fb_message("Peer is busy or the session ended");return;}
 fbm_queued();fbm_operation=FBM_MUTATE;fb_mode=FB_LIST;fb_message("Deleting item; Back cancels");
}
static bool fbm_destination(bool move){
 if(fbm_busy())return false;
 if(fb_storage_index==2&&!fbm_cap(move?PEER_CAP_MOVE:PEER_CAP_READ,move?"Move unavailable on this volume":"Reading unavailable on this volume"))return false;
 return true;
}
static bool fbm_commit_destination(void){
 if(fbm_busy())return false;
 char destination[FB_PATH_CAP];if(!fb_join(fb_path,fb_selected.name,destination,sizeof(destination))){fb_message("Destination path too long");return false;}
 if(risc_protected_settings_path(fb_source_path)||risc_protected_settings_path(destination)){fb_message("Protected system settings");return false;}
 if(fb_storage_index==fb_source_index&&!strcmp(destination,fb_source_path)){fb_message("Already in this folder");return false;}
 if(fb_moving&&fb_storage_index!=fb_source_index){fb_message("Move requires the same volume. Use Copy instead.");return false;}
 bool queued=false;
 if(fb_source_index==2&&fb_storage_index==2){
  if(!fbm_cap(fb_moving?PEER_CAP_MOVE:PEER_CAP_COPY,fb_moving?"Move unavailable on this volume":"Copy within this volume is unavailable"))return false;
  queued=portable_peer_mutate(fpeer,fb_moving?"MOVE":"COPY",fb_source_path,destination);
 }else if(fb_source_index==2){
  if(!fb_service_begin())return false;
  bool ready=fb_ready()&&fbx_write_available(fb_volume);
  bool exists=ready&&fbx_exists(fb_volume,destination);
  if(!fb_service_end())return false;
  if(!ready){fb_message("Destination is read-only or unavailable");return false;}
  if(exists){fb_message("Destination already exists");return false;}
  queued=portable_peer_download(fpeer,fb_source_path,fb_volume,destination);
 }else{
  if(!fbm_cap(PEER_CAP_PUT,"Writing unavailable on this volume"))return false;
  uint64_t size=0;bool directory=false;
  if(fbm_stat(NULL,destination,&size,&directory)){
   if(directory){fb_message("Destination is an existing folder");return false;}
   if(!(fpeer_status.capabilities&PEER_CAP_HEAD)){fb_message("Existing file cannot be replaced safely on this volume");return false;}
   strcpy(fbm_replace_path,destination);fb_mode=FB_REPLACE;fb_choice=0;
   fb_message("Replace the existing file? Current contents will be overwritten.");return false;
  }
  const risc_storage_volume_api_v1*source=fbm_local_index==fb_source_index?fbm_local_volume:fb_other_volume;
  queued=portable_peer_upload(fpeer,source,fb_source_path,destination);
 }
 if(!queued){fb_message("Operation unavailable or peer busy");return false;}
 fbm_queued();fbm_operation=FBM_COPY;fb_message(fb_moving?"Moving item; Back cancels":"Copying file; Back cancels");return true;
}
static void fbm_open(bool choose);
static void fbm_open_scoped(bool choose){
#ifdef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
 (void)choose;fbx_notice("Opening unavailable in this profile");return;
#else
 if(fb_selected.is_directory){strcpy(fb_path,fb_file_path);fb_query[0]=0;fb_mode=FB_LIST;(void)fb_load(0);return;}
 if(fbm_busy()||!fbm_cap(PEER_CAP_READ,"Reading unavailable on this volume"))return;
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
 if(strcmp(PORTABLE_FILE_BROWSER_CAPABILITY,"storage.volume")){fsource_open(choose,true);return;}
#endif
 const risc_storage_volume_api_v1*local=fbm_local_index==0?fbm_local_volume:fb_other_volume;
 if(!fbx_write_available(local)){fbx_notice("Opening remote files needs a writable local handler destination");return;}
 if(!fbx_handlers()){fb_notice_return=FB_ACTIONS;fb_mode=FB_NOTICE;return;}
 char remote[FB_PATH_CAP],destination[FB_PATH_CAP];strcpy(remote,fb_file_path);
 const char*folder=fbm_local_index==0?fbm_local_path:"/";
 if(!fb_join(folder,fb_selected.name,destination,sizeof(destination))||strlen(destination)+3>=sizeof(fb_handler_path)){fbx_notice("File handler path is too long");return;}
 strcpy(fb_handler_path,"/sd");strcpy(fb_handler_path+3,destination);
 if(!fb_handler_api->handler_count(fb_handler_path)){fbx_notice("No declared application for this file type");return;}
 if(fbx_exists(local,destination)){
  const char*extension=strrchr(fb_selected.name,'.');if(!extension)extension="";
  char name[128];bool available=false;
  for(unsigned i=1;i<=32;i++){
   int n=snprintf(name,sizeof(name),"nearby-%u%s",i,extension);
   if(n<0||(size_t)n>=sizeof(name)||!fb_join(folder,name,destination,sizeof(destination)))break;
   if(!fbx_exists(local,destination)){available=true;break;}
  }
  if(!available){fbx_notice("No unused local Open destination; choose Copy or free a local file");return;}
 }
 if(!portable_peer_download(fpeer,remote,local,destination)){fbx_notice("Download unavailable for the local volume");return;}
 fbm_choose_handler=choose;fbm_queued();fbm_operation=FBM_OPEN;fb_message("Opening file; Back cancels");
#endif
}
static void fbm_open(bool choose) {
 if(!fb_service_begin())return;
 fbm_open_scoped(choose);
 (void)fb_service_end();
}
static bool fbm_back(void){
 fbm_new_directory=false;
 if(!fbm_loading&&!fbm_operation)return false;
 if(fbm_operation==FBM_RETURN||fbm_operation==FBM_REFRESH){fbm_operation=FBM_IDLE;fb_mode=FB_LIST;fb_loaded=false;fb_count=0;fb_message("Operation completed. Refresh the folder when ready.");return true;}
 portable_peer_cancel_operation(fpeer);fbm_loading=false;fbm_operation=FBM_CANCEL;fb_message("Cancelling operation");return true;
}

static void fbm_defer_dispatch(unsigned selected){
 /* file.open requires a quiescent native boundary. Preserve the exact chosen
  * handler, stop the mount cooperatively, then dispatch on a later tick. */
 fb_handlers[0]=fb_handlers[selected];fb_handler_count=1;fb_handler_offset=0;
 fbm_dispatch_pending=true;fb_mode=FB_NOTICE;fb_notice_return=FB_ACTIONS;
 fb_message("Closing connection before opening file");fsh_phase=FSH_STOPPING;
}

static void fbm_replace_confirm(void){
 if(fbm_busy()||!fbm_mounted||fb_storage_index!=2||fb_source_index==2)return;
 const risc_storage_volume_api_v1*source=fbm_local_index==fb_source_index?fbm_local_volume:fb_other_volume;
 if(!portable_peer_replace(fpeer,source,fb_source_path,fbm_replace_path)){fb_message("Replacement unavailable or peer busy");return;}
 fbm_queued();fbm_operation=FBM_COPY;fb_mode=FB_DESTINATION;fb_message("Replacing file; Back cancels");
}

static void fbm_local_mkdir(void){
 if(!fb_name_valid(fb_editor,sizeof(fb_editor))){fb_message("Invalid folder name");return;}
 char path[FB_PATH_CAP];if(!fb_join(fb_path,fb_editor,path,sizeof(path))||risc_protected_settings_path(path)){fb_message("Invalid or protected folder path");return;}
 if(!fb_ready())return;
 const risc_storage_volume_api_v1_ext*ext=risc_storage_volume_extension(fb_volume);
 if(!ext||!ext->mkdir||!ext->mkdir(fb_volume->context,path)){fb_error("Cannot create folder");return;}
 fbm_new_directory=false;fb_mode=FB_LIST;fb_query[0]=0;(void)fb_load(0);fb_message("Folder created");
}
