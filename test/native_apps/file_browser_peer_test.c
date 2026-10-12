#include "RiscStorageVolumeStateV1.h"
#define FILE_PEER_FIXTURE
#define FILE_SETUP_TEST
#include "file_browser_sharing_test.c"
extern void peer_backend_begin(void),peer_backend_tick(uint64_t),peer_backend_verify(const char*),peer_backend_end(void);
extern void peer_backend_local_state(int),peer_backend_uploaded(void),peer_backend_direct(void);
extern void peer_backend_uploaded_contents(const char*),peer_backend_change_source(const char*);
extern uint64_t peer_backend_connections(void);
extern const void*peer_backend_capability(const char*);
extern void peer_backend_collision(void),peer_backend_collision_preserved(void);
static unsigned handler_launches;
static bool local_handoff;
static char expected_handler_path[512]="/sd/Downloads/incoming.obj";
static uint32_t peer_handler_count(const char*p){assert(!strcmp(p,expected_handler_path)||!strcmp(p,"/sd/Downloads/incoming.obj"));return 1;}
static bool peer_handler_get(const char*p,uint32_t i,t5_file_handler_t*out){assert(!strcmp(p,expected_handler_path)&&!i);memset(out,0,sizeof(*out));strcpy(out->app_id,local_handoff?"ebook_reader":"model_viewer");strcpy(out->display_name,"Model Viewer");return true;}
static bool peer_handler_open(const char*p,const char*id,uint64_t cookie){assert(!strcmp(p,expected_handler_path)&&!strcmp(id,local_handoff?"ebook_reader":"model_viewer")&&cookie==UINT64_C(0x4642524f57534552));assert(fsh_phase==FSH_OFF&&!fpeer&&!owned);if(!local_handoff)peer_backend_verify(p+3);handler_launches++;return true;}
static const t5_file_open_api_v1 peer_handlers={.api_version=1,.struct_size=sizeof(peer_handlers),.handler_count=peer_handler_count,.handler_get=peer_handler_get,.open_request=peer_handler_open};
const void*peer_fixture_capability(const char*n){return !strcmp(n,"file.open")?&peer_handlers:peer_backend_capability(n);}
static void peer_step(void){test_ticks+=1;fsh_tick();peer_backend_tick(fsh_now);if(cancel_calls)quiescent=true;}
static void peer_until(unsigned state){for(unsigned i=0;i<30000;i++){peer_step();if(fpeer_status.state==state)return;assert(!fb_terminal);}assert(!"UI timeout");}
static void peer_operation_done(void){for(unsigned i=0;i<30000&&(fbm_operation||fbm_loading);i++)peer_step();assert(!fbm_operation&&!fbm_loading&&fpeer_status.state==PEER_READY);}
static void peer_copy_local_to_remote(void){assert(fb_storage_index==0);strcpy(fb_path,"/Downloads");assert(fb_load(0));fb_choice=0;fbx_actions();fbx_destination(false);assert(fbx_swap()&&fb_storage_index==2);strcpy(fb_path,"/");(void)fb_load(0);peer_step();peer_until(PEER_READY);assert(!fbm_loading);}
static void peer_frame(void){for(unsigned i=0;i<12;i++){if(fb_dirty)fb_draw();test_ticks+=1;t5_app_input_t input={0};assert(fb_app->poll(&input,1));if(portable_file_browser_setup_frame_ready()&&!fb_dirty)return;}assert(!"frame incomplete");}
static bool unavailable_setup_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 assert(!strcmp(name,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY)&&version==1&&instance==0);
 assert(!grant->api&&!grant->slot&&!grant->generation);return false;
}
static void closed_local_setup(bool unavailable){
 risc_runtime_api_v1 setup_runtime=runtime;
 if(unavailable)setup_runtime.acquire=unavailable_setup_acquire;
 assert(portable_file_setup_init(&fsh_setup,&setup_runtime,fsh_owner,NULL));fsh_setup_initialized=true;
 const portable_file_setup_config cfg={.instance_id=0,.setup_lifetime_ms=1000,.name="Files fixture"};
 const risc_bluetooth_session_descriptor_v1 descriptor={.struct_size=sizeof(descriptor),.transport=RISC_SESSION_HTTP,.remaining_lifetime_ms=1000,.url="http://192.0.2.1/",.username="files",.password="fixture-password",.session_id="fixture-session"};
 assert(portable_file_setup_start(&fsh_setup,&cfg,&descriptor,test_ticks)==PORTABLE_FILE_SETUP_PENDING);
 assert((unavailable?portable_file_setup_step(&fsh_setup,test_ticks):portable_file_setup_close(&fsh_setup))==
  (unavailable?PORTABLE_FILE_SETUP_UNAVAILABLE:PORTABLE_FILE_SETUP_OK));
 portable_file_setup_get_status(&fsh_setup,&fsh_setup_status);
 assert(!fsh_setup.token&&!fsh_setup.grant.api&&!portable_file_browser_cleanup_only());
}
static void local_open_taps(int *x,int *y){
 /* Ordinary application loop: enter Downloads, select local.txt, choose Open. */
 if(test_steps==5||test_steps==11||test_steps==17){*x=100;*y=fb_paper?150:82;}
}
int main(int argc,char**argv){const char*mode=argc>1?argv[1]:"transfer";test_directory=argc>2?argv[2]:NULL;peer_backend_begin();if(!strcmp(mode,"existing"))peer_backend_collision();borrowed=strcmp(mode,"owned")!=0;
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;
 if(!strcmp(mode,"direct"))peer_backend_direct();else {const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);}
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0);
 if(!strncmp(mode,"local-handoff",13)){
  if(!strcmp(mode,"local-handoff-closed"))closed_local_setup(false);
  if(!strcmp(mode,"local-handoff-unavailable"))closed_local_setup(true);
  local_handoff=true;strcpy(expected_handler_path,"/sd/Downloads/local.txt");
  test_script=8;test_touch_script=local_open_taps;
  app_main();
  assert(handler_launches==1&&fb_terminal&&!fpeer&&!owned);
  assert(!retains&&"Completed file.open handoff must not retain during app_main drain");
  assert(!fb_grant.api&&!fb_handler_grant.api);
  assert(!fsh_setup_initialized||!fsh_setup_live());
  /* The app_main tail has already drained its last display after terminal
   * dispatch. Repeated drain/fini must remain a completed-state operation. */
  assert(paper_frame_drain()&&!retains);
  app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains);
  peer_backend_end();puts("Actual Files main: local file.open handoff, completed terminal drain and fini PASS");return 0;
 }
 assert(fb_open());assert(fb_option_count()==9);strcpy(fb_path,"/Downloads");assert(fb_load(0));fb_list_choice=0;
 fb_option(7);assert(fpeer_mode&&fb_mode==FB_SHARING&&fb_grant.api);peer_until(PEER_DISCOVERING);peer_step();assert(fpeer_status.count==1);peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);
 fpeer_choice=1;fpeer_action();assert(fpeer_status.state==PEER_PAIRING); /* no displayed number yet */
 peer_frame();capture_paper("peer-pairing");fpeer_action();peer_until(PEER_READY);assert(fpeer_status.count==1);peer_frame();capture_paper("peer-folder");
 assert(files_trace_confirm&&files_trace_join&&files_trace_handshake);
 {unsigned before=files_diagnostics;for(unsigned i=0;i<1000;i++)peer_step();assert(files_diagnostics==before);}
 if(!strcmp(mode,"upload")){
  assert(fbm_mounted&&fb_mode==FB_LIST);assert(!fb_back()&&fb_storage_index==0);fb_choice=0;fbx_actions();fbx_destination(false);
  assert(fbx_swap()&&fb_storage_index==2);strcpy(fb_path,"/");(void)fb_load(0);peer_step();peer_until(PEER_READY);
  fbx_commit_destination();assert(fbm_operation==FBM_COPY);peer_operation_done();assert(fpeer_status.operation_ok);peer_backend_uploaded();
  /* Declining a collision must not queue HEAD/PUT or change the old bytes. */
  peer_backend_change_source("confirmed replacement bytes");peer_copy_local_to_remote();
  uint64_t connections=peer_backend_connections();uint32_t generation=fpeer_status.operation_generation;
  fbx_commit_destination();assert(fb_mode==FB_REPLACE&&!fb_choice&&!fbm_operation);fbx_activate();assert(fb_mode==FB_DESTINATION&&!fbm_operation);
  for(unsigned i=0;i<12;i++)peer_step();
  assert(peer_backend_connections()==connections&&fpeer_status.operation_generation==generation);peer_backend_uploaded();
  /* A confirmed generic SD replacement makes exactly one HEAD and one PUT,
   * and verifies changed source bytes rather than merely a successful status. */
  fbx_commit_destination();assert(fb_mode==FB_REPLACE&&!fb_choice);fb_choice=1;fbx_activate();assert(fbm_operation==FBM_COPY);peer_operation_done();
  assert(fpeer_status.operation_ok&&!fpeer_status.uncertain&&peer_backend_connections()==connections+2);peer_backend_uploaded_contents("confirmed replacement bytes");
  /* Approval belongs to that operation. Another ordinary Copy still prompts. */
  peer_backend_change_source("later unconfirmed source");peer_copy_local_to_remote();connections=peer_backend_connections();generation=fpeer_status.operation_generation;
  fbx_commit_destination();assert(fb_mode==FB_REPLACE&&!fb_choice&&!fbm_operation);fbx_activate();assert(fb_mode==FB_DESTINATION&&!fbm_operation);
  for(unsigned i=0;i<12;i++)peer_step();
  assert(peer_backend_connections()==connections&&fpeer_status.operation_generation==generation);peer_backend_uploaded_contents("confirmed replacement bytes");
  /* Cancel an explicitly confirmed command before its first foreground tick. */
  fbx_commit_destination();assert(fb_mode==FB_REPLACE);fb_choice=1;fbx_activate();assert(fbm_operation==FBM_COPY);fbx_cancel();assert(fbm_operation==FBM_CANCEL);peer_operation_done();
  assert(!fpeer_status.operation_ok&&!fpeer_status.uncertain&&fpeer_status.operation_generation==generation+1&&fpeer_status.completed_generation==generation+1&&peer_backend_connections()==connections);peer_backend_uploaded_contents("confirmed replacement bytes");
  fb_mode=FB_LIST;strcpy(fb_path,"/");(void)fb_load(0);peer_step();peer_until(PEER_READY);
 }
 if(!strcmp(mode,"cancel")||!strcmp(mode,"expiry")){
  if(!strcmp(mode,"expiry"))test_ticks+=300001;else (void)fsh_back();
  for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++){peer_step();}assert(fsh_phase==FSH_OFF&&!fpeer&&!handler_launches&&!retains);assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();puts("Actual peer UI stop/expiry cleanup PASS");return 0;
 }
 if(!strcmp(mode,"unavailable"))peer_backend_local_state(RISC_STORAGE_STATE_UNAVAILABLE);
 fb_choice=0;fbx_actions();assert(fb_mode==FB_ACTIONS);assert(strlen(fbm_local_path)+17<sizeof(expected_handler_path));strcpy(expected_handler_path,"/sd");if(strcmp(fbm_local_path,"/"))strcat(expected_handler_path,fbm_local_path);strcat(expected_handler_path,!strcmp(mode,"existing")?"/nearby-1.obj":"/incoming.obj");fbx_open(false); /* Existing Open action materializes for its declared handler. */
 if(!strcmp(mode,"unavailable")){peer_step();assert(!handler_launches&&!fpeer_status.published_path[0]);(void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++){peer_step();}assert(!handler_launches&&!fpeer&&!retains);assert(portable_file_browser_close());app_module_fini();assert(!test_grants);peer_backend_end();puts("Actual peer UI unavailable/exported local media refuses download and launch PASS");return 0;}
 for(unsigned i=0;i<30000&&!handler_launches;i++){peer_step();}assert(handler_launches==1&&fb_terminal&&!fpeer&&!owned&&!retains);assert(paper_frame_drain()&&!retains);app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);if(!strcmp(mode,"existing"))peer_backend_collision_preserved();peer_backend_end();
 puts("Actual shared Files UI: local selection, peer discovery, displayed-number confirmation, production WebDAV download, cleanup before declared Model Viewer file-type launch PASS");return 0;
}
