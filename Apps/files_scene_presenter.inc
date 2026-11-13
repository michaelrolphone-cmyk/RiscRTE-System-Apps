/* Included after the unchanged Files domain. Copied semantic documents only;
 * no pixel input adapter, provider geometry, or storage/network calls in build.
 * Root must call commit only after successful publication and gate events by
 * session/sequence/neutral input. Pair consent additionally requires frame-ready. */
#include "RiscSceneComponentsV1.h"
enum {FP_BACK=1,FP_OPTIONS,FP_PREV,FP_NEXT,FP_TOGGLE,FP_UP,FP_HERE,FP_VOLUME,
 FP_REVIEW,FP_CONFIRM,FP_CANCEL,FP_RETRY_EDITOR,FP_SHARE,FP_TRANSPORT,FP_SETUP,
 FP_SETUP_REJECT,FP_SETUP_MATCH,FP_PEER_REJECT,FP_PEER_MATCH,FP_PEER_STOP,
 FP_ROW=256,FP_OPTION=512,FP_OPERATION=768,FP_HANDLER=1024,FP_PEER=1280};
#define FP_WINDOW 6u
typedef struct {
 bool modal,peer,setup,source_busy;unsigned mode,storage,page,count,page_rows,handler_offset,handler_count,window;
 unsigned peer_count,peer_state,setup_state,setup_provider,share_phase;
 uint32_t peer_generation,peer_number,setup_generation,setup_number;
 char path[FB_PATH_CAP],file_path[FB_PATH_CAP],source_path[FB_PATH_CAP],handler_path[FB_PATH_CAP];
 risc_storage_dirent_v1 rows[FB_MAX_PAGE_ROWS],selected;
 t5_file_handler_t handlers[FB_MAX_PAGE_ROWS];
 portable_peer_row peers[32];
} fp_identity;
static bool fp_modal,fp_failed,fp_built,fp_valid,fp_ready,fp_consumed;
static unsigned fp_window;
static uint64_t fp_sequence;
static fp_identity fp_pending,fp_committed;
static risc_components_document_v1 fp_pending_document,fp_document;
static bool fp_identity_copy(fp_identity *o){
 if(fb_count>FB_MAX_PAGE_ROWS||files_scene_peer_count>32||!fb_page_rows||fb_page_rows>FB_MAX_PAGE_ROWS||fb_handler_offset>fb_handler_count)return false;
 *o=(fp_identity){0};
 o->modal=fp_modal;o->peer=fpeer_mode;o->setup=fsh_setup_screen;o->source_busy=fsource_busy();
 o->mode=fb_mode;o->storage=fb_storage_index;o->page=fb_page;o->count=fb_count;o->page_rows=fb_page_rows;
 o->handler_offset=fb_handler_offset;o->handler_count=fb_handler_count;o->window=fp_window;
 o->peer_count=files_scene_peer_count;o->peer_state=fpeer_status.state;o->share_phase=fsh_phase;
 o->setup_state=fsh_setup_status.state;o->setup_provider=fsh_setup_status.provider_state;
 o->peer_generation=fpeer_status.pairing_generation;o->peer_number=fpeer_status.pairing_number;
 o->setup_generation=fsh_setup_status.pairing_generation;o->setup_number=fsh_setup_status.pairing_number;
 memcpy(o->path,fb_path,sizeof(o->path));memcpy(o->file_path,fb_file_path,sizeof(o->file_path));
 memcpy(o->source_path,fb_source_path,sizeof(o->source_path));memcpy(o->handler_path,fb_handler_path,sizeof(o->handler_path));
 memcpy(o->rows,fb_rows,sizeof(o->rows));o->selected=fb_selected;
 memcpy(o->handlers,fb_handlers,sizeof(o->handlers));memcpy(o->peers,files_scene_peer_rows,files_scene_peer_count*sizeof(o->peers[0]));
 return true;
}
static bool fp_current(void){
 fp_identity current;
 return files_scene_alive()&&fp_valid&&!fp_consumed&&fp_identity_copy(&current)&&!memcmp(&current,&fp_committed,sizeof(current));
}
static risc_scene_node_v1 *fp_node(risc_components_document_v1*d,uint32_t kind,uint32_t action,const char*label,const char*text){
 if(d->node_count>=RISC_COMPONENTS_MAX_NODES){fp_failed=true;return NULL;}
 unsigned i=d->node_count++;risc_scene_node_v1*n=&d->nodes[i];
 *n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action};
 snprintf(n->label,sizeof(n->label),"%s",label?label:"");snprintf(n->text,sizeof(n->text),"%s",text?text:"");return n;
}
/* Preserve every byte of bounded domain strings, instead of replacing full
 * paths/names with a truncated header. Provider scrolling owns their layout. */
