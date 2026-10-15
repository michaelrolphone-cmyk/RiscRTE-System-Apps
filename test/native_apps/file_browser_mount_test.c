#include "RiscStorageVolumeStateV1.h"
#define FILE_PEER_FIXTURE
#define FILE_SETUP_TEST
#include "file_browser_sharing_test.c"
extern void peer_backend_begin(void),peer_backend_tick(uint64_t),peer_backend_verify(const char*),peer_backend_end(void);
extern void peer_backend_local_state(int),peer_backend_uploaded(void),peer_backend_direct(void);
extern const void*peer_backend_capability(const char*);
static unsigned handler_launches;
static uint32_t peer_handler_count(const char*p){assert(!strcmp(p,"/sd/Downloads/incoming.obj"));return 1;}
static bool peer_handler_get(const char*p,uint32_t i,t5_file_handler_t*out){assert(!strcmp(p,"/sd/Downloads/incoming.obj")&&!i);memset(out,0,sizeof(*out));strcpy(out->app_id,"model_viewer");strcpy(out->display_name,"Model Viewer");return true;}
static bool peer_handler_open(const char*p,const char*id,uint64_t cookie){assert(!strcmp(p,"/sd/Downloads/incoming.obj")&&!strcmp(id,"model_viewer")&&cookie==UINT64_C(0x4642524f57534552));assert(fsh_phase==FSH_OFF&&!fpeer&&!owned);peer_backend_verify(p+3);handler_launches++;return true;}
static const t5_file_open_api_v1 peer_handlers={.api_version=1,.struct_size=sizeof(peer_handlers),.handler_count=peer_handler_count,.handler_get=peer_handler_get,.open_request=peer_handler_open};
const void*peer_fixture_capability(const char*n){return !strcmp(n,"file.open")?&peer_handlers:peer_backend_capability(n);}
static void peer_step(void){test_ticks+=1;fsh_tick();peer_backend_tick(fsh_now);if(cancel_calls)quiescent=true;}
static void peer_until(unsigned state){for(unsigned i=0;i<30000;i++){peer_step();if(fpeer_status.state==state)return;assert(!fb_terminal);}assert(!"UI timeout");}
static void peer_frame(void){for(unsigned i=0;i<12;i++){if(fb_dirty)fb_draw();test_ticks+=1;t5_app_input_t input={0};assert(fb_app->poll(&input,1));if(portable_file_browser_setup_frame_ready()&&!fb_dirty)return;}assert(!"frame incomplete");}
extern void peer_backend_folders(void),peer_backend_watch(void),peer_backend_watch_replaced(void);
extern bool peer_backend_remote_exists(const char*),peer_backend_local_exists(const char*);
static void mount_idle(void){for(unsigned i=0;i<30000;i++){peer_step();if(!fbm_operation&&!fbm_loading)return;assert(!fb_terminal);}assert(!"mounted operation timeout");}
static unsigned mount_find(const char*name){for(unsigned pages=0;pages<40;pages++){for(unsigned i=0;i<fb_count;i++)if(!strcmp(fb_rows[i].name,name))return i;if(!fb_more)break;fb_page_next();}assert(!"missing browser row");return 0;}
static void mount_action(const char*name,unsigned action){fb_choice=mount_find(name);fbx_actions();assert(fb_mode==FB_ACTIONS);fb_choice=action;fbx_activate();}

