/* Reader file-management workflows from T5S3-Reader 34d8e694d89a1e72d8854403d8592c289fae3ddc.
 * The presentation is local; authority stays in the admitted volume / file.open
 * capabilities. No firmware UI, arbitrary filesystem or new storage ABI. */
static const char *const fbx_labels[]={"Open","Open with","Preview","Rename","Move","Copy","Delete"};
static void fbx_notice(const char *message){fb_notice_return=FB_ACTIONS;fb_mode=FB_NOTICE;fb_message(message);}
static bool fbx_same_or_child(const char *candidate,const char *parent){size_t n=strlen(parent);return !strncmp(candidate,parent,n)&&(candidate[n]==0||candidate[n]=='/');}
static bool fbx_write_available(const risc_storage_volume_api_v1 *v){return v&&v->file_open_write&&v->file_write&&v->file_close;}
static bool fbx_rename_available(void){const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(fb_volume);return ext&&ext->rename;}
static bool fbx_exists(const risc_storage_volume_api_v1 *v,const char *path){uint64_t size;bool dir;return v->stat(v->context,path,&size,&dir);}
static void fbx_success(const char *message){fb_mode=FB_LIST;fb_query[0]=0;(void)fb_load(0);fb_message(message);}
static bool fbx_swap(void) {
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
 if(!portable_file_browser_safe()){fb_message("Close failed. Back retries safely.");return false;}
 if(!fb_other_volume){
  if(fb_other_grant.api){if(!fb_runtime->release(&fb_other_grant)){fb_message("Other volume release failed");return false;}memset(&fb_other_grant,0,sizeof(fb_other_grant));}
  fb_other_grant=(risc_runtime_capability_v1){.struct_size=sizeof(fb_other_grant)};
  uint64_t instance=fb_storage_index?PORTABLE_FILE_BROWSER_INSTANCE:PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE;
  const char *cap=fb_storage_index?PORTABLE_FILE_BROWSER_CAPABILITY:"storage.volume";
  if(!fb_runtime->acquire(cap,1,instance,&fb_other_grant)){fb_message("Other volume unavailable or not granted");return false;}
  if(!fb_api_valid(fb_other_grant.api)){PORTABLE_APP_RETAIN();if(PORTABLE_APP_RETAINED())return false;if(fb_runtime->release(&fb_other_grant))memset(&fb_other_grant,0,sizeof(fb_other_grant));fb_message("Other volume has an invalid interface");return false;}
  fb_other_volume=fb_other_grant.api;
 }
 const risc_storage_volume_api_v1 *v=fb_volume;fb_volume=fb_other_volume;fb_other_volume=v;
 risc_runtime_capability_v1 g=fb_grant;fb_grant=fb_other_grant;fb_other_grant=g;fb_storage_index^=1;
 return true;
#else
 fb_message("No secondary volume configured");return false;
#endif
}
static void fbx_switch(void){if(fbx_swap()){strcpy(fb_path,"/");fb_query[0]=0;fb_mode=FB_LIST;(void)fb_load(0);}else {fb_notice_return=FB_LIST;fb_mode=FB_NOTICE;fb_dirty=true;}}
static void fbx_actions(void){
 if(!fb_loaded||fb_choice>=fb_count){fb_mode=FB_LIST;fb_message("Select an item first");return;}
 fb_selected=fb_rows[fb_choice];
 if(!fb_join(fb_path,fb_selected.name,fb_file_path,sizeof(fb_file_path))){fb_message("Path too long");return;}
 fb_size=fb_selected.size;fb_choice=0;fb_action_page=0;fb_mode=FB_ACTIONS;fb_dirty=true;
}
static void fbx_rename(void){
 if(!fb_ready())return;
 if(!fbx_rename_available()){fb_message("Rename unavailable on this volume");return;}
 if(!fb_name_valid(fb_editor,sizeof(fb_editor))){fb_message("Invalid name. No slash, backslash, . or ..");return;}
 char destination[FB_PATH_CAP];
 if(!fb_join(fb_path,fb_editor,destination,sizeof(destination))){fb_message("Path too long");return;}
 if(!strcmp(fb_file_path,destination)){fb_mode=FB_ACTIONS;fb_message("Name unchanged");return;}
 if(fbx_exists(fb_volume,destination)){fb_message("That name already exists");return;}
 const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(fb_volume);
 if(!ext->rename(fb_volume->context,fb_file_path,destination)){fb_error("Rename failed");return;}
 fbx_success("Renamed successfully");
}
static void fbx_cancel(void){
 /* The paper destination's Cancel button reaches here directly. Retry retained
  * closes before switching volumes, with their original commit/abort intent. */
 if(!portable_file_browser_safe()&&!portable_file_browser_close()){
  fb_error("Close failed. Cancel retries safely.");return;
 }
 if(fb_storage_index!=fb_source_index&&!fbx_swap())return;
 strcpy(fb_path,fb_source_base);strcpy(fb_query,fb_source_query);fb_mode=FB_LIST;(void)fb_load(0);fb_message(fb_moving?"Move cancelled":"Copy cancelled");
}
static void fbx_destination(bool move){
 if(!fb_ready())return;
 if(move&&!fbx_rename_available()){fbx_notice("Move unavailable on this volume");return;}
 if(!move&&fb_selected.is_directory){fbx_notice("Folder copy is not supported. Select a file.");return;}
 fb_source_index=fb_storage_index;strcpy(fb_source_path,fb_file_path);strcpy(fb_source_base,fb_path);strcpy(fb_source_query,fb_query);
 fb_moving=move;fb_mode=FB_DESTINATION;fb_query[0]=0;strcpy(fb_path,"/");(void)fb_load(0);
}
static bool fbx_copy(const risc_storage_volume_api_v1 *source,const char *from,const risc_storage_volume_api_v1 *target,const char *to){
 if(!fbx_write_available(target)){fb_message("Destination is read-only");return false;}
 if(!source->refresh(source->context)||!source->ready(source->context)||!target->refresh(target->context)||!target->ready(target->context)){fb_message("Storage disconnected");return false;}
 if(fbx_exists(target,to)){fb_message("Destination already exists");return false;}
 uint64_t size=0;risc_storage_file_t input=source->file_open_read(source->context,from,&size);
 if(!input){fb_message("Cannot open source file");return false;}
 risc_storage_file_t output=target->file_open_write(target->context,to);
 if(!output){(void)fbx_close_file(source,input,true);fb_message("Cannot create destination file");return false;}
 static unsigned char buffer[RISC_STORAGE_VOLUME_IO_MAX];uint64_t copied=0;bool okay=true;
 while(copied<size){size_t want=size-copied<sizeof(buffer)?(size_t)(size-copied):sizeof(buffer);size_t got=source->file_read(source->context,input,buffer,want);
  if(!got||got>want){okay=false;break;}
  size_t written=0;while(written<got){size_t n=target->file_write(target->context,output,buffer+written,got-written);if(!n||n>got-written){okay=false;break;}written+=n;}
  if(!okay)break;
  copied+=got;fb_runtime->yield_ms(1);
 }
 /* Do not publish a destination if the source grew or shrank while copying. */
 uint64_t latest=0;bool directory=false;
 if(!source->stat(source->context,from,&latest,&directory)||directory||latest!=size)okay=false;
 if(!fbx_close_file(source,input,true))okay=false;
 if(PORTABLE_APP_RETAINED())return false;
 bool committed=okay&&copied==size;
 if(!fbx_close_file(target,output,committed))okay=false;
 if(!okay){fb_message(portable_file_browser_safe()?"Copy failed; incomplete destination aborted":"Close failed. Back retries safely.");return false;}
 return true;
}
static void fbx_commit_destination(void){
 if(!fb_ready())return;
 char destination[FB_PATH_CAP];if(!fb_join(fb_path,fb_selected.name,destination,sizeof(destination))){fb_message("Destination path too long");return;}
 bool same=fb_storage_index==fb_source_index;
 if(same&&!strcmp(destination,fb_source_path)){fb_message("Already in this folder");return;}
 if(same&&fb_selected.is_directory&&fbx_same_or_child(fb_path,fb_source_path)){fb_message("Cannot move a folder into itself");return;}
 if(fbx_exists(fb_volume,destination)){fb_message("Destination already exists");return;}
 fb_message(fb_moving?"Moving item...":"Copying file...");fb_draw();
 bool okay=false;
 if(fb_moving){
  if(!same){fb_message("Move requires the same volume. Use Copy instead.");return;}
  const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(fb_volume);
  okay=ext&&ext->rename&&ext->rename(fb_volume->context,fb_source_path,destination);
  if(!okay)fb_error("Move failed");
 }else okay=fbx_copy(same?fb_volume:fb_other_volume,fb_source_path,fb_volume,destination);
 if(!okay)return;
 if(fb_storage_index!=fb_source_index&&!fbx_swap())return;
 strcpy(fb_path,fb_source_base);strcpy(fb_query,fb_source_query);fb_mode=FB_LIST;(void)fb_load(0);fb_message(fb_moving?"Moved successfully":"Copied successfully");
}
/* Same recursive-folder delete operation as Reader, using bounded path memory.
 * Close each directory before descending/removing; a failed close is retained
 * and halts mutation. Reopening the parent avoids stale directory cursors. */