static bool fp_text(risc_components_document_v1*d,const char*label,const char*s,size_t cap){
 size_t n=0;while(n<cap&&s[n])++n;if(n==cap){fp_failed=true;return false;}size_t at=0;
 do{char part[RISC_SCENE_TEXT];size_t k=n-at;if(k>=sizeof(part))k=sizeof(part)-1;memcpy(part,s+at,k);part[k]=0;
 if(!fp_node(d,RISC_SCENE_TEXT_NODE,0,at?"":label,part)){return false;}
 at+=k;
 }while(at<n);
 return true;
}
static void fp_button(risc_components_document_v1*d,uint32_t action,const char*label){(void)fp_node(d,RISC_SCENE_ACTION,action,label,"");}
static bool fp_build_document(risc_components_document_v1*d,uint32_t revision){
 if(!files_scene_alive()||!d||!revision||fb_mode>FB_SHARING||fb_count>FB_MAX_PAGE_ROWS||files_scene_peer_count>32)return false;
 
 *d=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*d),.revision=revision,.root=1,.route_count=1,.screen_key=1+fb_mode+256u*fp_window};
 d->routes[0]=(risc_scene_route_v1){.id=1,.back_action=FP_BACK};snprintf(d->routes[0].title,sizeof(d->routes[0].title),"FILES");
 if(fp_modal){d->flags=RISC_COMPONENTS_CONFIRM;d->cancel_action=FP_CANCEL;d->screen_key+=64;
  fp_node(d,RISC_SCENE_TEXT_NODE,0,fb_mode==FB_DELETE?"Delete permanently?":"Replace destination?","The preceding details identify this item. This action cannot be undone.");fp_button(d,FP_CANCEL,"Cancel");fp_button(d,FP_CONFIRM,fb_mode==FB_DELETE?"Delete":"Replace");return !fp_failed;}
 fp_node(d,RISC_COMPONENT_HEADER_ACTION,FP_BACK,"Back","");d->details[0].symbol=RISC_SYMBOL_BACK;
 if(!fp_text(d,"STATUS",fb_status,sizeof(fb_status)))return false;
 if(fsource_busy()){snprintf(d->routes[0].title,sizeof(d->routes[0].title),"OPENING FILE");return fp_text(d,"SOURCE",fsource_name,sizeof(fsource_name))&&fp_text(d,"PROGRESS",fb_status,sizeof(fb_status));}
 if(fb_mode==FB_SHARING){
  if(fsh_setup_screen){snprintf(d->routes[0].title,sizeof(d->routes[0].title),"BLUETOOTH SETUP");fp_text(d,"SETUP",fsh_setup_label(),256);
   bool pairing=fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_ACTIVE&&fsh_setup_status.provider_state==RISC_SETUP_PAIR_CONFIRM&&fsh_setup_status.pairing_generation;
   if(pairing){char number[16];snprintf(number,sizeof(number),"%06u",(unsigned)fsh_setup_status.pairing_number);fp_node(d,RISC_SCENE_TEXT_NODE,0,"COMPARE BOTH DEVICES",number);fp_button(d,FP_SETUP_REJECT,"Reject");fp_button(d,FP_SETUP_MATCH,"Match");}
   fp_text(d,"PRIVACY","BLE shares this temporary session only. HTTP file contents remain unencrypted.",128);return true;}
  if(fpeer_mode){snprintf(d->routes[0].title,sizeof(d->routes[0].title),"NEARBY FILES");if(!fp_text(d,"CONNECTION",fpeer_status.message,sizeof(fpeer_status.message)))return false;
   if(fpeer_status.state==PEER_PAIRING){char number[16];snprintf(number,sizeof(number),"%06u",(unsigned)fpeer_status.pairing_number);fp_node(d,RISC_SCENE_TEXT_NODE,0,"COMPARE BOTH DEVICES",number);fp_node(d,RISC_SCENE_TEXT_NODE,0,"CONSENT","Match only if both numbers are identical.");fp_button(d,FP_PEER_REJECT,"Reject");fp_button(d,FP_PEER_MATCH,"Match");}
   else if(fpeer_status.state==PEER_DISCOVERING){for(unsigned i=fp_window;i<files_scene_peer_count&&i<fp_window+FP_WINDOW;i++){fp_node(d,RISC_COMPONENT_ROW,FP_PEER+i,files_scene_peer_rows[i].name[0]?files_scene_peer_rows[i].name:"RiscRTE Files","");if(!fp_text(d,"DEVICE",files_scene_peer_rows[i].name,sizeof(files_scene_peer_rows[i].name))||!fp_text(d,"PEER PATH",files_scene_peer_rows[i].path,sizeof(files_scene_peer_rows[i].path)))return false;}if(!files_scene_peer_count)fp_node(d,RISC_COMPONENT_EMPTY,0,"Waiting for nearby Files","Back cancels");}
   else if(!fp_text(d,"PATH",fpeer_status.path,sizeof(fpeer_status.path)))return false;
   if(fpeer_status.state==PEER_DISCOVERING){fp_button(d,FP_PREV,"Previous devices");fp_button(d,FP_NEXT,"Next devices");}fp_button(d,FP_PEER_STOP,fsh_phase==FSH_OFF?"Reconnect":"Stop");return true;}
  snprintf(d->routes[0].title,sizeof(d->routes[0].title),"SHARE FILES");
  bool ready=fsh_phase==FSH_ACTIVE&&(fsh_status.state==PORTABLE_FILE_SHARING_LISTENING||fsh_status.state==PORTABLE_FILE_SHARING_SERVING);
  if(ready){if(!fp_text(d,"URL",fsh_url,sizeof(fsh_url))||!fp_text(d,"USERNAME",fsh_status.username,sizeof(fsh_status.username))||!fp_text(d,"TEMPORARY PASSWORD",fsh_status.password,sizeof(fsh_status.password)))return false;char timing[72];unsigned left=fsh_status.expires_ms>fsh_now?(unsigned)((fsh_status.expires_ms-fsh_now)/1000u):0;snprintf(timing,sizeof(timing),"%u:%02u left / %u requests",left/60,left%60,(unsigned)fsh_status.completed_requests);fp_node(d,RISC_SCENE_TEXT_NODE,0,"SESSION",timing);fp_button(d,FP_SETUP,"Bluetooth setup");}
  else fp_node(d,RISC_SCENE_TEXT_NODE,0,"SESSION",fsh_phase==FSH_OFF?"Share files for 5 minutes. Explicit confirmation is required.":"Preparing private session. Back cancels.");
  if(fsh_phase==FSH_OFF)fp_button(d,FP_TRANSPORT,fsh_direct?"Direct device (no router)":"Same LAN");
  fp_text(d,"SHARING",fsh_message,sizeof(fsh_message));fp_text(d,"PRIVACY","Authenticated HTTP. File contents are unencrypted. Confirm setup on both devices.",128);fp_button(d,FP_SHARE,fsh_phase==FSH_OFF?"Start sharing":"Stop");return true;
 }
 if(!fp_text(d,"VOLUME",fb_label,sizeof(fb_label))||!fp_text(d,"PATH",fb_path,sizeof(fb_path)))return false;
 if(fb_mode==FB_LIST||fb_mode==FB_DESTINATION){
  if(fb_mode==FB_LIST)fp_button(d,FP_OPTIONS,"Options");
  for(unsigned i=fp_window;i<fb_count&&i<fp_window+FP_WINDOW;i++){char size[32];fb_size_text(fb_rows[i].size,size,sizeof(size));fp_node(d,RISC_COMPONENT_ROW,FP_ROW+i,fb_rows[i].name,fb_rows[i].is_directory?"Folder":size);if(!fp_text(d,"NAME",fb_rows[i].name,sizeof(fb_rows[i].name)))return false;}
  if(!fb_count)fp_node(d,RISC_COMPONENT_EMPTY,0,"No entries",fb_status);
  char count[72];snprintf(count,sizeof(count),"Page %u / %u %s",fb_page+1,(unsigned)fb_matched,fb_query[0]?"matches":"items");fp_node(d,RISC_SCENE_TEXT_NODE,0,"LIST",count);
  fp_button(d,FP_PREV,"Previous page");fp_button(d,FP_NEXT,"Next page");
  if(fb_mode==FB_DESTINATION){fp_button(d,FP_CANCEL,"Cancel");fp_button(d,FP_UP,"Up");fp_button(d,FP_HERE,fb_moving?"Move here":"Copy here");if(!fb_moving)fp_button(d,FP_VOLUME,"Switch volume");}return true;
 }
 if(fb_mode==FB_OPTIONS){for(unsigned i=0;i<fb_option_count();i++)fp_node(d,RISC_COMPONENT_ROW,FP_OPTION+i,fb_options[i],i==2?(fb_hidden?"On":"Off"):"");return true;}
 if(fb_mode==FB_FILTER||fb_mode==FB_RENAME){if(!fp_text(d,"DRAFT",fb_editor,sizeof(fb_editor)))return false;fp_button(d,FP_RETRY_EDITOR,fb_mode==FB_FILTER?"Find file":"Edit name");return true;}
 if(!fp_text(d,"SELECTED ITEM",fb_selected.name,sizeof(fb_selected.name))||!fp_text(d,"FILE PATH",fb_file_path,sizeof(fb_file_path)))return false;
 if(fb_mode==FB_ACTIONS){for(unsigned i=0;i<FBX_ACTION_COUNT;i++)fp_node(d,RISC_COMPONENT_ROW,FP_OPERATION+i,fbx_labels[i],"");return true;}
 if(fb_mode==FB_HANDLERS){if(!fp_text(d,"HANDLER PATH",fb_handler_path,sizeof(fb_handler_path)))return false;if(fb_handler_offset>fb_handler_count)return false;unsigned n=fb_handler_count-fb_handler_offset;if(n>fb_page_rows)n=fb_page_rows;if(n>FB_MAX_PAGE_ROWS)return false;for(unsigned i=fp_window;i<n&&i<fp_window+FP_WINDOW;i++){fp_node(d,RISC_COMPONENT_ROW,FP_HANDLER+i,fb_handlers[i].display_name,"");if(!fp_text(d,"HANDLER",fb_handlers[i].display_name,sizeof(fb_handlers[i].display_name))||!fp_text(d,"APP ID",fb_handlers[i].app_id,sizeof(fb_handlers[i].app_id)))return false;}fp_button(d,FP_PREV,"Previous handlers");fp_button(d,FP_NEXT,"Next handlers");return true;}
 if(fb_mode==FB_DELETE||fb_mode==FB_REPLACE){fp_node(d,RISC_SCENE_TEXT_NODE,0,"WARNING",fb_mode==FB_DELETE?"Deletion is permanent and cannot be undone.":"The destination already exists. Review both paths before replacing.");if(fb_mode==FB_REPLACE&&!fp_text(d,"SOURCE",fb_source_path,sizeof(fb_source_path)))return false;fp_button(d,FP_CANCEL,"Cancel");fp_button(d,FP_REVIEW,"Review confirmation");return true;}
 if(fb_mode==FB_DETAIL){char size[32];fb_size_text(fb_size,size,sizeof(size));fp_node(d,RISC_SCENE_TEXT_NODE,0,"SIZE",size);fp_node(d,RISC_SCENE_TEXT_NODE,0,"AUTHORITY","Read-only storage. Files cannot change the installed system.");fp_button(d,FP_TOGGLE,"Read preview");return true;}
 if(fb_mode==FB_PREVIEW){if(fb_preview_count>FB_PREVIEW_BYTES)return false;char line[72];snprintf(line,sizeof(line),"%s / byte %llu",fb_hex?"HEX":"TEXT",(unsigned long long)fb_offset);fp_node(d,RISC_SCENE_TEXT_NODE,0,"PREVIEW",line);
  for(unsigned pos=0;pos<fb_preview_count;){unsigned n=0,limit=fb_hex?16:64;for(unsigned col=0;col<limit&&pos<fb_preview_count;col++){unsigned ch=fb_preview[pos++];if(fb_hex){static const char digits[]="0123456789ABCDEF";line[n++]=digits[ch>>4];line[n++]=digits[ch&15];line[n++]=' ';}else line[n++]=(char)(ch>=32&&ch<=126?ch:ch=='\n'||ch=='\r'||ch=='\t'?' ':'.');}line[n]=0;fp_node(d,RISC_SCENE_TEXT_NODE,0,"",line);}
  fp_button(d,FP_PREV,"Previous bytes");fp_button(d,FP_TOGGLE,fb_hex?"Text":"Hex");fp_button(d,FP_NEXT,"Next bytes");return true;}
 return fb_mode==FB_NOTICE;
}

