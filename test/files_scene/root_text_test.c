/* Actual domain queue bridge and original extended text client, endpoint doubles.
 * Deliberately not a full root/background/provider qualification. */
#include <assert.h>
#include "../../Apps/files_scene.c"
bool portable_peer_row_get(const portable_peer_files *p,unsigned i,portable_peer_row *out){(void)p;(void)i;(void)out;assert(0&&"text test must not enumerate peers");return false;}
static risc_runtime_api_v1 runtime;
static uint32_t test_clock,checkpoint_reason;
static unsigned checkpoints;
bool files_scene_background_capture_active(void){return false;}
uint32_t files_scene_background_effects(void){return 0;}
bool files_scene_background_effect_done(unsigned effect,unsigned outcome){(void)effect;(void)outcome;assert(0);return false;}
bool files_scene_background_idle_timeout(uint32_t*out){*out=5000;return true;}
static bool monotonic(uint32_t*out){*out=test_clock;return true;}
static int root_checkpoint(uint64_t i,const risc_resident_request_v1*q,risc_resident_reply_v1*r){assert(i==5&&!files_ui.token&&!fb_text_active&&!q->flags&&!q->reserved);assert(q->reason!=RISC_RESIDENT_CHECKPOINT_POLL);checkpoint_reason=q->reason;checkpoints++;r->flags=0;return 0;}
static bool root_resident_bind(risc_resident_client_v1*c){*c=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*c),.invocation=5,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=root_checkpoint};return true;}
static unsigned handler_acquire_mode;
static t5_file_open_api_v1 test_handler;
static unsigned root_acquire_mode,root_attempts,root_ready_calls,source_acquire_mode;
static risc_storage_volume_api_v1 generic_volume;
static bool root_ready(void*p){(void)p;root_ready_calls++;return false;}
static risc_storage_file_t root_open(void*p,const char*n,uint64_t*z){(void)p;(void)n;(void)z;assert(0);return 0;}
static size_t root_read(void*p,risc_storage_file_t f,void*b,size_t n){(void)p;(void)f;(void)b;(void)n;assert(0);return 0;}
static bool root_close(void*p,risc_storage_file_t f,bool c){(void)p;(void)f;(void)c;assert(0);return false;}
static risc_text_entry_api_v1_extended keyboard;
static unsigned opened,closed,released;
static int open_result,close_result;
static bool mutate_on_open;
static char captured[128];
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
bool files_scene_background_pause(void){return true;}
static bool retained(void){return true;}
static bool get(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 if(handler_acquire_mode){assert(!strcmp(name,"file.open"));if(handler_acquire_mode==2)g->slot=1;if(handler_acquire_mode==3){*g=(risc_runtime_capability_v1){sizeof(*g),1,1,&test_handler};return true;}if(handler_acquire_mode==4)files_retained=true;return false;}
 if(source_acquire_mode){assert(!strcmp(name,RISC_FILE_OPEN_SOURCE_CAPABILITY));if(source_acquire_mode==2)g->generation=1;return false;}
 if(root_acquire_mode){
  assert(version==1&&!instance);root_attempts++;
  if(root_acquire_mode==3){g->slot=1;return false;}
  bool ok=root_acquire_mode==2||!strcmp(name,"storage.volume");
  if(ok)*g=(risc_runtime_capability_v1){sizeof(*g),1,1,&generic_volume};
  return ok;
 }
 assert(!strcmp(name,RISC_TEXT_ENTRY_CAPABILITY)&&version==1&&!instance);*g=(risc_runtime_capability_v1){sizeof(*g),1,1,&keyboard};return true;}
