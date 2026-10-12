/* Optional disk-backed selected-source handoff. The ordinary action/handler
 * screens remain unchanged. One bounded chunk is copied per foreground tick;
 * Runtime owns sealed bytes after Files unloads, never this adapter or volume. */
static uint64_t fsource_token,fsource_size,fsource_copied;
static risc_storage_file_t fsource_input;
static const risc_storage_volume_api_v1 *fsource_local;
static char fsource_name[RISC_FILE_OPEN_SOURCE_NAME_MAX],fsource_path[FB_PATH_CAP],fsource_handler[T5_FILE_HANDLER_ID_MAX];
static uint8_t fsource_buffer[RISC_FILE_OPEN_SOURCE_IO_MAX];
static uint32_t fsource_used,fsource_at,fsource_progress_at;
static int32_t fsource_error;
static bool fsource_writer,fsource_sealed,fsource_published;
static char fsource_stage[FB_PATH_CAP];
static bool fsource_busy(void){return fsource_state>=FSOURCE_START;}
static void fsource_reset(void){memset(&fsource_grant,0,sizeof(fsource_grant));fsource_api=NULL;fsource_state=FSOURCE_NONE;fsource_remote=fsource_handoff=false;fsource_token=fsource_size=fsource_copied=0;fsource_input=0;fsource_local=NULL;fsource_error=0;fsource_writer=fsource_sealed=fsource_published=false;fsource_used=fsource_at=fsource_progress_at=0;fsource_name[0]=fsource_path[0]=fsource_handler[0]=fsource_stage[0]=0;}
static bool fsource_result(int32_t result){
 fsource_error=result;
 if(result==RISC_FILE_SOURCE_RETAINED){PORTABLE_APP_RETAIN();return false;}
 return result==RISC_FILE_SOURCE_OK;
}
static const char *fsource_reason(void){return fsource_error==RISC_FILE_SOURCE_NO_SPACE?"Not enough temporary storage to open this file":fsource_error==RISC_FILE_SOURCE_UNAVAILABLE?"Temporary file storage unavailable":fsource_error==RISC_FILE_SOURCE_INCOMPLETE?"File changed or transfer incomplete":fsource_error==RISC_FILE_SOURCE_RETAINED?"File storage custody retained; restart required":"Cannot prepare file for the selected application";}
static bool fsource_acquire(void){
 if(fsource_api)return true;
 if(fsource_grant.api)return false;
 fsource_grant=(risc_runtime_capability_v1){.struct_size=sizeof(fsource_grant)};
 if(!fb_runtime->acquire(RISC_FILE_OPEN_SOURCE_CAPABILITY,1,0,&fsource_grant))return false;
 const risc_file_open_source_api_v1*a=fsource_grant.api;
 if(!a||a->api_version!=1||a->struct_size<sizeof(*a)||!a->handler_count||!a->handler_get||!a->handler_supported||!a->begin||!a->write||!a->finish||!a->discard||!a->open_request){PORTABLE_APP_RETAIN();return false;}
 fsource_api=a;return true;
}
static bool fsource_resume_result(void){
 if(!fsource_acquire()||!fsource_api->take_result)return false;
 int32_t error=0;uint64_t cookie=0;
 if(!fsource_api->take_result(&error,&cookie)||cookie!=UINT64_C(0x4642524f57534552))return false;
 if(error){char message[96];snprintf(message,sizeof(message),"File handler returned error %d",(int)error);fb_message(message);}
 else fb_message("File handler returned");
 return true;
}
static bool fsource_handler_get(uint32_t index,t5_file_handler_t*out){return fsource_api&&fsource_api->handler_get(fsource_name,index,out);}
static void fsource_open_scoped(bool choose,bool remote){
 if(fsource_state!=FSOURCE_NONE){fbx_notice("Finish or cancel the current Open first");return;}
 if(!fb_name_valid(fb_selected.name,sizeof(fb_selected.name))){fbx_notice("Invalid file name");return;}
 size_t n=strlen(fb_selected.name);
 if(n>=4&&fb_contains(fb_selected.name+n-4,".elf")){fbx_notice("Install and admit this app before launching it");return;}
 if(!fsource_acquire()){fbx_notice("Selected-volume file opening is unavailable");return;}
 snprintf(fsource_name,sizeof(fsource_name),"%s",fb_selected.name);snprintf(fsource_path,sizeof(fsource_path),"%s",fb_file_path);
 fsource_remote=remote;fsource_local=remote?NULL:fb_volume;fsource_size=fb_selected.size;fsource_copied=0;fsource_error=0;
 fb_handler_count=fsource_api->handler_count(fsource_name);if(fb_handler_count>128)fb_handler_count=128;
 if(!fb_handler_count){fbx_notice("No declared application for this file type");return;}
 fsource_state=FSOURCE_CHOOSE;fb_handler_offset=0;
 if(!fbx_handler_page()){fsource_state=FSOURCE_NONE;return;}
 if(!choose&&fb_handler_count==1)fbx_dispatch(0);
}
static void fsource_open(bool choose,bool remote){if(!fb_service_begin())return;fsource_open_scoped(choose,remote);(void)fb_service_end();}
static void fsource_start(unsigned selected){
 unsigned n=fb_handler_count-fb_handler_offset;if(n>fb_page_rows)n=fb_page_rows;
 if(selected>=n||fsource_state!=FSOURCE_CHOOSE)return;
 const t5_file_handler_t*h=&fb_handlers[selected];
 if(h->kind!=T5_FILE_HANDLER_APP||!fsource_api->handler_supported(h->app_id)){fsource_state=FSOURCE_NONE;fbx_notice("This application cannot read selected-volume sources");return;}
 snprintf(fsource_handler,sizeof(fsource_handler),"%s",h->app_id);
 fsource_state=FSOURCE_START;fsource_used=fsource_at=0;fsource_writer=fsource_sealed=fsource_published=false;
 fb_notice_return=FB_ACTIONS;fb_mode=FB_NOTICE;fb_message("Preparing file; Back cancels");
}
static bool fsource_discard(void){
 if(!fsource_token)return true;
 const int32_t prior=fsource_error;
 if(!fsource_result(fsource_api->discard(fsource_token)))return false;
 fsource_error=prior;
 fsource_token=0;fsource_writer=fsource_sealed=fsource_published=false;return true;
}
static bool fsource_input_close(void){
 if(!fsource_input)return true;
 if(!fbx_close_file(fsource_local,fsource_input,true)){fsource_input=0;return false;}
 fsource_input=0;return true;
}
static void fsource_progress(void){uint32_t now=fb_app->millis();if(fsource_copied&&fsource_copied!=fsource_size&&(uint32_t)(now-fsource_progress_at)<250)return;fsource_progress_at=now;char message[96];snprintf(message,sizeof(message),"Opening: %llu / %llu bytes. Back cancels",(unsigned long long)fsource_copied,(unsigned long long)fsource_size);fb_message(message);}
static void fsource_fail(int32_t error){fsource_error=error;fsource_state=FSOURCE_CANCEL;fb_message(fsource_reason());}

