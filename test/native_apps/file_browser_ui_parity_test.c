#define FILE_SHARED_INPUT_FIXTURE
#include "file_browser_mount_test.c"
static void ui_capture(const char*name){peer_frame();char path[512];assert(test_directory);snprintf(path,sizeof(path),"%s/%s.raw",test_directory,name);FILE*out=fopen(path,"wb");assert(out&&fwrite(fb_pixels,1,sizeof(fb_pixels),out)==sizeof(fb_pixels)&&!fclose(out));}
int main(int argc,char**argv){assert(argc==2);test_directory=argv[1];peer_backend_begin();peer_backend_folders();borrowed=true;
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;
 const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());ui_capture("root");strcpy(fb_path,"/Downloads");assert(fb_load(0));ui_capture("local-list");
 fb_mode=FB_OPTIONS;fb_choice=0;ui_capture("local-options");fb_mode=FB_LIST;mount_action("local.txt",2);ui_capture("local-preview");assert(!fb_back());ui_capture("local-actions");fb_mode=FB_LIST;mount_action("local.txt",6);ui_capture("local-delete-confirmation");assert(!fb_back());assert(!fb_back());
 fb_list_choice=0;fb_option(7);peer_until(PEER_DISCOVERING);peer_step();peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);ui_capture("peer-pairing");fpeer_choice=1;fpeer_action();peer_until(PEER_READY);ui_capture("remote-list");
 fb_mode=FB_OPTIONS;fb_choice=8;ui_capture("remote-options");fb_mode=FB_LIST;mount_action("incoming.obj",2);mount_idle();ui_capture("remote-preview");assert(!fb_back());ui_capture("remote-actions");assert(!fb_back());
 mount_action("incoming.obj",5);mount_idle();ui_capture("copy-destination");fbx_cancel();mount_idle();mount_action("incoming.obj",6);ui_capture("remote-delete-confirmation");assert(!fb_back());assert(!fb_back());
 fb_choice=mount_find("Documents");fb_activate_file(fb_choice);mount_idle();ui_capture("remote-folder");
 (void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)peer_step();assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains);peer_backend_end();puts("Files non-editor UI captured with unchanged actions and provider lifecycle");return 0;}
