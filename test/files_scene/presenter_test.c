/* Semantic presenter endpoint doubles; not production filesystem/radio proof. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "RiscStorageVolumeV1.h"
#include "T5FileOpenApi.h"
#include "PortablePeerFiles.h"
#define FB_PATH_CAP 512u
#define FB_MAX_PAGE_ROWS 16u
#define FB_PREVIEW_BYTES 192u
enum {FB_LIST,FB_OPTIONS,FB_FILTER,FB_DETAIL,FB_PREVIEW,FB_ACTIONS,FB_RENAME,FB_DESTINATION,FB_DELETE,FB_HANDLERS,FB_NOTICE,FB_REPLACE,FB_SHARING};
enum {FSH_OFF,FSH_ACTIVE,FSH_STOPPING};
enum {PORTABLE_FILE_SETUP_STATE_ACTIVE=3,RISC_SETUP_PAIR_CONFIRM=4};
enum {PORTABLE_FILE_SHARING_LISTENING=1,PORTABLE_FILE_SHARING_SERVING=2};
static bool live=true,source_busy,fb_moving,fb_dirty,fb_hidden,fb_hex,fpeer_mode,fsh_setup_screen,fsh_direct,ready,changed_peer,invalidate_peer;
static unsigned fb_mode,fb_storage_index,fb_page,fb_count,fb_page_rows=16,fb_handler_offset,fb_handler_count,fb_list_choice,fb_choice,fb_matched;
static unsigned fb_preview_count,files_scene_peer_count,fpeer_choice,fsh_phase;
static uint64_t fb_size,fb_offset,fsh_now;
static uint32_t fpeer_pair_generation,fpeer_pair_number,fsh_setup_display_generation,fsh_setup_display_number,fsh_setup_pending_generation;
static char fb_path[512],fb_file_path[512],fb_source_path[512],fb_handler_path[512],fb_query[64],fb_editor[128],fb_status[96],fb_label[64],fsource_name[512],fsh_url[512],fsh_message[96];
static unsigned char fb_preview[192];
static risc_storage_dirent_v1 fb_rows[16],fb_selected;
static t5_file_handler_t fb_handlers[16];
static portable_peer_row files_scene_peer_rows[32];
static portable_peer_status fpeer_status;
static portable_peer_files *fpeer;
static struct {unsigned state,provider_state;uint32_t pairing_generation,pairing_number;} fsh_setup_status;
static struct {unsigned state,completed_requests;uint64_t expires_ms;char username[64],password[64];} fsh_status;
static const char *fb_options[]={"Find","Refresh","Hidden","Root","Selected","Volume","Share","Nearby","New folder","Disconnect"};
static const char *fbx_labels[]={"Open","Open with","Preview","Rename","Move","Copy","Delete"};
#define FBX_ACTION_COUNT 7u
static unsigned effects,last,tests;
static bool files_scene_alive(void){return live;}
static bool fsource_busy(void){return source_busy;}
static void fb_size_text(uint64_t n,char *s,size_t z){(void)n;snprintf(s,z,"size");}
static const char *fsh_setup_label(void){return "Compare both devices";}
static unsigned fb_option_count(void){return 10;}
static void hit(unsigned n){effects++;last=n;}
static bool fb_back(void){hit(1);return false;}
static void fb_option(unsigned n){hit(100+n);}
static void fb_activate_file(unsigned n){hit(200+n);}
static void fbx_activate(void){hit(300+fb_choice);}
static void fbx_dispatch(unsigned n){hit(400+n);}
static void fb_activate(void){hit(500);}
static void fb_page_previous(void){hit(600);if(fb_page)fb_page--;}
static void fb_page_next(void){hit(601);fb_page++;}
static void fbx_move(int n){hit(n<0?602:603);if(n>0)fb_handler_offset+=fb_page_rows;else if(fb_handler_offset>=fb_page_rows)fb_handler_offset-=fb_page_rows;}
static void fb_preview_page(int n){hit(n<0?604:605);}
static bool fb_parent(char *s){hit(606);strcpy(s,"/");return true;}
static bool fb_load(unsigned n){(void)n;hit(607);return true;}
static void fbx_commit_destination(void){hit(608);}
static bool fbx_swap(void){hit(609);return true;}
static void fbx_cancel(void){hit(610);}
static bool fb_text_begin(unsigned m,const char*s){(void)m;(void)s;hit(611);return true;}
static void fsh_action(void){hit(612);}
static void fsh_setup_start(void){hit(613);}
static void fsh_setup_choice(bool a){assert(ready&&fsh_setup_display_generation==fsh_setup_status.pairing_generation&&fsh_setup_display_number==fsh_setup_status.pairing_number);hit(a?615:614);fsh_setup_display_generation=0;}
static void fpeer_action(void){if(fpeer_status.state==PEER_PAIRING&&fpeer_choice<2)assert(ready&&fpeer_pair_generation==fpeer_status.pairing_generation&&fpeer_pair_number==fpeer_status.pairing_number);hit(620+fpeer_choice);}
bool portable_peer_row_get(const portable_peer_files*p,unsigned i,portable_peer_row*out){(void)p;if(i>=files_scene_peer_count)return false;*out=files_scene_peer_rows[i];if(invalidate_peer)live=false;if(changed_peer)out->peer_id++;return true;}
#include "../../Apps/files_scene_presenter.inc"
static uint32_t revision;
static uint64_t sequence;
static risc_components_document_v1 document;
static void reset(void){
 files_presenter_reset();live=true;source_busy=false;ready=false;changed_peer=false;invalidate_peer=false;
 fb_mode=FB_LIST;fb_storage_index=fb_page=fb_count=fb_handler_offset=fb_handler_count=fb_list_choice=fb_choice=fb_matched=0;fb_page_rows=16;
 fb_moving=fb_dirty=fb_hidden=fb_hex=fpeer_mode=fsh_setup_screen=fsh_direct=false;
 files_scene_peer_count=fb_preview_count=fsh_phase=0;fb_size=fb_offset=fsh_now=0;
 memset(fb_rows,0,sizeof(fb_rows));memset(&fb_selected,0,sizeof(fb_selected));memset(fb_handlers,0,sizeof(fb_handlers));memset(files_scene_peer_rows,0,sizeof(files_scene_peer_rows));memset(&fpeer_status,0,sizeof(fpeer_status));memset(&fsh_setup_status,0,sizeof(fsh_setup_status));memset(&fsh_status,0,sizeof(fsh_status));
 memset(fb_path,0,sizeof(fb_path));strcpy(fb_path,"/");memset(fb_file_path,0,sizeof(fb_file_path));memset(fb_source_path,0,sizeof(fb_source_path));memset(fb_handler_path,0,sizeof(fb_handler_path));
 fb_query[0]=fb_editor[0]=fb_status[0]=fb_label[0]=fsource_name[0]=fsh_url[0]=fsh_message[0]=0;
 effects=last=0;revision=0;sequence=0;
}
static void publish(void){unsigned before=effects;assert(files_presenter_build(&document,++revision));assert(effects==before);assert(document.node_count<=96);assert(files_presenter_commit(&document));}
static risc_scene_event_v1 event(unsigned a){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==a)return(risc_scene_event_v1){sizeof(risc_scene_event_v1),RISC_SCENE_ACTION_EVENT,document.revision,document.nodes[i].id,a,0,++sequence};assert(!"action missing");return(risc_scene_event_v1){0};}
static bool action(unsigned a){risc_scene_event_v1 e=event(a);return files_presenter_action(&e);}
static void frame(void){ready=true;assert(files_presenter_frame_ready(document.revision));}
static void test_routes(void){
 for(unsigned m=0;m<=FB_SHARING;m++){reset();fb_mode=m;strcpy(fb_selected.name,"item");publish();assert(document.route_count==1&&document.routes[0].back_action==FP_BACK);}
 tests++;
}
static void test_commit(void){
 reset();fb_count=1;strcpy(fb_rows[0].name,"before");assert(files_presenter_build(&document,++revision));strcpy(fb_rows[0].name,"after");assert(files_presenter_commit(&document));assert(!action(FP_ROW)&&!effects);
 reset();assert(files_presenter_build(&document,++revision));document.nodes[0].action=999;assert(!files_presenter_commit(&document));tests++;
}
static void test_events(void){
 reset();publish();risc_scene_event_v1 e=event(FP_OPTIONS),good=e;
 e.document_revision++;assert(!files_presenter_action(&e));good.sequence=++sequence;good.node=999;assert(!files_presenter_action(&good));good=event(FP_OPTIONS);good.kind=RISC_SCENE_VALUE_EVENT;assert(!files_presenter_action(&good));
 good=event(FP_OPTIONS);assert(files_presenter_action(&good));assert(fb_mode==FB_OPTIONS);assert(!files_presenter_action(&good));good.sequence=++sequence;assert(!files_presenter_action(&good));tests++;
}
static void test_path_stale(void){
 reset();fb_count=1;strcpy(fb_rows[0].name,"entry");publish();strcpy(fb_path,"/changed");assert(!action(FP_ROW)&&!effects);tests++;
}
static void test_pages(void){
 reset();fb_count=16;for(unsigned i=0;i<16;i++)snprintf(fb_rows[i].name,sizeof(fb_rows[i].name),"row%u",i);publish();
 assert(action(FP_NEXT)&&!effects);publish();assert(action(FP_ROW+6)&&last==206);publish();assert(action(FP_NEXT));publish();assert(action(FP_ROW+12)&&last==212);tests++;
}
static void test_peers(void){
 reset();fb_mode=FB_SHARING;fpeer_mode=true;fpeer_status.state=PEER_DISCOVERING;fpeer_status.count=files_scene_peer_count=32;
 for(unsigned i=0;i<32;i++){files_scene_peer_rows[i].peer_id=i+10;memset(files_scene_peer_rows[i].name,'A',127);files_scene_peer_rows[i].name[127]=0;memset(files_scene_peer_rows[i].path,'P',512);files_scene_peer_rows[i].path[512]=0;}
 for(unsigned first=0;first<32;first+=6){publish();assert(document.node_count<=96);if(first==30){assert(action(FP_PEER+31)&&last==651);break;}assert(action(FP_NEXT));}
 reset();fb_mode=FB_SHARING;fpeer_mode=true;fpeer_status.state=PEER_DISCOVERING;files_scene_peer_count=fpeer_status.count=1;files_scene_peer_rows[0].peer_id=1;publish();changed_peer=true;assert(!action(FP_PEER)&&!effects);changed_peer=false;invalidate_peer=true;assert(!action(FP_PEER)&&!effects&&!live);tests++;
}
static void test_pair(void){
 for(unsigned setup=0;setup<2;setup++){
 reset();fb_mode=FB_SHARING;fsh_setup_screen=setup;fpeer_mode=!setup;
 fpeer_status.state=PEER_PAIRING;fpeer_status.pairing_generation=7;fpeer_status.pairing_number=123456;
 fsh_setup_status.state=PORTABLE_FILE_SETUP_STATE_ACTIVE;fsh_setup_status.provider_state=RISC_SETUP_PAIR_CONFIRM;fsh_setup_status.pairing_generation=7;fsh_setup_status.pairing_number=123456;
 publish();assert(!action(setup?FP_SETUP_MATCH:FP_PEER_MATCH)&&!effects);frame();assert(action(setup?FP_SETUP_MATCH:FP_PEER_MATCH)&&effects==1);
 publish();if(setup)fsh_setup_status.pairing_number++;else fpeer_status.pairing_number++;
 assert(!files_presenter_frame_ready(document.revision));assert(!action(setup?FP_SETUP_MATCH:FP_PEER_MATCH));}
 tests++;
}
static void test_modal(void){
 for(unsigned m=FB_DELETE;m<=FB_REPLACE;m+=FB_REPLACE-FB_DELETE){
 reset();fb_mode=m;strcpy(fb_selected.name,"selected");strcpy(fb_file_path,"/selected");publish();assert(action(FP_REVIEW)&&!effects);publish();assert(document.flags&RISC_COMPONENTS_CONFIRM);assert(document.cancel_action==FP_CANCEL);
 unsigned texts=0;for(unsigned i=0;i<document.node_count;i++)texts+=document.nodes[i].kind==RISC_SCENE_TEXT_NODE;assert(texts==1);
 assert(!action(FP_CONFIRM));frame();assert(action(FP_CONFIRM)&&last==301); }
 reset();fb_mode=FB_DELETE;publish();assert(action(FP_REVIEW));publish();assert(action(FP_CANCEL)&&!effects&&!fp_modal);tests++;
}
static void test_handlers(void){
 reset();fb_mode=FB_HANDLERS;fb_handler_count=32;for(unsigned i=0;i<16;i++){snprintf(fb_handlers[i].display_name,sizeof(fb_handlers[i].display_name),"handler%u",i);snprintf(fb_handlers[i].app_id,sizeof(fb_handlers[i].app_id),"app%u",i);}
 publish();assert(action(FP_NEXT));publish();assert(action(FP_HANDLER+6)&&last==406);publish();strcpy(fb_handlers[6].app_id,"changed");assert(!action(FP_HANDLER+6));tests++;
}
static void test_capacity(void){
 reset();memset(fb_path,'X',sizeof(fb_path));assert(!files_presenter_build(&document,1));
 reset();risc_components_document_v1 d={0};fp_failed=false;for(unsigned i=0;i<97;i++)fp_button(&d,1,"x");assert(fp_failed&&d.node_count==96);tests++;
}
static void test_back_and_terminal(void){
 reset();publish();risc_scene_event_v1 e={sizeof(e),RISC_SCENE_ACTION_EVENT,document.revision,0,FP_BACK,0,++sequence};assert(files_presenter_action(&e)&&last==1);
 reset();publish();live=false;assert(!action(FP_OPTIONS)&&!effects);assert(!files_presenter_frame_ready(document.revision));tests++;
}
int main(void){test_routes();test_commit();test_events();test_path_stale();test_pages();test_peers();test_pair();test_modal();test_handlers();test_capacity();test_back_and_terminal();printf("Files semantic presenter %u groups PASS (endpoint doubles)\n",tests);return 0;}
