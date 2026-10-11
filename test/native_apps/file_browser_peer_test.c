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
int main(int argc,char**argv){const char*mode=argc>1?argv[1]:"transfer";test_directory=argc>2?argv[2]:NULL;peer_backend_begin();borrowed=strcmp(mode,"owned")!=0;
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;test_runtime_override=&runtime;
 if(!strcmp(mode,"direct"))peer_backend_direct();else {const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);}
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());assert(fb_option_count()==8);strcpy(fb_path,"/Downloads");assert(fb_load(0));fb_list_choice=0;
 fb_option(7);assert(fpeer_mode&&fb_mode==FB_SHARING&&fb_grant.api);peer_until(PEER_DISCOVERING);peer_step();assert(fpeer_status.count==1);peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);
 fpeer_choice=1;fpeer_action();assert(fpeer_status.state==PEER_PAIRING); /* no displayed number yet */
 peer_frame();capture_paper("peer-pairing");fpeer_action();peer_until(PEER_READY);assert(fpeer_status.count==1);peer_frame();capture_paper("peer-folder");
 if(!strcmp(mode,"upload")){fpeer_choice=fpeer_status.count+1;fpeer_action();peer_step();peer_until(PEER_READY);peer_backend_uploaded();}
 if(!strcmp(mode,"cancel")||!strcmp(mode,"expiry")){
  if(!strcmp(mode,"expiry"))test_ticks+=300001;else (void)fsh_back();
  for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++){peer_step();}assert(fsh_phase==FSH_OFF&&!fpeer&&!handler_launches&&!retains);assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();puts("Actual peer UI stop/expiry cleanup PASS");return 0;
 }
 if(!strcmp(mode,"unavailable"))peer_backend_local_state(RISC_STORAGE_STATE_UNAVAILABLE);
 fpeer_choice=0;fpeer_action();assert(fpeer_page==1);fpeer_choice=1;fpeer_action(); /* download and open */
 if(!strcmp(mode,"unavailable")){peer_step();assert(!handler_launches&&!fpeer_status.published_path[0]);(void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++){peer_step();}assert(!handler_launches&&!fpeer&&!retains);assert(portable_file_browser_close());app_module_fini();assert(!test_grants);peer_backend_end();puts("Actual peer UI unavailable/exported local media refuses download and launch PASS");return 0;}
 for(unsigned i=0;i<30000&&!handler_launches;i++){peer_step();}assert(handler_launches==1&&fb_terminal&&!fpeer&&!owned&&!retains);app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);peer_backend_end();
 puts("Actual shared Files UI: local selection, peer discovery, displayed-number confirmation, production WebDAV download, cleanup before declared Model Viewer file-type launch PASS");return 0;
}
