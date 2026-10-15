#include "RiscStorageVolumeStateV1.h"
#define FILE_PEER_FIXTURE
#define FILE_SETUP_TEST
#include "file_browser_sharing_test.c"
extern void peer_backend_begin(void),peer_backend_tick(uint64_t),peer_backend_verify(const char*),peer_backend_end(void);
extern void peer_backend_local_state(int),peer_backend_uploaded(void),peer_backend_direct(void);
extern const void*peer_backend_capability(const char*);
extern void peer_scope_assert(void),peer_scope_enable(void),peer_scope_busy(bool,bool);
extern unsigned peer_scope_calls(void);
extern bool peer_scope_joined(void),peer_scope_held(void);
static unsigned handler_launches;
static uint32_t peer_handler_count(const char*p){peer_scope_assert();assert(!strcmp(p,"/sd/Downloads/incoming.obj"));return 1;}
static bool peer_handler_get(const char*p,uint32_t i,t5_file_handler_t*out){peer_scope_assert();assert(!strcmp(p,"/sd/Downloads/incoming.obj")&&!i);memset(out,0,sizeof(*out));strcpy(out->app_id,"model_viewer");strcpy(out->display_name,"Model Viewer");return true;}
static bool peer_handler_open(const char*p,const char*id,uint64_t cookie){assert(!strcmp(p,"/sd/Downloads/incoming.obj")&&!strcmp(id,"model_viewer")&&cookie==UINT64_C(0x4642524f57534552));assert(fsh_phase==FSH_OFF&&!fpeer&&!owned);peer_backend_verify(p+3);handler_launches++;return true;}
static const t5_file_open_api_v1 peer_handlers={.api_version=1,.struct_size=sizeof(peer_handlers),.handler_count=peer_handler_count,.handler_get=peer_handler_get,.open_request=peer_handler_open};
const void*peer_fixture_capability(const char*n){if(!strcmp(n,"file.open")||!strcmp(n,"storage.installed-files"))peer_scope_assert();return !strcmp(n,"file.open")?&peer_handlers:peer_backend_capability(n);}
static void peer_step(void){test_ticks+=1;fsh_tick();peer_backend_tick(fsh_now);if(cancel_calls)quiescent=true;}
static void peer_until(unsigned state){for(unsigned i=0;i<30000;i++){peer_step();if(fpeer_status.state==state)return;assert(!fb_terminal);}assert(!"UI timeout");}
static void peer_frame(void){for(unsigned i=0;i<12;i++){if(fb_dirty)fb_draw();test_ticks+=1;t5_app_input_t input={0};assert(fb_app->poll(&input,1));if(portable_file_browser_setup_frame_ready()&&!fb_dirty)return;}assert(!"frame incomplete");}
extern void peer_backend_folders(void);
extern bool peer_backend_remote_exists(const char*),peer_backend_local_exists(const char*);
static void mount_idle(void){for(unsigned i=0;i<30000;i++){peer_step();if(!fbm_operation&&!fbm_loading)return;assert(!fb_terminal);}assert(!"mounted operation timeout");}
static unsigned mount_find(const char*name){for(unsigned pages=0;pages<40;pages++){for(unsigned i=0;i<fb_count;i++)if(!strcmp(fb_rows[i].name,name))return i;if(!fb_more)break;fb_page_next();}assert(!"missing browser row");return 0;}
static void mount_action(const char*name,unsigned action){fb_choice=mount_find(name);fbx_actions();assert(fb_mode==FB_ACTIONS);fb_choice=action;fbx_activate();}

int main(void){
 peer_backend_begin();peer_scope_enable();borrowed=false;
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;
 assert(app_module_init()==0&&fb_open());strcpy(fb_path,"/Downloads");assert(fb_load(0));
 fb_option(7);peer_until(PEER_DISCOVERING);peer_step();peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);peer_frame();fpeer_choice=1;fpeer_action();
 for(unsigned i=0;i<30000&&!peer_scope_joined();i++)peer_step();assert(peer_scope_joined()&&!fbm_mounted);
 /* Local callbacks during private JOIN are leased, even before mount. */
 unsigned before=peer_scope_calls();assert(fb_load(0));assert(peer_scope_calls()>before&&!peer_scope_held());
 peer_scope_busy(true,false);before=peer_scope_calls();assert(!fb_load(0)&&fb_io_pending&&peer_scope_calls()==before&&!peer_scope_held());peer_scope_busy(false,false);
 peer_until(PEER_READY);assert(fbm_mounted);assert(!fb_back()&&fb_storage_index==0);assert(!strcmp(fb_path,"/Downloads"));
 /* A pending end retains its lease but never repeats completed I/O. */
 peer_scope_busy(false,true);before=peer_scope_calls();assert(fb_load(0)&&peer_scope_held()&&peer_scope_calls()>before);
 before=peer_scope_calls();assert(!fb_load(0)&&fb_io_pending&&peer_scope_calls()==before&&peer_scope_held());
 peer_scope_busy(false,false);assert(!fb_load(0)&&fb_io_pending&&peer_scope_calls()==before&&!peer_scope_held());assert(fb_load(0)&&!fb_io_pending&&peer_scope_calls()>before);
 fb_option(8);strcpy(fb_editor,"Local folder");fb_filter_key(PWK_DONE);assert(peer_backend_local_exists("/Downloads/Local folder"));
 mount_action("local.txt",2);assert(fb_mode==FB_PREVIEW&&fb_preview_count==12);assert(!fb_back());assert(!fb_back());
 mount_action("local.txt",3);strcpy(fb_editor,"renamed.txt");fb_filter_key(PWK_DONE);assert(peer_backend_local_exists("/Downloads/renamed.txt"));
 mount_action("renamed.txt",5);fbx_commit_destination();assert(peer_backend_local_exists("/renamed.txt"));
 mount_action("renamed.txt",6);fb_choice=1;fbx_activate();assert(!peer_backend_local_exists("/Downloads/renamed.txt"));
 /* Handler discovery must be scoped even while the peer remains connected. */
 assert(fbx_handlers());strcpy(fb_handler_path,"/sd/Downloads/incoming.obj");fb_handler_count=1;fb_handler_offset=0;assert(fbx_handler_page());fb_mode=FB_LIST;
 assert(fbx_swap()&&fb_storage_index==2);strcpy(fb_path,"/");assert(!fb_load(0));mount_idle();
 /* Remote Copy checks the local destination under a short lease too. */
 mount_action("incoming.obj",5);mount_idle();assert(fbx_swap());strcpy(fb_path,"/");assert(fb_load(0));fbx_commit_destination();assert(fbm_operation==FBM_COPY);mount_idle();assert(peer_backend_local_exists("/incoming.obj"));
 /* Choose Downloads as the ordinary local materialization destination. */
 assert(fbx_swap());strcpy(fb_path,"/Downloads");assert(fb_load(0));assert(fbx_swap());strcpy(fb_path,"/");(void)fb_load(0);mount_idle();
 peer_frame();mount_action("incoming.obj",0);assert(fbm_operation==FBM_OPEN);
 for(unsigned i=0;i<30000&&!handler_launches;i++)peer_step();assert(handler_launches==1&&fb_terminal&&!fpeer&&!peer_scope_held()&&!retains);
 app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();
 puts("Local service scopes: JOIN/connected reads, preview, rename, copy, delete, handler discovery, busy begin, pending end without replay, and peer Open PASS");
 return 0;
}