static void files_presenter_reset(void){
 fp_modal=fp_failed=fp_built=fp_valid=fp_ready=fp_consumed=false;fp_window=0;fp_sequence=0;
 memset(&fp_pending,0,sizeof(fp_pending));memset(&fp_committed,0,sizeof(fp_committed));
 memset(&fp_document,0,sizeof(fp_document));memset(&fp_pending_document,0,sizeof(fp_pending_document));
 fpeer_pair_generation=0;fsh_setup_display_generation=fsh_setup_pending_generation=0;
}
static bool files_presenter_build(risc_components_document_v1 *d,uint32_t revision){
 fp_built=false;fp_failed=false;
 if(!files_scene_alive()||!d||!revision)return false;
 if(fb_mode!=FB_DELETE&&fb_mode!=FB_REPLACE)fp_modal=false;
 if(!fp_valid||fb_mode!=fp_committed.mode||fb_storage_index!=fp_committed.storage||fb_page!=fp_committed.page||
    fb_handler_offset!=fp_committed.handler_offset||fpeer_mode!=fp_committed.peer||fsh_setup_screen!=fp_committed.setup)fp_window=0;
 unsigned n=fb_mode==FB_HANDLERS?(fb_handler_count>=fb_handler_offset?fb_handler_count-fb_handler_offset:0):
  (fb_mode==FB_SHARING&&fpeer_mode?files_scene_peer_count:fb_count);
 if(n&&fp_window>=n)fp_window=((n-1)/FP_WINDOW)*FP_WINDOW;
 if(!n)fp_window=0;
 if(!fp_identity_copy(&fp_pending)||!fp_build_document(d,revision)||fp_failed)return false;
 fp_pending_document=*d;fp_built=true;return true;
}
static bool files_presenter_commit(const risc_components_document_v1 *d){
 if(!files_scene_alive()||!d||!fp_built||memcmp(d,&fp_pending_document,sizeof(*d))||
    (fp_valid&&d->revision<=fp_document.revision))return false;
 fp_committed=fp_pending;fp_document=*d;fp_valid=true;fp_built=false;fp_ready=false;fp_consumed=false;
 /* Successful submission is not yet proof that pairing numbers were visible. */
 fpeer_pair_generation=0;fsh_setup_display_generation=fsh_setup_pending_generation=0;
 return true;
}
static bool files_presenter_frame_ready(uint32_t revision){
 if(!fp_current()||revision!=fp_document.revision)return false;
 fp_ready=true;
 if(fp_committed.mode==FB_SHARING&&fp_committed.peer&&fp_committed.peer_state==PEER_PAIRING){
  fpeer_pair_generation=fp_committed.peer_generation;fpeer_pair_number=fp_committed.peer_number;
 }
 if(fp_committed.mode==FB_SHARING&&fp_committed.setup&&fp_committed.setup_state==PORTABLE_FILE_SETUP_STATE_ACTIVE&&
    fp_committed.setup_provider==RISC_SETUP_PAIR_CONFIRM){
  fsh_setup_display_generation=fp_committed.setup_generation;fsh_setup_display_number=fp_committed.setup_number;
 }
 return true;
}
static bool fp_page(int direction){
 if(fb_mode==FB_SHARING&&fpeer_mode&&fpeer_status.state==PEER_DISCOVERING){
  if(direction<0){if(fp_window)fp_window-=FP_WINDOW;}
  else if(fp_window+FP_WINDOW<files_scene_peer_count)fp_window+=FP_WINDOW;
 }else if(fb_mode==FB_HANDLERS){
  unsigned n=fb_handler_count-fb_handler_offset;if(n>fb_page_rows)n=fb_page_rows;
  if(direction<0&&fp_window)fp_window-=FP_WINDOW;
  else if(direction>0&&fp_window+FP_WINDOW<n)fp_window+=FP_WINDOW;
  else {fb_choice=direction<0?0:(n?n-1:0);fp_window=0;fbx_move(direction);}
 }else if(fb_mode==FB_LIST||fb_mode==FB_DESTINATION){
  if(direction<0&&fp_window)fp_window-=FP_WINDOW;
  else if(direction>0&&fp_window+FP_WINDOW<fb_count)fp_window+=FP_WINDOW;
  else {fp_window=0;if(direction<0)fb_page_previous();else fb_page_next();}
 }else if(fb_mode==FB_PREVIEW)fb_preview_page(direction);
 else return false;
 fb_dirty=true;return true;
}
static bool fp_action(const risc_scene_event_v1 *e){
 if(!files_scene_alive()||!e||e->struct_size!=sizeof(*e)||e->kind!=RISC_SCENE_ACTION_EVENT||
    !e->sequence||e->sequence<=fp_sequence)return false;
 fp_sequence=e->sequence;
 if(!fp_current()||e->document_revision!=fp_document.revision||e->value)return false;
 bool found=e->node==0&&e->action==fp_document.routes[0].back_action;
 for(unsigned i=0;!found&&i<fp_document.node_count;i++){
  const risc_scene_node_v1*n=&fp_document.nodes[i];
  found=n->id==e->node&&n->action==e->action&&n->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN));
 }
 if(!found)return false;
 unsigned a=e->action;
 if(fp_modal){
  if(a==FP_BACK||a==FP_CANCEL){fp_modal=false;fb_choice=0;fb_dirty=true;return true;}
  if(a!=FP_CONFIRM||!fp_ready)return false;
  fp_modal=false;fb_choice=1;fbx_activate();fb_dirty=true;return true;
 }
 if(a==FP_BACK){fp_window=0;return (void)fb_back(),true;}
 if(fsource_busy())return false;
 if(a>=FP_PEER&&a<FP_PEER+32){
  unsigned i=a-FP_PEER;
  if(fb_mode!=FB_SHARING||!fpeer_mode||fpeer_status.state!=PEER_DISCOVERING||i>=files_scene_peer_count||
     i<fp_committed.window||i>=fp_committed.window+FP_WINDOW)return false;
  portable_peer_row current={0};
  bool got=portable_peer_row_get(fpeer,i,&current);
  if(!files_scene_alive()||!got||memcmp(&current,&fp_committed.peers[i],sizeof(current)))return false;
  fpeer_choice=i;fpeer_action();return true;
 }
 if(a>=FP_HANDLER&&a<FP_HANDLER+FB_MAX_PAGE_ROWS){fb_choice=a-FP_HANDLER;fbx_dispatch(fb_choice);return true;}
 if(a>=FP_OPERATION&&a<FP_OPERATION+FBX_ACTION_COUNT){fb_choice=a-FP_OPERATION;fbx_activate();fb_dirty=true;return true;}
 if(a>=FP_OPTION&&a<FP_OPTION+fb_option_count()){fb_option(a-FP_OPTION);return true;}
 if(a>=FP_ROW&&a<FP_ROW+FB_MAX_PAGE_ROWS){
  fb_choice=a-FP_ROW;
  if(fb_mode==FB_DESTINATION)fbx_activate();else fb_activate_file(fb_choice);
  return true;
 }
 switch(a){
 case FP_OPTIONS:fb_list_choice=fb_choice;fb_mode=FB_OPTIONS;fb_choice=0;break;
 case FP_PREV:return fp_page(-1);
 case FP_NEXT:return fp_page(1);
 case FP_TOGGLE:fb_activate();break;
 case FP_UP:if(fb_parent(fb_path)){fb_query[0]=0;(void)fb_load(0);}break;
 case FP_HERE:fbx_commit_destination();break;
 case FP_VOLUME:if(fbx_swap()){strcpy(fb_path,"/");fb_query[0]=0;(void)fb_load(0);}break;
 case FP_REVIEW:if(fb_mode!=FB_DELETE&&fb_mode!=FB_REPLACE)return false;fp_modal=true;break;
 case FP_CANCEL:if(fb_mode==FB_DESTINATION)fbx_cancel();else (void)fb_back();break;
 case FP_RETRY_EDITOR:(void)fb_text_begin(fb_mode,fb_mode==FB_FILTER?"Find file":"Edit name");break;
 case FP_SHARE:fsh_action();break;
 case FP_TRANSPORT:if(fsh_phase!=FSH_OFF)return false;fsh_direct=!fsh_direct;break;
 case FP_SETUP:fsh_setup_start();break;
 case FP_SETUP_REJECT:case FP_SETUP_MATCH:
  if(!fp_ready||!fp_committed.setup_generation)return false;
  fsh_setup_choice(a==FP_SETUP_MATCH);fp_ready=false;break;
 case FP_PEER_REJECT:case FP_PEER_MATCH:
  if(!fp_ready||!fp_committed.peer_generation)return false;
  fpeer_choice=a==FP_PEER_MATCH?1:0;fpeer_action();fp_ready=false;break;
 case FP_PEER_STOP:
  fpeer_choice=fpeer_status.state==PEER_PAIRING?2:fpeer_status.state==PEER_DISCOVERING?fpeer_status.count:0;fpeer_action();break;
 default:return false;
 }
 fb_dirty=true;return true;
}

static bool files_presenter_action(const risc_scene_event_v1 *e){
 bool accepted=fp_action(e);if(accepted)fp_consumed=true;return accepted;
}