static bool fbx_remove_tree(const char *root){
 char path[FB_PATH_CAP];strcpy(path,root);
 for(;;){
  uint64_t size;bool directory;
  if(!fb_volume->stat(fb_volume->context,path,&size,&directory))return false;
  if(directory){
   risc_storage_dir_t dir=fb_volume->dir_open(fb_volume->context,path);if(!dir)return false;
   risc_storage_dirent_v1 entry={0};bool child=fb_volume->dir_next(fb_volume->context,dir,&entry);
   const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(fb_volume);
   bool error=ext&&ext->handle_error?ext->handle_error(fb_volume->context,dir,true)!=0:false;
   if(!ext||!ext->handle_error){char error_text[96]={0};error=fb_volume->last_error(fb_volume->context,error_text,sizeof(error_text))&&error_text[0];}
   if(!fbx_close_dir(fb_volume,dir)||error)return false;
   if(child){char next[FB_PATH_CAP];if(!fb_name_valid(entry.name,sizeof(entry.name))||entry.is_directory>1||!fb_join(path,entry.name,next,sizeof(next)))return false;strcpy(path,next);fb_runtime->yield_ms(1);continue;}
  }
  if(!fb_volume->remove(fb_volume->context,path))return false;
  if(!strcmp(path,root))return true;
  if(!fb_parent(path)||!fbx_same_or_child(path,root))return false;
  fb_runtime->yield_ms(1);
 }
}
static void fbx_delete(void){
 if(!fb_ready())return;
 if(!fb_volume->remove){fbx_notice("Delete unavailable on this volume");return;}
 fb_message("Deleting selected item...");fb_draw();
 if(fbx_remove_tree(fb_file_path))fbx_success("Deleted successfully");
 else {fb_notice_return=FB_LIST;fb_mode=FB_NOTICE;fb_error(portable_file_browser_safe()?"Delete stopped; some items may remain":"Close failed. Back retries safely.");}
}
static bool fbx_handlers(void){
#ifdef PORTABLE_FILE_BROWSER_HANDLERS
 if(fb_handler_api)return true;
 if(fb_handler_grant.api){fb_message("File handler service release failed");return false;}
 fb_handler_grant=(risc_runtime_capability_v1){.struct_size=sizeof(fb_handler_grant)};
 if(fb_runtime->acquire("file.open",T5_FILE_OPEN_API_VERSION,0,&fb_handler_grant)){
  const t5_file_open_api_v1 *api=fb_handler_grant.api;
  if(api&&api->api_version==T5_FILE_OPEN_API_VERSION&&api->struct_size>=sizeof(*api)&&api->handler_count&&api->handler_get&&api->open_request){fb_handler_api=api;return true;}
  PORTABLE_APP_RETAIN();if(PORTABLE_APP_RETAINED())return false;
  if(fb_runtime->release(&fb_handler_grant))memset(&fb_handler_grant,0,sizeof(fb_handler_grant));
 }
#endif
 fb_message("File handler service unavailable");return false;
}
static void fbx_resume_result(void){
#ifdef PORTABLE_FILE_BROWSER_HANDLERS
 char previous[sizeof(fb_status)];strcpy(previous,fb_status);bool feedback=fb_feedback;
 if(!fbx_handlers()){strcpy(fb_status,previous);fb_feedback=feedback;return;}
 int32_t error=0;uint64_t cookie=0;
 if(fb_handler_api->open_take_result&&fb_handler_api->open_take_result(&error,&cookie)&&cookie==UINT64_C(0x4642524f57534552)){
  if(error){char message[96];snprintf(message,sizeof(message),"File handler returned error %d",(int)error);fb_message(message);}
  else fb_message("File handler returned");
 }
#endif
}
static bool fbx_handler_page(void){
 unsigned n=fb_handler_count-fb_handler_offset;if(n>fb_page_rows)n=fb_page_rows;
 for(unsigned i=0;i<n;i++){t5_file_handler_t *h=&fb_handlers[i];memset(h,0,sizeof(*h));if(!fb_handler_api->handler_get(fb_handler_path,fb_handler_offset+i,h)||!memchr(h->app_id,0,sizeof(h->app_id))||!h->app_id[0]||!memchr(h->display_name,0,sizeof(h->display_name))||!memchr(h->icon,0,sizeof(h->icon))||h->kind>T5_FILE_HANDLER_SYSTEM_READER){fbx_notice("Invalid file handler data");return false;}}
 fb_choice=0;fb_mode=FB_HANDLERS;fb_dirty=true;return true;
}
static void fbx_dispatch(unsigned selected){
 unsigned n=fb_handler_count-fb_handler_offset;if(n>fb_page_rows)n=fb_page_rows;if(selected>=n)return;
 if(!portable_file_browser_safe()){fbx_notice("Close failed. Back retries safely.");return;}
 t5_file_handler_t handler=fb_handlers[selected];
 /* Reader's system-reader dispatch is a separate browser service, not an
  * invented native launch. A composed broker may expose normal app handlers. */
 if(handler.kind!=T5_FILE_HANDLER_APP){fbx_notice("System document reader unavailable");return;}
 if(!fb_handler_api->open_request(fb_handler_path,handler.app_id,UINT64_C(0x4642524f57534552))){fbx_notice("File handler launch failed");return;}
 /* Successful request is terminal even if capability release must be retried. */
 if(!portable_file_browser_close()){if(PORTABLE_APP_RETAINED())return;fb_runtime->diagnostic("FILE_BROWSER handler cleanup-unconfirmed; invocation retained");for(;;)fb_runtime->yield_ms(50);}
 fb_terminal=true;
}
static void fbx_open(bool choose){
 if(fb_selected.is_directory){strcpy(fb_path,fb_file_path);fb_query[0]=0;fb_mode=FB_LIST;(void)fb_load(0);return;}
 if(fb_storage_index){fbx_notice("Copy this file to primary storage before opening");return;}
 if(!fb_ready())return;
 size_t len=strlen(fb_selected.name);
 if(!choose&&len>=4&&fb_contains(fb_selected.name+len-4,".elf")){
  fbx_notice("Install and admit this SD app before launching it");return;
 }
 if(strlen(fb_file_path)+3>=sizeof(fb_handler_path)){fbx_notice("File handler path is too long");return;}
 strcpy(fb_handler_path,"/sd");strcpy(fb_handler_path+3,fb_file_path);
 if(!fbx_handlers()){fb_notice_return=FB_ACTIONS;fb_mode=FB_NOTICE;return;}
 fb_handler_count=fb_handler_api->handler_count(fb_handler_path);if(fb_handler_count>128)fb_handler_count=128;
 if(!fb_handler_count){fbx_notice("No declared application for this file type");return;}
 fb_handler_offset=0;if(fbx_handler_page()&&!choose&&fb_handler_count==1)fbx_dispatch(0);
}
static void fbx_activate(void){
 if(fb_mode==FB_NOTICE){fb_mode=fb_notice_return;fb_choice=0;return;}
 if(fb_mode==FB_RENAME){fb_filter_key(fb_choice);return;}
 if(fb_mode==FB_DELETE){if(fb_choice==1)fbx_delete();else {fb_mode=FB_ACTIONS;fb_choice=0;}return;}
 if(fb_mode==FB_HANDLERS){fbx_dispatch(fb_choice);return;}
 if(fb_mode==FB_DESTINATION){if(fb_choice<fb_count){char next[FB_PATH_CAP];if(fb_join(fb_path,fb_rows[fb_choice].name,next,sizeof(next))){strcpy(fb_path,next);(void)fb_load(0);}}return;}
 if(fb_mode!=FB_ACTIONS)return;
 if(!fb_ready()){fb_notice_return=FB_LIST;fb_mode=FB_NOTICE;return;}
 switch(fb_choice){
  case 0:fbx_open(false);break;
  case 1:fbx_open(true);break;
  case 2:if(fb_selected.is_directory){fbx_notice("Select a file to preview");break;}fb_mode=FB_PREVIEW;fb_offset=0;fb_hex=false;(void)fb_preview_read();break;
  case 3:if(!fbx_rename_available()){fbx_notice("Rename unavailable on this volume");break;}strcpy(fb_editor,fb_selected.name);fb_key_page=PWK_INITIAL_PAGE;fbp_key_page=0;fb_choice=0;fb_mode=FB_RENAME;fb_message("Done renames / Cancel keeps original");break;
  case 4:fbx_destination(true);break;
  case 5:fbx_destination(false);break;
  case 6:if(!fb_volume->remove){fbx_notice("Delete unavailable on this volume");break;}fb_mode=FB_DELETE;fb_choice=0;fb_message("Deletion cannot be undone");break;
 }
}
static void fbx_move(int direction){
 if(fb_mode==FB_DESTINATION){if(direction<0)fb_page_previous();else fb_page_next();return;}
 if(fb_mode==FB_HANDLERS){unsigned n=fb_handler_count-fb_handler_offset;if(n>fb_page_rows)n=fb_page_rows;if(direction>0&&fb_choice+1>=n){if(fb_handler_offset+n<fb_handler_count){fb_handler_offset+=n;(void)fbx_handler_page();}}else if(direction<0&&!fb_choice){if(fb_handler_offset){fb_handler_offset-=fb_page_rows;(void)fbx_handler_page();}}else fb_choice=(unsigned)((int)fb_choice+direction);fb_dirty=true;return;}
 unsigned count=fb_mode==FB_ACTIONS?7:fb_mode==FB_RENAME?PWK_COUNT:fb_mode==FB_DELETE?2:1;
 fb_choice=(fb_choice+count+(unsigned)direction)%count;fb_action_page=fb_choice/fb_page_rows;fb_dirty=true;
}