#ifdef PORTABLE_FILE_PEER
/* Ephemeral target adapter used only while the Files invocation is alive. The
 * WebDAV controller's existing checked transaction contract remains intact:
 * stage -> sync -> close -> rename publishes only the sealed Runtime token. */
static bool fsource_volume_ready(void*c){(void)c;return fsource_token&&fsource_error!=RISC_FILE_SOURCE_RETAINED;}
static bool fsource_volume_stat(void*c,const char*p,uint64_t*z,bool*d){(void)c;if(z)*z=0;if(d)*d=false;if(!z||!d)return false;if(!strcmp(p,"/")){*d=true;return true;}if((fsource_writer||fsource_sealed)&&!strcmp(p,fsource_stage)){*z=fsource_copied;return true;}if(fsource_published&&!strcmp(p,"/selected")){*z=fsource_size;return true;}return false;}
static risc_storage_file_t fsource_volume_open(void*c,const char*p){(void)c;if(!fsource_token||fsource_writer||fsource_sealed||strncmp(p,"/.peer-",7)||strlen(p)>=sizeof(fsource_stage))return 0;strcpy(fsource_stage,p);fsource_writer=true;return 1;}
static size_t fsource_volume_write(void*c,risc_storage_file_t h,const void*b,size_t n){(void)c;if(h!=1||!fsource_writer||!n||n>RISC_FILE_OPEN_SOURCE_IO_MAX)return 0;size_t at=0;while(at<n){uint32_t written=0;if(!fsource_result(fsource_api->write(fsource_token,(const uint8_t*)b+at,(uint32_t)(n-at),&written))||!written||written>n-at)return at;at+=written;fsource_copied+=written;}fsource_progress();return at;}
static bool fsource_volume_sync(void*c,risc_storage_file_t h){(void)c;if(h!=1||!fsource_writer||!fsource_result(fsource_api->finish(fsource_token)))return false;fsource_sealed=true;return true;}
static bool fsource_volume_close(void*c,risc_storage_file_t h,bool commit){(void)c;if(h!=1||!fsource_writer)return false;if(!commit&&!fsource_discard())return false;fsource_writer=false;return true;}
static bool fsource_volume_remove(void*c,const char*p){(void)c;if(!strcmp(p,fsource_stage))return fsource_discard();return false;}
static bool fsource_volume_rename(void*c,const char*from,const char*to){(void)c;if(!fsource_sealed||fsource_writer||strcmp(from,fsource_stage)||strcmp(to,"/selected"))return false;fsource_published=true;return true;}
static bool fsource_volume_error(void*c,char*out,size_t n){(void)c;if(!out||!n)return false;snprintf(out,n,"%s",fsource_error?fsource_reason():"");return true;}
static uint32_t fsource_volume_handle_error(void*c,uint32_t h,bool dir){(void)c;(void)h;(void)dir;return fsource_error?(uint32_t)-fsource_error:0;}
static bool fsource_volume_info(void*c,risc_storage_file_t h,uint64_t*z,uint64_t*o){(void)c;if(h!=1||!fsource_writer||!z||!o)return false;*z=*o=fsource_copied;return true;}
static int32_t fsource_volume_observe(void*c){(void)c;return fsource_error==RISC_FILE_SOURCE_RETAINED?RISC_STORAGE_STATE_RETAINED:RISC_STORAGE_STATE_READY;}
static const risc_storage_volume_api_v1_state fsource_volume={.prepared={.base={.sleep={.terminal={.power={.volume={.base={.api_version=1,.struct_size=sizeof(fsource_volume),.refresh=fsource_volume_ready,.ready=fsource_volume_ready,.stat=fsource_volume_stat,.file_open_write=fsource_volume_open,.file_write=fsource_volume_write,.file_close=fsource_volume_close,.remove=fsource_volume_remove,.last_error=fsource_volume_error},.file_info=fsource_volume_info,.file_sync=fsource_volume_sync,.handle_error=fsource_volume_handle_error,.rename=fsource_volume_rename}}}}}},.state_tag=RISC_STORAGE_STATE_TAG,.state_version=1,.observe=fsource_volume_observe};
static void fsource_remote_done(bool okay){
 if(fsource_state==FSOURCE_CANCEL)return;
 if(!okay||!fsource_sealed||!fsource_published){fsource_fail(fsource_error?fsource_error:RISC_FILE_SOURCE_IO);return;}
 fsource_state=FSOURCE_SEALED;fb_message("Closing connection before opening file");
#ifdef PORTABLE_FILE_PEER
 fsh_phase=FSH_STOPPING;
#endif
}