#ifndef FILE_SHARED_INPUT_FIXTURE
int main(int argc,char**argv){const char*scenario=argc>1?argv[1]:"actions";test_directory=argc>2?argv[2]:NULL;peer_backend_begin();if(!strcmp(scenario,"watch-export"))peer_backend_watch();else peer_backend_folders();borrowed=true;
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;
 const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());strcpy(fb_path,"/Downloads");assert(fb_load(0));fb_list_choice=0;
 fb_option(7);peer_until(PEER_DISCOVERING);peer_step();peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);peer_frame();fpeer_choice=1;fpeer_action();peer_until(PEER_READY);

 if(!strcmp(scenario,"watch-export")){
  assert(fbm_mounted&&fb_mode==FB_LIST&&fb_matched==5&&fpeer_status.capabilities==15);
  fb_choice=mount_find("models");fb_activate_file(fb_choice);mount_idle();assert(fb_count==1&&!strcmp(fb_rows[0].name,"model.obj"));peer_frame();capture_paper("watch-models-browser");
  mount_action("model.obj",3);assert(fb_mode==FB_NOTICE&&strstr(fb_status,"unavailable"));assert(!fb_back());assert(!fb_back());
  fb_option(8);assert(fb_mode==FB_NOTICE&&strstr(fb_status,"unavailable"));assert(!fb_back());assert(!fb_back());
  mount_action("model.obj",2);mount_idle();assert(fb_preview_count==32);assert(!fb_back());assert(!fb_back());
  mount_action("model.obj",5);mount_idle();assert(fbx_swap());strcpy(fb_path,"/Downloads");assert(fb_load(0));fbx_commit_destination();mount_idle();assert(peer_backend_local_exists("/Downloads/model.obj"));
  assert(fbx_swap());strcpy(fb_path,"/Downloads");assert(fb_load(0));mount_action("model.obj",5);assert(fbx_swap());strcpy(fb_path,"/models");assert(!fb_load(0));mount_idle();fbx_commit_destination();assert(fb_mode==FB_REPLACE);fb_choice=1;fbx_activate();mount_idle();assert(fpeer_status.operation_ok);peer_backend_watch_replaced();
  (void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)peer_step();assert(!fpeer&&!retains);assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();
  puts("Actual Watch eight-path export in common browser: mapped folders, children, preview, Copy out, confirmed revision-safe Copy back, unavailable operations PASS");return 0;
 }
 assert(fbm_mounted&&fb_mode==FB_LIST&&fb_storage_index==2&&fb_loaded&&fb_count==2);
 assert(!strcmp(fb_rows[0].name,"Documents")&&fb_rows[0].is_directory);
 assert(!strcmp(fb_rows[1].name,"incoming.obj")&&!fb_rows[1].is_directory);
 peer_frame();capture_paper("mounted-root");
 if(!strcmp(scenario,"failures")){
  /* A queued operation is still pending while service begin/end is BUSY. */
  mount_action("incoming.obj",2);lease_busy=true;uint32_t queued=fbm_wait_generation;
  for(unsigned i=0;i<4;i++){peer_step();assert(fbm_operation==FBM_PREVIEW&&fpeer_status.completed_generation!=queued);}
  unsigned mode=fb_mode;uint64_t offset=fb_offset;fb_preview_page(1);fb_activate();assert(fb_mode==mode&&fb_offset==offset&&!fb_hex);
  lease_busy=false;mount_idle();assert(fb_preview_count==32);assert(!fb_back());assert(!fb_back());
  end_busy=true;peer_step();assert(!fb_load(0)&&fbm_loading);queued=fbm_wait_generation;
  for(unsigned i=0;i<4;i++){peer_step();assert(fbm_loading&&fpeer_status.completed_generation!=queued);}
  fb_option(3);assert(!strcmp(fb_path,"/"));end_busy=false;mount_idle();assert(fb_loaded&&fb_count==2);
  /* Failed directory never exposes old rows; ordinary Refresh retries. */
  strcpy(fb_path,"/missing");assert(!fb_load(0));mount_idle();assert(!fb_loaded&&!fb_count&&!fpeer_status.operation_ok);
  strcpy(fb_path,"/");assert(!fb_load(0));mount_idle();assert(fb_loaded&&fb_count==2);
  /* User Cancel before command consumption keeps mounted session usable. */
  mount_action("incoming.obj",2);lease_busy=true;assert(!fb_back()&&fbm_operation==FBM_CANCEL);for(unsigned i=0;i<4;i++)peer_step();assert(fbm_operation==FBM_CANCEL);
  lease_busy=false;mount_idle();assert(!fbm_operation&&fbm_mounted&&fpeer_status.state==PEER_READY);assert(!fb_load(0));mount_idle();
  /* Expiry invalidates a remote copy source before Here can use a local API. */
  mount_action("incoming.obj",5);mount_idle();assert(fbx_swap());strcpy(fb_path,"/Downloads");assert(fb_load(0));assert(fb_source_index==2&&fb_mode==FB_DESTINATION);
  test_ticks+=300001;for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)peer_step();assert(!fpeer&&!fbm_mounted&&fb_mode==FB_LIST&&fb_source_index==0&&!fb_source_path[0]);
  assert(!strcmp(fb_path,"/Downloads")&&fb_loaded&&!peer_backend_local_exists("/Downloads/incoming.obj"));
  assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();puts("Mounted async busy/retry/cancel/expiry preserves generation, rows and local source identity PASS");return 0;
 }

 fb_activate_file(0);assert(fbm_loading&&!fb_loaded&&!strcmp(fb_path,"/Documents"));peer_step();peer_until(PEER_READY);
 assert(fb_loaded&&fb_count==1&&!strcmp(fb_rows[0].name,"Nested"));
 fb_activate_file(0);assert(fbm_loading);peer_step();peer_until(PEER_READY);
 assert(fb_loaded&&fb_count==1&&!strcmp(fb_rows[0].name,"note.txt"));peer_frame();capture_paper("mounted-nested");
 assert(!fb_back()&&fbm_loading);peer_step();peer_until(PEER_READY);assert(!strcmp(fb_path,"/Documents")&&fb_count==1);
 assert(!fb_back());peer_step();peer_until(PEER_READY);assert(!strcmp(fb_path,"/")&&fb_count==2);
 assert(!fb_back()&&fb_storage_index==0&&!strcmp(fb_path,"/Downloads")&&fb_loaded);
 assert(fbx_swap()&&fb_storage_index==2);strcpy(fb_path,"/");assert(!fb_load(0));peer_step();peer_until(PEER_READY);assert(fb_loaded&&fb_count==2);

 /* Common Preview action and Back, with no remote-specific panel. */
 mount_action("incoming.obj",2);assert(fbm_operation==FBM_PREVIEW&&fb_mode==FB_PREVIEW);mount_idle();assert(fb_preview_count==32&&!memcmp(fb_preview,"v 0 0 0",7));assert(!fb_back());assert(!fb_back());
 /* Common Rename and New-folder editor. */
 mount_action("incoming.obj",3);assert(fb_mode==FB_RENAME);strcpy(fb_editor,"renamed.obj");fb_filter_key(PWK_DONE);mount_idle();assert(peer_backend_remote_exists("/renamed.obj")&&!peer_backend_remote_exists("/incoming.obj"));
 fb_option(8);assert(fb_mode==FB_RENAME);strcpy(fb_editor,"Created");fb_filter_key(PWK_DONE);mount_idle();assert(peer_backend_remote_exists("/Created"));
 /* Common Copy destination picker, remote to local. */
 mount_action("renamed.obj",5);mount_idle();assert(fb_mode==FB_DESTINATION&&fb_source_index==2);assert(fbx_swap()&&fb_storage_index==0);strcpy(fb_path,"/Downloads");assert(fb_load(0));fbx_commit_destination();assert(fbm_operation==FBM_COPY);mount_idle();assert(peer_backend_local_exists("/Downloads/renamed.obj")&&fb_storage_index==2&&fb_mode==FB_LIST);
 /* Same-volume Move uses the same destination picker. */
 mount_action("renamed.obj",4);mount_idle();fb_choice=mount_find("Created");fbx_activate();mount_idle();fbx_commit_destination();mount_idle();assert(peer_backend_remote_exists("/Created/renamed.obj")&&!peer_backend_remote_exists("/renamed.obj"));
 /* Normal Delete confirmation, then refresh the ordinary list. */
 fb_choice=mount_find("Created");fb_activate_file(fb_choice);mount_idle();mount_action("renamed.obj",6);assert(fb_mode==FB_DELETE);fb_choice=1;fbx_activate();mount_idle();assert(!peer_backend_remote_exists("/Created/renamed.obj"));
 assert(!fb_back());mount_idle();
 /* Cancel is wired through the destination's same Cancel control. */
 fb_choice=mount_find("Documents");fb_activate_file(fb_choice);mount_idle();fb_choice=mount_find("Nested");fb_activate_file(fb_choice);mount_idle();mount_action("note.txt",5);mount_idle();assert(fbx_swap());strcpy(fb_path,"/Downloads");assert(fb_load(0));fbx_commit_destination();assert(fbm_operation==FBM_COPY);fbx_cancel();assert(fbm_operation==FBM_CANCEL);mount_idle();assert(!peer_backend_local_exists("/Downloads/note.txt"));
 (void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)peer_step();assert(fsh_phase==FSH_OFF&&!fpeer&&!fbm_mounted&&!retains);
 assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();
 puts("Mounted peer: common rows/navigation/preview/rename/mkdir/copy/move/delete/cancel and checked disconnect PASS");return 0;
}

#endif
