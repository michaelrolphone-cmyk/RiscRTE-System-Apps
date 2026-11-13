/* Deterministic in-memory volume endpoints, driven only through native scene
 * navigation. The production root, filesystem algorithms and presenter run. */
static const char storage_name[]="complete original filename with spaces.txt";
static const char storage_path[]="/complete original filename with spaces.txt";
static const char storage_content[]="Line one\nLine two\n";
static bool storage_text_accept,storage_new_folder;static unsigned storage_renames,storage_mkdirs;
static bool storage_text_cancel,storage_text_seen;static unsigned storage_text_frame;
static bool storage_handler_case,storage_handler_paged;static unsigned storage_handler_requests;
static bool storage_copy,storage_move,storage_destination,storage_listing_destination;
static unsigned storage_write_at,storage_writes,storage_moves;
static bool storage_exists=true,storage_directory_open,storage_file_open,storage_reader_open,storage_cancel,storage_preview;
static unsigned storage_directory_at,storage_read_at,storage_removes,storage_reads,storage_stage,storage_frames;
static bool storage_ready(void*c){(void)c;return true;}
static bool storage_stat(void*c,const char*p,uint64_t*n,bool*d){assert(native_alive);(void)c;if(!strcmp(p,"/")){*n=0;*d=true;return true;}if((storage_copy||storage_move)&&!strcmp(p,"/destination")){*n=0;*d=true;return true;}if(!storage_exists||strcmp(p,storage_path))return false;*n=sizeof(storage_content)-1;*d=false;if(storage_source_fault==3&&fsource_state==FSOURCE_LOCAL)native_alive=false;return true;}
static risc_storage_dir_t storage_dir(void*c,const char*p){(void)c;assert((!strcmp(p,"/")||!strcmp(p,"/destination"))&&!storage_directory_open);storage_listing_destination=strcmp(p,"/")!=0;storage_directory_open=true;storage_directory_at=0;return 1;}
static bool storage_next(void*c,risc_storage_dir_t h,risc_storage_dirent_v1*out){
 (void)c;assert(h==1&&storage_directory_open);memset(out,0,sizeof(*out));unsigned i=storage_directory_at++;
 if(storage_listing_destination)return false;
 bool folder=storage_copy||storage_move;
 if(folder&&i==0){strcpy(out->name,"destination");out->is_directory=1;return true;}
 if(!storage_exists||i!=(folder?1u:0u))return false;
 strcpy(out->name,storage_name);out->size=sizeof(storage_content)-1;return true;
}
static void storage_dir_close(void*c,risc_storage_dir_t h){(void)c;assert(h==1&&storage_directory_open);storage_directory_open=false;}
static risc_storage_file_t storage_open(void*c,const char*p,uint64_t*n){(void)c;assert(storage_exists&&!strcmp(p,storage_path)&&!storage_reader_open);storage_reader_open=true;storage_read_at=0;*n=sizeof(storage_content)-1;return 2;}
static size_t storage_read(void*c,risc_storage_file_t h,void*b,size_t n){assert(native_alive);(void)c;assert(h==2&&storage_reader_open);size_t left=sizeof(storage_content)-1-storage_read_at;if(n>left)n=left;memcpy(b,storage_content+storage_read_at,n);storage_read_at+=(unsigned)n;++storage_reads;if(storage_source_fault==2)native_alive=false;return n;}
static bool storage_close(void*c,risc_storage_file_t h,bool commit){assert(native_alive);(void)c;(void)commit;assert((h==2&&storage_reader_open)||(h==3&&storage_file_open));if(h==2){storage_reader_open=false;return true;}if(h==3){assert(commit&&storage_write_at==sizeof(storage_content)-1);storage_destination=true;}storage_file_open=false;return true;}
static bool storage_remove(void*c,const char*p){(void)c;assert(storage_exists&&!strcmp(p,storage_path)&&!storage_file_open&&!storage_directory_open);assert(!storage_cancel&&!storage_preview);if(storage_move)assert(storage_destination);++storage_removes;storage_exists=false;return true;}
static risc_storage_file_t storage_create(void*c,const char*p){(void)c;char wanted[128];snprintf(wanted,sizeof(wanted),"/destination/%s",storage_name);assert(!strcmp(p,wanted)&&!storage_destination);storage_file_open=true;storage_write_at=0;return 3;}
static size_t storage_write(void*c,risc_storage_file_t h,const void*b,size_t n){(void)c;assert(h==3&&storage_file_open&&storage_write_at+n<=sizeof(storage_content)-1);assert(!memcmp(b,storage_content+storage_write_at,n));storage_write_at+=(unsigned)n;++storage_writes;return n;}
static bool storage_rename(void*c,const char*from,const char*to){(void)c;char wanted[128];snprintf(wanted,sizeof(wanted),"/destination/%s",storage_name);if(storage_text_accept){assert(!strcmp(from,storage_path)&&!strcmp(to,"/new.txt")&&storage_exists&&!storage_reader_open&&!storage_file_open);storage_exists=false;storage_destination=true;++storage_renames;return true;}assert(storage_move&&storage_exists&&!strcmp(from,storage_path)&&!strcmp(to,wanted)&&!storage_destination&&!storage_reader_open&&!storage_file_open);storage_exists=false;storage_destination=true;++storage_moves;return true;}
static bool storage_mkdir(void*c,const char*p){(void)c;assert(storage_new_folder&&!strcmp(p,"/new.txt")&&!storage_file_open&&!storage_reader_open);++storage_mkdirs;return true;}
#include "RiscUsbHidV1.h"
static bool storage_usb;static unsigned storage_usb_index,storage_usb_edge;static uint64_t storage_usb_sequence;
static uint64_t storage_usb_subscribe(void*c,uint64_t device){(void)c;assert(!device&&!storage_usb);storage_usb=true;return 7;}
static bool storage_usb_unsubscribe(void*c,uint64_t token){(void)c;assert(token==7&&storage_usb);storage_usb=false;return true;}
static bool storage_usb_poll(void*c,size_t reports){(void)c;assert(reports==8);storage_usb_edge=0;return true;}
static int32_t storage_usb_next(void*c,uint64_t token,risc_usb_keyboard_event_v1*out){
 (void)c;assert(token==7&&storage_usb);static const uint8_t keys[]={0x11,0x08,0x1a,0x37,0x17,0x1b,0x17,0x28};
 unsigned erase=storage_new_folder?0:sizeof(storage_name)-1;
 if(!storage_text_seen||submits<=storage_text_frame||storage_usb_index>=erase+sizeof(keys)||storage_usb_edge>=2)return 0;
 uint8_t usage=storage_usb_index<erase?0x2a:keys[storage_usb_index-erase];*out=(risc_usb_keyboard_event_v1){.sequence=++storage_usb_sequence,.device=1,.kind=storage_usb_edge?4:3,.usage=usage};if(++storage_usb_edge==2)++storage_usb_index;return 1;
}
static bool storage_usb_snapshot(void*c,risc_usb_keyboard_state_v1*out,size_t*n){(void)c;assert(*n);*out=(risc_usb_keyboard_state_v1){.device=1,.connected=1};*n=1;return true;}
static const risc_usb_keyboard_api_v1 storage_keyboard={1,sizeof(storage_keyboard),NULL,storage_usb_subscribe,storage_usb_unsubscribe,storage_usb_poll,storage_usb_next,storage_usb_snapshot};
static bool storage_error(void*c,char*out,size_t n){(void)c;assert(n);out[0]=0;return true;}
static uint32_t storage_handler_count(const char*p){char wanted[128];snprintf(wanted,sizeof(wanted),"/sd%s",storage_path);assert(!strcmp(p,wanted));return storage_handler_paged?12:1;}
static bool storage_handler_get(const char*p,uint32_t i,t5_file_handler_t*out){assert(i<storage_handler_count(p));memset(out,0,sizeof(*out));out->kind=T5_FILE_HANDLER_APP;snprintf(out->app_id,sizeof(out->app_id),"reader%02u",i);snprintf(out->display_name,sizeof(out->display_name),"Handler %02u",i);return true;}
static bool storage_handler_request(const char*p,const char*id,uint64_t cookie){assert(storage_handler_count(p)&&!strcmp(id,storage_handler_paged?"reader09":"reader00")&&cookie==UINT64_C(0x4642524f57534552));assert(!files_ui.token&&!held&&!presenting&&!storage_reader_open&&!storage_file_open&&!storage_directory_open&&!storage_removes&&!storage_writes);++storage_handler_requests;assert(storage_handler_requests==1);stage=3;return true;}
static bool storage_handler_result(int32_t*error,uint64_t*cookie){(void)error;(void)cookie;return false;}
static const t5_file_open_api_v1 storage_handlers={.api_version=1,.struct_size=sizeof(storage_handlers),.handler_count=storage_handler_count,.handler_get=storage_handler_get,.open_request=storage_handler_request,.open_take_result=storage_handler_result};
static unsigned storage_source_begins,storage_source_writes,storage_source_finishes,storage_source_requests,storage_source_bytes,storage_source_discards;
static uint32_t storage_source_count(const char*n){assert(!strcmp(n,storage_name));return 1;}
static bool storage_source_get(const char*n,uint32_t i,t5_file_handler_t*out){assert(storage_source_count(n)&&!i);memset(out,0,sizeof(*out));out->kind=T5_FILE_HANDLER_APP;strcpy(out->app_id,"reader00");strcpy(out->display_name,"Reader");return true;}
static bool storage_source_supported(const char*id){assert(!strcmp(id,"reader00"));return true;}
static int32_t storage_source_begin(const char*n,uint64_t size,uint64_t*token){assert(storage_source_count(n)&&size==sizeof(storage_content)-1&&storage_reader_open);++storage_source_begins;if(storage_source_busy_case&&storage_source_begins==1)return RISC_FILE_SOURCE_BUSY;assert(storage_source_begins==(storage_source_busy_case?2u:1u));*token=17;if(storage_source_fault==6)return RISC_FILE_SOURCE_BUSY;if(storage_source_fault==1)native_alive=false;return 0;}
static int32_t storage_source_write(uint64_t token,const void*b,uint32_t n,uint32_t*written){assert(native_alive&&token==17&&n&&storage_reader_open);assert(!memcmp(b,storage_content+storage_source_bytes,n));*written=storage_source_identity_case?n:n>3?3:n;storage_source_bytes+=*written;++storage_source_writes;if(storage_source_identity_case)native_alive=false;if(storage_source_fault==7)return RISC_FILE_SOURCE_BUSY;return 0;}
static int32_t storage_source_finish(uint64_t token){assert(token==17&&!storage_reader_open&&storage_source_bytes==sizeof(storage_content)-1);++storage_source_finishes;if(storage_source_busy_case&&storage_source_finishes==1)return RISC_FILE_SOURCE_BUSY;assert(storage_source_finishes==(storage_source_busy_case?2u:1u));if(storage_source_fault==4)native_alive=false;if(storage_source_fault==8)return 42;return 0;}
static int32_t storage_source_discard(uint64_t token){assert(token==17&&storage_source_cancel_case&&!storage_reader_open&&!storage_source_requests);assert(++storage_source_discards==1);return 0;}
static bool storage_source_request(uint64_t token,const char*id,uint64_t cookie){assert(token==17&&storage_source_supported(id)&&cookie==UINT64_C(0x4642524f57534552));assert(storage_source_finishes==(storage_source_busy_case?2u:1u)&&!files_ui.token&&!held&&!presenting&&!storage_reader_open&&!storage_directory_open&&!storage_handler_requests);assert(++storage_source_requests==1);stage=3;if(storage_source_fault==5)native_alive=false;return true;}
static const risc_file_open_source_api_v1 storage_source={.api_version=1,.struct_size=sizeof(storage_source),.handler_count=storage_source_count,.handler_get=storage_source_get,.handler_supported=storage_source_supported,.begin=storage_source_begin,.write=storage_source_write,.finish=storage_source_finish,.discard=storage_source_discard,.open_request=storage_source_request,.take_result=storage_handler_result};
static void storage_action(unsigned action){
 unsigned target=0;for(unsigned i=0;i<files_ui.committed.node_count;i++)if(files_ui.committed.nodes[i].action==action){target=files_ui.committed.nodes[i].id;break;}
 if(!target)return;
 risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;assert(api->snapshot(api->context,files_ui.token,&n,&flags)==0);
 if(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return;
 nav_pressed=n.focus[n.depth-1]==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;
}
static void storage_input(void){
 if((storage_text_cancel||storage_text_accept||storage_new_folder)&&fb_text_active){if(!storage_text_seen){storage_text_seen=true;storage_text_frame=submits;}storage_stage=2;if(storage_text_cancel&&submits>storage_text_frame&&!held&&!presenting)nav_pressed=RISC_NAV_BACK;return;}
 if(!files_ui.token||held||presenting||!submits||fb_dirty)return;
 if(storage_new_folder){
  if(storage_stage==0){if(fb_mode==FB_LIST){storage_action(FP_OPTIONS);return;}assert(fb_mode==FB_OPTIONS);storage_stage=1;}
  if(storage_stage==1){storage_action(FP_OPTION+8);return;}
  assert(storage_mkdirs==1&&!fb_text_active&&!storage_renames&&!storage_writes&&!storage_removes&&storage_exists);nav_pressed=RISC_NAV_HOME;stage=3;return;
 }
 if(storage_stage==0){if(fb_mode==FB_LIST){unsigned row=0;while(row<fb_count&&strcmp(fb_rows[row].name,storage_name))++row;assert(row<fb_count);storage_action(FP_ROW+row);return;}assert(fb_mode==FB_ACTIONS);assert(!strcmp(fb_selected.name,storage_name)&&!strcmp(fb_file_path,storage_path));storage_stage=1;}
 if(storage_stage==1){if(fb_mode==FB_ACTIONS){storage_action(FP_OPERATION+((storage_handler_case||storage_source_case)?(storage_handler_paged?1:0):storage_preview?2:(storage_text_cancel||storage_text_accept)?3:storage_copy?5:storage_move?4:6));return;}storage_stage=2;}
 if(storage_source_case){if(storage_source_cancel_case){if(fsource_busy()){nav_pressed=RISC_NAV_BACK;return;}assert(storage_source_discards==1&&!storage_source_requests&&!storage_reader_open);nav_pressed=RISC_NAV_HOME;stage=3;return;}assert(fsource_busy()||storage_source_requests);return;}
 if(storage_handler_case){if(storage_handler_requests)return;assert(storage_handler_paged&&fb_mode==FB_HANDLERS);unsigned action=0;for(unsigned i=0;i<files_ui.committed.node_count;i++){risc_scene_node_v1*n=&files_ui.committed.nodes[i];if(n->action>=FP_HANDLER&&!strcmp(n->label,"Handler 09"))action=n->action;}storage_action(action?action:FP_NEXT);return;}
 if(storage_text_accept){assert(storage_text_seen&&!fb_text_active&&storage_renames==1&&!storage_exists&&storage_destination&&!storage_moves&&!storage_writes&&!storage_removes);nav_pressed=RISC_NAV_HOME;stage=3;return;}
 if(storage_text_cancel){assert(storage_text_seen&&!fb_text_active&&fb_mode==FB_ACTIONS&&!strcmp(fb_editor,storage_name)&&!storage_moves&&!storage_writes&&!storage_removes);nav_pressed=RISC_NAV_HOME;stage=3;return;}
 if(storage_copy||storage_move){
  if(storage_stage==2){assert(fb_mode==FB_DESTINATION);if(strcmp(fb_path,"/destination")){storage_action(FP_ROW);return;}storage_action(FP_HERE);if(nav_pressed==RISC_NAV_CONFIRM)storage_stage=3;return;}
  if(!storage_destination)return;
  assert(!storage_removes&&storage_writes==(storage_move?0u:1u)&&storage_moves==(storage_move?1u:0u));nav_pressed=RISC_NAV_HOME;stage=3;return;
 }
 if(storage_preview){assert(storage_reads&&!memcmp(fb_preview,storage_content,sizeof(storage_content)-1));if(fb_mode==FB_PREVIEW){nav_pressed=RISC_NAV_BACK;return;}nav_pressed=RISC_NAV_HOME;stage=3;return;}
 if(storage_stage==2){assert(!storage_removes);if(!fp_modal){storage_action(FP_REVIEW);return;}storage_stage=3;storage_frames=submits;}
 if(storage_stage==3){if(fp_modal){storage_action(storage_cancel?FP_CANCEL:FP_CONFIRM);return;}storage_stage=4;}
 assert(storage_cancel?!storage_removes:storage_removes==1);assert(submits>=storage_frames);nav_pressed=RISC_NAV_HOME;stage=3;
}