static bool put(risc_runtime_capability_v1 *g){released++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static int native_open(void*p,const risc_text_entry_request_v1*q,uint64_t*t){(void)p;(void)q;(void)t;assert(0);return -1;}
static int native_poll(void*p,uint64_t t,risc_text_entry_state_v1*q){(void)p;(void)t;(void)q;assert(0);return -1;}
static int native_close(void*p,uint64_t t){(void)p;assert(t==44);closed++;return close_result;}
static int extended_open(void*p,const risc_text_entry_request_v1_extended*q,uint64_t*t){(void)p;opened++;strcpy(captured,q->text);if(mutate_on_open){fb_mode=FB_NOTICE;fb_choice=99;strcpy(fb_editor,"callback mutation");}if(open_result==0)*t=44;return open_result;}
static int extended_poll(void*p,uint64_t t,risc_text_entry_state_v1_extended*q){(void)p;(void)t;(void)q;return RISC_TEXT_ENTRY_AGAIN;}
static void setup(void){
 files_text_suspended=false;files_message_pending=files_launch_pending=false;
 handler_acquire_mode=0;fb_handler_api=NULL;fb_handler_grant=(risc_runtime_capability_v1){0};test_handler=(t5_file_open_api_v1){0};
 source_acquire_mode=0;fsource_reset();root_acquire_mode=root_attempts=root_ready_calls=0;generic_volume=fbm_volume;generic_volume.ready=root_ready;generic_volume.file_open_read=root_open;generic_volume.file_read=root_read;generic_volume.file_close=root_close;
 files_retained=false;files_mount_epoch=0;runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=get,.release=put,.retain_invocation=retained,.monotonic_ms=monotonic,.resident_shell=root_resident_bind};files_runtime=fb_runtime=&runtime;
 memset(&keyboard,0,sizeof(keyboard));keyboard.base.base.base=(risc_text_entry_api_v1){.api_version=1,.struct_size=sizeof(keyboard),.open=native_open,.poll=native_poll,.close=native_close};keyboard.base.base.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG;keyboard.base.base.home_reason_version=1;keyboard.base.masked_tag=RISC_TEXT_ENTRY_MASKED_TAG;keyboard.base.masked_version=1;keyboard.extended_tag=RISC_TEXT_ENTRY_EXTENDED_TAG;keyboard.extended_version=1;keyboard.open_extended=extended_open;keyboard.poll_extended=extended_poll;
 files_ui_init(&files_ui,&runtime,files_alive,files_retain,NULL);files_text_pending_init(&files_text_queue,NULL,files_alive,files_retain,files_text_snapshot,files_text_drain,files_text_open);
 fb_text=(portable_text_client){0};fb_text_active=fb_text_closing=fb_text_invalidated=false;fb_mode=FB_OPTIONS;fb_choice=8;fb_storage_index=0;fb_volume=NULL;fb_grant=(risc_runtime_capability_v1){0};memset(&fb_selected,0,sizeof(fb_selected));strcpy(fb_path,"/");fb_file_path[0]=0;strcpy(fb_editor,"original draft");fbm_new_directory=true;fbm_mounted=false;fpeer=NULL;memset(&fpeer_status,0,sizeof(fpeer_status));opened=closed=released=0;open_result=close_result=0;mutate_on_open=false;
}
int main(void){
 setup();assert(fb_text_begin(FB_RENAME,"New folder")&&files_text_queue.pending&&!opened);mutate_on_open=true;assert(files_text_pending_step(&files_text_queue)==FILES_TEXT_OPENED);assert(opened==1&&!strcmp(captured,"original draft"));assert(fb_text_return_mode==FB_OPTIONS&&fb_text_return_choice==8&&fb_text_storage==0);assert(fb_text_closing); /* missing policy suffix cancels safely */
 close_result=RISC_TEXT_ENTRY_AGAIN;assert(!fb_text_close()&&fb_text_active&&!released);close_result=0;assert(fb_text_close()&&released==1&&!fb_text_active);
 setup();assert(fb_text_begin(FB_RENAME,"New folder"));files_text_intent prior=files_text_queue.intent;files_scene_mount_changed();assert(files_text_pending_step(&files_text_queue)==FILES_TEXT_CANCELLED&&!opened);files_text_retire(&prior);assert(fbm_new_directory);
 setup();assert(fb_text_begin(FB_RENAME,"New folder"));fbm_wait_generation++;assert(files_text_pending_step(&files_text_queue)==FILES_TEXT_CANCELLED&&!opened);
 setup();open_result=RISC_TEXT_ENTRY_BUSY;assert(fb_text_begin(FB_RENAME,"New folder"));prior=files_text_queue.intent;assert(files_text_pending_step(&files_text_queue)==FILES_TEXT_UNAVAILABLE&&opened==1&&released==1&&!files_retained);files_text_retire(&prior);assert(!fbm_new_directory);assert(files_text_pending_step(&files_text_queue)==FILES_TEXT_WAIT&&opened==1);
 setup();open_result=RISC_TEXT_ENTRY_AGAIN;assert(fb_text_begin(FB_RENAME,"New folder"));assert(files_text_pending_step(&files_text_queue)==FILES_TEXT_RETAINED&&opened==1&&files_retained&&!released);
 setup();fb_storage_index=0;assert(!fsh_volume_share());
#ifdef PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE
 fb_storage_index=1;assert(!fsh_volume_share());fb_storage_index=2;assert(fsh_volume_share());
#endif
 setup();root_acquire_mode=1;
#ifdef PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE
 assert(files_select_root()&&fb_storage_index==2&&root_attempts==3&&!root_ready_calls);
#endif
 setup();root_acquire_mode=2;assert(files_select_root()&&fb_storage_index==0&&root_attempts==1&&!root_ready_calls);
 setup();root_acquire_mode=3;assert(!files_select_root()&&files_retained&&root_attempts==1);
 setup();source_acquire_mode=1;assert(!fsource_acquire()&&fsource_can_fallback()&&!files_retained);fsource_token=77;assert(!fsource_can_fallback());fsource_token=0;fsource_state=FSOURCE_CANCEL;assert(!fsource_can_fallback());
 setup();source_acquire_mode=2;assert(!fsource_acquire()&&files_retained&&!fsource_can_fallback());assert(!released);
 setup();source_acquire_mode=1;fsource_grant.slot=2;assert(!fsource_acquire()&&files_retained&&!fsource_can_fallback());
 setup();test_clock=6000;files_idle_at=0;files_idle_ms=5000;fsh_phase=FSH_OFF;fbm_operation=0;checkpoints=0;
 assert(files_resident_init(&files_host,&runtime,files_alive,files_retain,files_resident_settle,NULL));
 assert(files_host_step(true)&&checkpoint_reason==RISC_RESIDENT_QUERY_ALARM_ONLY);
 assert(files_host_step(true)&&checkpoint_reason==RISC_RESIDENT_QUERY_IDLE_ONLY);
 assert(files_host_step(true)&&checkpoint_reason==RISC_RESIDENT_APPLY_IDLE_ONLY);
 assert(files_host_step(true)&&checkpoint_reason==RISC_RESIDENT_QUERY_LAST_IDLE_ALARM&&checkpoints==4&&files_idle_at==test_clock&&!files_host.phase);
 test_clock+=6000;fsh_phase=FSH_ACTIVE;assert(files_host_step(true)&&checkpoints==4);fsh_phase=FSH_OFF;assert(files_host_step(false)&&checkpoints==4);assert(files_host_step(true)&&checkpoints==5);
 setup();handler_acquire_mode=1;assert(!fbx_handlers_scoped()&&!files_retained&&!released);
 setup();handler_acquire_mode=2;assert(!fbx_handlers_scoped()&&files_retained&&!released);
 setup();handler_acquire_mode=3;assert(!fbx_handlers_scoped()&&files_retained&&!released);
 setup();handler_acquire_mode=4;assert(!fbx_handlers_scoped()&&files_retained&&!released);
 setup();fb_handler_grant.generation=4;assert(!fbx_handlers_scoped()&&files_retained&&!released);
 puts("19 Files actual domain text/root-selection/source-absence/resident groups PASS (endpoint doubles)");
}
