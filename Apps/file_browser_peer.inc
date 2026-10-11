/* Shared Nova/paper peer UI. Commands enqueue work; the ordinary foreground
 * tick owns all BLE, TCP and bounded storage operations. */
static portable_peer_status fpeer_status;
static portable_peer_row fpeer_selected;
static unsigned fpeer_choice,fpeer_page,fpeer_edit_kind;
static bool fpeer_open_after,fpeer_launch;
static uint32_t fpeer_pair_generation,fpeer_pair_number;
static char fpeer_local_folder[FB_PATH_CAP],fpeer_upload_path[FB_PATH_CAP],fpeer_upload_name[128],fpeer_launch_path[FB_PATH_CAP];
static const char *const fpeer_actions[]={"Download","Download and open","Rename","Move to path","Copy to path","Delete","Replace with selected local file","Back to folder"};
static bool fpeer_close(void){
 if(!fpeer)return true;
 if(!portable_file_browser_services_begin())return false;
 portable_peer_stop(fpeer);
portable_peer_tick(fpeer,fsh_clock());
portable_peer_status_get(fpeer,&fpeer_status);
 if(fpeer_status.state==PEER_RETAINED){fsh_retain();
return false;
}
 if(!portable_file_browser_services_end())return false;
 if(fpeer_status.state!=PEER_OFF||fsh_service_ending)return false;
 if(!portable_peer_destroy(fpeer)){fsh_retain();
return false;
}fpeer=NULL;
return true;
}
static void fpeer_open(void){
 if(!portable_file_browser_safe()||!fb_ready())return;
 fpeer_mode=true;
fpeer_editing=false;
fpeer_page=fpeer_choice=0;
fpeer_open_after=fpeer_launch=false;
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
 if(!portable_file_browser_services_begin())return;
 portable_peer_tick(fpeer,now);
portable_peer_status next;
portable_peer_status_get(fpeer,&next);
 if(next.state==PEER_RETAINED){fsh_retain();
return;
}
 if(!portable_file_browser_services_end())return;
 if(memcmp(&next,&fpeer_status,sizeof(next))){
  if(next.state!=fpeer_status.state)fpeer_choice=0;
  fpeer_status=next;
fb_dirty=true;
 }
 if(next.state==PEER_READY&&fpeer_open_after){
  fpeer_open_after=false;
  if(next.published_path[0]&&!risc_protected_settings_path(next.published_path)&&strlen(next.published_path)<sizeof(fpeer_launch_path)){
   strcpy(fpeer_launch_path,next.published_path);
fpeer_launch=true;
fsh_phase=FSH_STOPPING;
  }
 }
 if(next.state==PEER_OFF){fsh_error(next.message[0]?next.message:"Peer session ended");
fsh_phase=FSH_STOPPING;
}
}
static void fpeer_after_close(void){
 if(!fpeer_launch||fsh_phase!=FSH_OFF)return;
fpeer_launch=false;
fpeer_mode=false;
 strcpy(fb_file_path,fpeer_launch_path);
memset(&fb_selected,0,sizeof(fb_selected));
 const char*name=strrchr(fb_file_path,'/');
snprintf(fb_selected.name,sizeof(fb_selected.name),"%s",name?name+1:fb_file_path);
 fb_mode=FB_ACTIONS;
fb_choice=0;
fb_dirty=true;
#ifndef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
 fbx_open(false);
 /* Existing declared file.open broker, including ELF rejection. */
#endif
}
static unsigned fpeer_count(void){if(fpeer_status.state==PEER_PAIRING)return 3;
if(fpeer_status.state==PEER_DISCOVERING)return fpeer_status.count+1;
if(fpeer_status.state!=PEER_READY)return 1;
if(fpeer_page==2||fpeer_page==3)return 2;
if(fpeer_page==1)return 8;
return fpeer_status.count+5;
}
static void fpeer_move(int direction){unsigned count=fpeer_count();
if(direction<0)fpeer_choice=fpeer_choice?fpeer_choice-1:count-1;
else fpeer_choice=(fpeer_choice+1)%count;
fb_dirty=true;
}
static const char *fpeer_label(unsigned i,char*out,size_t n){
 if(fpeer_status.state==PEER_PAIRING)return i==0?"Reject":i==1?"Match":"Stop";
 if(fpeer_status.state==PEER_DISCOVERING){portable_peer_row row;
if(i<fpeer_status.count&&portable_peer_row_get(fpeer,i,&row)){snprintf(out,n,"%s",row.name[0]?row.name:"RiscRTE Files");
return out;
}return "Stop";
}
 if(fpeer_status.state!=PEER_READY)return fsh_phase==FSH_OFF?"Reconnect":"Stop";
 if(fpeer_page==3)return i?"Replace existing file":"Cancel replacement";
 if(fpeer_page==2)return i?"Delete permanently":"Cancel deletion";
 if(fpeer_page==1)return fpeer_actions[i];
 portable_peer_row row;
if(i<fpeer_status.count&&portable_peer_row_get(fpeer,i,&row)){snprintf(out,n,"%s%s",row.directory?"/ ":"",row.name);
return out;
}
 unsigned action=i-fpeer_status.count;
return action==0?"Parent folder":action==1?"Upload selected local file":action==2?"New folder":action==3?"Folder actions":"Stop";
}
static void fpeer_edit(unsigned kind,const char*initial){fpeer_edit_kind=kind;
snprintf(fb_editor,sizeof(fb_editor),"%s",initial);
fpeer_editing=true;
fb_key_page=PWK_INITIAL_PAGE;
fbp_key_page=0;
fb_choice=0;
fb_mode=FB_RENAME;
fb_message(kind==3||kind==4?"Enter absolute remote destination":"Enter a name");
}
static void fpeer_edit_done(void){
 char path[513];
bool okay=false;
 if(fpeer_edit_kind==3||fpeer_edit_kind==4){if(fb_editor[0]!='/'){fb_message("Use an absolute remote path");
return;
}strcpy(path,fb_editor);
}
 else if(!fb_name_valid(fb_editor,sizeof(fb_editor))||!fb_join(fpeer_status.path,fb_editor,path,sizeof(path))){fb_message("Invalid name or path");
return;
}
 if(fpeer_edit_kind==0)okay=portable_peer_mutate(fpeer,"MKCOL",path,NULL);
 else okay=portable_peer_mutate(fpeer,fpeer_edit_kind==4?"COPY":"MOVE",fpeer_selected.path,path);
 if(!okay){fb_message("Peer is busy or the session ended");
return;
}
 fpeer_editing=false;
fb_mode=FB_SHARING;
fpeer_page=fpeer_choice=0;
fb_dirty=true;
}
static void fpeer_action(void){
 if(fsh_phase==FSH_OFF){fpeer_open();
return;
}
 if(fpeer_status.state==PEER_PAIRING){
  if(fpeer_choice<2){if(!portable_file_browser_setup_frame_ready()||fpeer_pair_generation!=fpeer_status.pairing_generation||fpeer_pair_number!=fpeer_status.pairing_number)return;
   (void)portable_peer_confirm(fpeer,fpeer_pair_generation,fpeer_pair_number,fpeer_choice==1);
fpeer_pair_generation=0;
  }else fsh_phase=FSH_STOPPING;
 }else if(fpeer_status.state==PEER_DISCOVERING){portable_peer_row row;
if(fpeer_choice<fpeer_status.count&&portable_peer_row_get(fpeer,fpeer_choice,&row))(void)portable_peer_select(fpeer,row.peer_id);
else fsh_phase=FSH_STOPPING;
}
 else if(fpeer_status.state!=PEER_READY)fsh_phase=FSH_STOPPING;
 else if(fpeer_page==3){if(fpeer_choice){if(!fpeer_upload_path[0])fsh_error("Select a local replacement file first");
else (void)portable_peer_replace(fpeer,fb_volume,fpeer_upload_path,fpeer_selected.path);
}fpeer_page=fpeer_choice=0;
}
 else if(fpeer_page==2){if(fpeer_choice)(void)portable_peer_mutate(fpeer,"DELETE",fpeer_selected.path,NULL);
fpeer_page=fpeer_choice=0;
}
 else if(fpeer_page==1){
  char destination[FB_PATH_CAP];
switch(fpeer_choice){
   case 0:case 1:
    if(fpeer_selected.directory){fsh_error("Open the folder to select a file");
break;
}
    if(fpeer_choice==1&&fb_storage_index){fsh_error("Select primary storage before download and open");
break;
}
    if(fb_join(fpeer_local_folder,fpeer_selected.name,destination,sizeof(destination))&&portable_peer_download(fpeer,fpeer_selected.path,fb_volume,destination)){fpeer_open_after=fpeer_choice==1;
fpeer_page=fpeer_choice=0;
}else fsh_error("Download unavailable for this local volume");
break;
   case 2:fpeer_edit(2,fpeer_selected.name);
break;
   case 3:fpeer_edit(3,fpeer_selected.path);
break;
   case 4:fpeer_edit(4,fpeer_selected.path);
break;
   case 6:fpeer_page=3;
fpeer_choice=0;
break;
   case 5:fpeer_page=2;
fpeer_choice=0;
break;
   default:fpeer_page=fpeer_choice=0;
(void)portable_peer_list(fpeer,fpeer_status.path);
break;
  }
 }else if(fpeer_choice<fpeer_status.count){
  if(portable_peer_row_get(fpeer,fpeer_choice,&fpeer_selected)){if(fpeer_selected.directory)(void)portable_peer_list(fpeer,fpeer_selected.path);
else {fpeer_page=1;
fpeer_choice=0;
}}
 }else{unsigned action=fpeer_choice-fpeer_status.count;
char path[513];
  if(action==0){strcpy(path,fpeer_status.path);
(void)fb_parent(path);
(void)portable_peer_list(fpeer,path);
}
  else if(action==1){if(!fpeer_upload_path[0])fsh_error("Select a local file before opening Nearby files");
else if(fb_join(fpeer_status.path,fpeer_upload_name,path,sizeof(path)))(void)portable_peer_upload(fpeer,fb_volume,fpeer_upload_path,path);
}
  else if(action==2)fpeer_edit(0,"");
else if(action==3){if(!strcmp(fpeer_status.path,"/")){fsh_error("The shared root cannot be renamed or deleted");}else{memset(&fpeer_selected,0,sizeof(fpeer_selected));fpeer_selected.directory=true;strcpy(fpeer_selected.path,fpeer_status.path);const char*n=strrchr(fpeer_status.path,'/');snprintf(fpeer_selected.name,sizeof(fpeer_selected.name),"%s",n?n+1:fpeer_status.path);fpeer_page=1;fpeer_choice=0;}}
else fsh_phase=FSH_STOPPING;
 }
 fb_dirty=true;
}
static void fpeer_draw(void){
 if(fb_paper&&!paper_frame_ready())return;
 portable_peer_status frame=fpeer_status;
bool pairing=frame.state==PEER_PAIRING;
char title[64],label[160],detail[96];
 snprintf(title,sizeof(title),pairing?"COMPARE %06u":"NEARBY FILES",frame.pairing_number);
 snprintf(detail,sizeof(detail),"%s",pairing?"Compare both numbers. Match only if identical.":fsh_message[0]?fsh_message:frame.message[0]?frame.message:frame.state==PEER_DISCOVERING?"Select the device sharing its files":frame.state==PEER_READY?frame.path:"Connecting; Back cancels");
 unsigned count=fpeer_count();
if(fpeer_choice>=count)fpeer_choice=0;
unsigned first=fpeer_choice/2*2;
 fb_dirty=false;
fpeer_pair_generation=0;
 if(fb_paper){fb_paper->begin();
fbp_text(32,28,fbp_w()-64,title,true,true,false);
fbp_wrap(32,122,fbp_w()-64,3,detail,true);
  for(unsigned j=0;j<2&&first+j<count;j++)fbp_button(32,256+(int)j*104,fbp_w()-64,88,fpeer_label(first+j,label,sizeof(label)),NULL,first+j==fpeer_choice);
  fbp_button(32,fbp_h()-216,(fbp_w()-80)/2,88,"PREVIOUS",NULL,false);
fbp_button(fbp_w()/2+8,fbp_h()-216,(fbp_w()-80)/2,88,"NEXT",NULL,false);
fbp_button(32,fbp_h()-112,fbp_w()-64,88,"BACK / STOP",NULL,false);
 }else{portable_nova_begin();
portable_nova_header(title);
portable_nova_wrap(3,16,52,208,12,2,detail,NOVA_CAP);
  for(unsigned j=0;j<2&&first+j<count;j++)portable_nova_row(16,76+(int)j*42,208,38,fpeer_label(first+j,label,sizeof(label)),"",first+j==fpeer_choice);
  portable_nova_button(16,164,100,28,"Previous",false);
portable_nova_button(124,164,100,28,"Next",false);
portable_nova_button(16,196,208,36,"Back / Stop",false);
 }
 fb_app->present(false);
if(pairing){fpeer_pair_generation=frame.pairing_generation;
fpeer_pair_number=frame.pairing_number;
}
}
static bool fpeer_touch(int x,int y){
 if(fb_paper){if(x<32||x>=fbp_w()-32)return false;
for(unsigned j=0;j<2;j++)if(y>=256+(int)j*104&&y<344+(int)j*104){unsigned i=fpeer_choice/2*2+j;
if(i<fpeer_count()){fpeer_choice=i;
fpeer_action();
}return false;
}
  if(y>=fbp_h()-216&&y<fbp_h()-128){fpeer_move(x<fbp_w()/2?-1:1);
return false;
}
  if(y>=fbp_h()-112&&y<fbp_h()-24)return fsh_back();
 }else{if(x<16||x>=224)return false;
for(unsigned j=0;j<2;j++)if(y>=76+(int)j*42&&y<114+(int)j*42){unsigned i=fpeer_choice/2*2+j;
if(i<fpeer_count()){fpeer_choice=i;
fpeer_action();
}return false;
}
  if(y>=164&&y<192){fpeer_move(x<120?-1:1);
return false;
}if(y>=196&&y<232)return fsh_back();
 }return false;
}