#endif
static void fsource_tick(void){
 if(!fsource_busy()||PORTABLE_APP_RETAINED())return;
 if(fsource_state==FSOURCE_REMOTE){
#ifdef PORTABLE_FILE_PEER
  if(!fpeer||fsh_phase==FSH_OFF){fsource_fail(RISC_FILE_SOURCE_IO);}
  else return;
#else
  fsource_fail(RISC_FILE_SOURCE_UNAVAILABLE);
#endif
 }
 if(fsource_state==FSOURCE_SEALED){
#ifdef PORTABLE_FILE_PEER
  if(fsh_phase!=FSH_OFF){fsh_phase=FSH_STOPPING;return;}
#endif
  if(!portable_file_browser_safe())return;
  if(!fsource_api->open_request(fsource_token,fsource_handler,UINT64_C(0x4642524f57534552))){fsource_fail(RISC_FILE_SOURCE_IO);return;}
  fsource_handoff=true;
  if(!portable_file_browser_close()){PORTABLE_APP_RETAIN();return;}
  fb_terminal=true;return;
 }
#ifdef PORTABLE_FILE_PEER
 if(fsource_state==FSOURCE_CANCEL&&fsource_remote&&fbm_operation)return;
#endif
 if(!fb_service_begin())return;
 if(fsource_state==FSOURCE_CANCEL){
  const int32_t prior=fsource_error;
  if(fsource_input_close()&&fsource_discard()){
   fsource_state=FSOURCE_NONE;fsource_remote=false;fsource_local=NULL;fb_notice_return=FB_ACTIONS;fb_mode=FB_NOTICE;
   fsource_error=prior;fb_message(prior?fsource_reason():"Open cancelled");
  }
  (void)fb_service_end();return;
 }
 if(fsource_state==FSOURCE_START){
  if(!fsource_remote&&!fsource_input){uint64_t size=0;fsource_input=fsource_local->file_open_read(fsource_local->context,fsource_path,&size);if(!fsource_input){fsource_fail(RISC_FILE_SOURCE_IO);(void)fb_service_end();return;}fsource_size=size;}
  int32_t rc=fsource_api->begin(fsource_name,fsource_size,&fsource_token);
  if(rc==RISC_FILE_SOURCE_BUSY){(void)fb_service_end();return;}
  if(!fsource_result(rc)){fsource_fail(rc);(void)fb_service_end();return;}
  fsource_copied=0;
  if(fsource_remote){
#ifdef PORTABLE_FILE_PEER
   if(!portable_peer_download(fpeer,fsource_path,&fsource_volume.prepared.base.sleep.terminal.power.volume.base,"/selected")){fsource_fail(RISC_FILE_SOURCE_IO);}
   else {fbm_queued();fbm_operation=FBM_SOURCE;fsource_state=FSOURCE_REMOTE;fsource_progress();}
#else
   fsource_fail(RISC_FILE_SOURCE_UNAVAILABLE);
#endif
  }else fsource_state=FSOURCE_LOCAL;
  (void)fb_service_end();return;
 }
 if(fsource_state==FSOURCE_LOCAL){
  if(fsource_at==fsource_used&&fsource_copied<fsource_size){uint64_t left=fsource_size-fsource_copied;size_t want=left<sizeof(fsource_buffer)?(size_t)left:sizeof(fsource_buffer);size_t got=fsource_local->file_read(fsource_local->context,fsource_input,fsource_buffer,want);if(!got||got>want){fsource_fail(RISC_FILE_SOURCE_IO);(void)fb_service_end();return;}fsource_used=(uint32_t)got;fsource_at=0;}
  if(fsource_at<fsource_used){uint32_t written=0;int32_t rc=fsource_api->write(fsource_token,fsource_buffer+fsource_at,fsource_used-fsource_at,&written);if(rc==RISC_FILE_SOURCE_BUSY){(void)fb_service_end();return;}if(!fsource_result(rc)||!written||written>fsource_used-fsource_at){fsource_fail(rc?rc:RISC_FILE_SOURCE_IO);(void)fb_service_end();return;}fsource_at+=written;fsource_copied+=written;fsource_progress();}
  if(fsource_copied==fsource_size){uint64_t current=0;bool directory=false;if(!fsource_local->stat(fsource_local->context,fsource_path,&current,&directory)||directory||current!=fsource_size){fsource_fail(RISC_FILE_SOURCE_INCOMPLETE);(void)fb_service_end();return;}if(!fsource_input_close()){fsource_fail(RISC_FILE_SOURCE_IO);(void)fb_service_end();return;}int32_t rc=fsource_api->finish(fsource_token);if(rc==RISC_FILE_SOURCE_BUSY){(void)fb_service_end();return;}if(fsource_result(rc)){fsource_state=FSOURCE_SEALED;fb_message("Opening selected application");}else fsource_fail(rc);}
 }
 (void)fb_service_end();
}
static bool fsource_back(void){
 if(fsource_state==FSOURCE_NONE)return false;
 if(fsource_state==FSOURCE_CHOOSE){fsource_state=FSOURCE_NONE;return false;}
 fsource_error=0;fsource_state=FSOURCE_CANCEL;
#ifdef PORTABLE_FILE_PEER
 if(fsource_remote&&fbm_operation){portable_peer_cancel_operation(fpeer);fbm_loading=false;fbm_operation=FBM_CANCEL;}
#endif
 fb_message("Cancelling Open");return true;
}
static bool fsource_close(void){
 if(PORTABLE_APP_RETAINED())return false;
 if(!fsource_input_close())return false;
 if(!fsource_handoff&&!fsource_discard())return false;
 if(fsource_grant.api&&!fb_runtime->release(&fsource_grant))return false;
 memset(&fsource_grant,0,sizeof(fsource_grant));fsource_api=NULL;fsource_state=FSOURCE_NONE;return true;
}
