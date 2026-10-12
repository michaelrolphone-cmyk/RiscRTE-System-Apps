#define PORTABLE_FILE_BROWSER_SHARED_INPUT
#define PORTABLE_TEXT_INPUT_CLIENT
#define FILE_SHARED_INPUT_FIXTURE
#include "file_browser_mount_test.c"
#include "RiscProviderV2.h"
#include "RiscSceneV1.h"
#include "../../Services/text_input/SceneKeyboardV1.h"
extern const risc_driver_v2 *files_text_driver_get(uint32_t);
static const risc_driver_v2 *text_driver;
static risc_scene_document_v1 text_document;
static unsigned text_key_value,text_event_kind,text_opens,text_closes,text_grants;
static bool text_presenting,text_denied,text_fail_close;
static int32_t text_scene_open(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*p,uint64_t*out){
 (void)c;(void)p;assert(text_input_suspended&&!test_subs&&!test_frames);text_document=*d;*out=11;++text_opens;return 0;
}
static int32_t text_scene_update(void*c,uint64_t s,const risc_scene_document_v1*d){(void)c;assert(s==11&&text_input_suspended);text_document=*d;return 0;}
static int32_t text_scene_next(void*c,uint64_t s,risc_scene_event_v1*out){(void)c;assert(s==11);if(!text_event_kind)return RISC_SCENE_IDLE;
 *out=(risc_scene_event_v1){.struct_size=sizeof(*out),.kind=text_event_kind,.document_revision=text_document.revision,.node=1,.action=1,.value=(int32_t)text_key_value,.sequence=1};text_event_kind=0;return 0;
}
static int32_t text_scene_snapshot(void*c,uint64_t s,risc_scene_navigation_v1*p,uint32_t*f){(void)c;(void)p;assert(s==11);*f=text_presenting?RISC_SCENE_PRESENTING:0;return 0;}
static int32_t text_scene_close(void*c,uint64_t s){(void)c;assert(s==11);if(text_fail_close)return RISC_SCENE_RETAINED;if(text_presenting)return RISC_SCENE_AGAIN;++text_closes;return 0;}
static const risc_scene_api_v1 text_scene={1,sizeof(text_scene),NULL,text_scene_open,text_scene_update,text_scene_next,NULL,text_scene_snapshot,text_scene_close};
static bool text_acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 if(strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)){bool ok=get_cap(n,v,instance,g);if(ok){g->slot=g->slot?g->slot:1;g->generation=g->generation?g->generation:1;}return ok;}
 assert(v==1&&!instance&&!text_grants);if(text_denied)return false;
 g->api=text_driver->capability;g->slot=91;g->generation=1;text_grants++;test_grants++;return true;
}
static bool text_release(risc_runtime_capability_v1*g){bool ok;if(g->api==text_driver->capability){assert(text_driver->quiesce()&&text_grants==1);text_grants--;ok=test_release(g);}else ok=release_cap(g);if(ok)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return ok;}
static void text_step(void){test_ticks++;fsh_tick();peer_backend_tick(fsh_now);if(cancel_calls)quiescent=true;if(fb_text_active)fb_text_step();}
static void text_key(unsigned key){assert(fb_text_active&&!fb_text_closing);text_key_value=key;text_event_kind=RISC_SCENE_VALUE_EVENT;text_step();}
static void text_replace(const char*value){text_key(RISC_SCENE_KEY_CLEAR);for(unsigned i=0;value[i];i++)text_key((unsigned char)value[i]);}
static void text_finish(unsigned key){text_key(key);for(unsigned i=0;i<100&&fb_text_active;i++)text_step();if(fb_text_active||text_grants||test_subs!=1||text_input_suspended)fprintf(stderr,"finish active=%u closing=%u retained=%u native=%u text=%u subs=%u suspended=%u state=%u opens=%u closes=%u\n",fb_text_active,fb_text_closing,fb_text.retained,native_custody_retained,text_grants,test_subs,text_input_suspended,fb_text_state.state,text_opens,text_closes);assert(!fb_text_active&&!text_grants&&test_subs==1&&!text_input_suspended);}
static void text_open_action(const char*name){mount_action(name,3);assert(fb_mode==FB_RENAME&&fb_text_active&&text_grants==1&&!test_subs);}
static void text_clean(void){(void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)text_step();assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains&&!fb_text_active&&!text_grants);assert(text_driver->quiesce());text_driver->stop();peer_backend_end();}
static bool entry_neutral,entry_home,entry_cancel,entry_pending,entry_entered;
static unsigned entry_polls,entry_keys,entry_closing_ticks;
static bool entry_poll(t5_app_input_t*out,uint32_t wait){
 (void)wait;assert(++entry_polls<200);*out=(t5_app_input_t){0};
 if(!entry_neutral){entry_neutral=true;return true;}entry_neutral=false;
 if(entry_entered&&!fb_text_active){out->exit_requested=true;return true;}
 if(fb_mode==FB_LIST){out->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 if(fb_mode==FB_ACTIONS){out->buttons=fb_choice==3?T5_APP_BUTTON_CONFIRM:T5_APP_BUTTON_DOWN;return true;}
 assert(false);return false;
}
static void entry_yield(uint32_t n){
 test_yield(n);peer_backend_tick(fsh_now);if(cancel_calls)quiescent=true;
 if(!fb_text_active)return;
 entry_entered=true;
 if(fb_text_closing){if(++entry_closing_ticks==3)text_presenting=false;return;}
 static const unsigned keys[]={RISC_SCENE_KEY_CLEAR,'Q','Q','W','W',RISC_SCENE_KEY_DONE};
 if(entry_keys>=sizeof(keys)/sizeof(keys[0]))return;
 text_presenting=entry_pending;unsigned key=keys[entry_keys++];
 if(key==RISC_SCENE_KEY_DONE&&entry_home)text_event_kind=RISC_SCENE_SUSPEND_EVENT;
 else {text_key_value=key==RISC_SCENE_KEY_DONE&&entry_cancel?RISC_SCENE_KEY_CANCEL:key;text_event_kind=RISC_SCENE_VALUE_EVENT;}
}
static void entry_workflow(const char*scenario){
 assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_subs);
 test_browser_poll_override=entry_poll;runtime.yield_ms=entry_yield;entry_home=strstr(scenario,"home")!=NULL;entry_cancel=strstr(scenario,"cancel")!=NULL;entry_pending=strstr(scenario,"pending")!=NULL;
 assert(app_module_init()==0);app_main();assert(entry_entered&&entry_keys==6&&!fb_text_active&&!text_grants);app_module_fini();assert(!test_grants&&!test_subs&&!test_frames&&!retains);
 if(entry_home||entry_cancel)assert(peer_backend_local_exists("/Downloads/local.txt")&&!peer_backend_local_exists("/Downloads/QQWW"));
 else assert(peer_backend_local_exists("/Downloads/QQWW")&&!peer_backend_local_exists("/Downloads/local.txt"));
 assert(!entry_pending||entry_closing_ticks>=3);assert(text_driver->quiesce());text_driver->stop();peer_backend_end();printf("Files actual app_main %s: shared text result, retained pending close, neutral resume and terminal exit PASS\n",scenario);
}
int main(int argc,char**argv){
 assert(argc>=2);const char*scenario=argv[1];test_directory=argc>2?argv[2]:NULL;peer_backend_begin();peer_backend_folders();borrowed=true;
 text_driver=files_text_driver_get(2);risc_provider_dependency_v1 dependency={RISC_SCENE_CAPABILITY,1,&text_scene};assert(text_driver->start(&dependency,1));
 runtime=test_runtime;runtime.acquire=text_acquire;runtime.release=text_release;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;
 const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());strcpy(fb_path,"/Downloads");assert(fb_load(0));
 if(!strncmp(scenario,"entry-",6)){entry_workflow(scenario);return 0;}
 bool remote=!strncmp(scenario,"remote",6);
 if(remote){fb_list_choice=0;fb_option(7);peer_until(PEER_DISCOVERING);peer_step();peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);peer_frame();fpeer_choice=1;fpeer_action();peer_until(PEER_READY);assert(fbm_mounted&&fb_storage_index==2);}
 const char*name=remote?"incoming.obj":"local.txt";
 if(strstr(scenario,"filter")){
  fb_mode=FB_OPTIONS;fb_choice=0;fb_option(0);assert(fb_text_active&&fb_mode==FB_FILTER);text_replace("local");text_finish(RISC_SCENE_KEY_DONE);assert(fb_mode==FB_LIST&&!strcmp(fb_query,"local")&&fb_count==1);
  fb_mode=FB_OPTIONS;fb_option(0);text_replace("discard");text_finish(RISC_SCENE_KEY_CANCEL);assert(fb_mode==FB_OPTIONS&&!strcmp(fb_query,"local"));
 }else if(strstr(scenario,"nonascii")){
  text_open_action(name);text_finish(RISC_SCENE_KEY_CANCEL);
  const char original[]="caf\303\251\177.txt";strcpy(fb_editor,original);fb_mode=FB_ACTIONS;assert(fb_text_begin(FB_RENAME,"Rename"));fb_text_step();assert(!strcmp(fb_text_state.text,original));text_key('Q');assert(!strncmp(fb_text_state.text,original,sizeof(original)-1));text_finish(RISC_SCENE_KEY_DONE);assert(peer_backend_local_exists("/Downloads/caf\303\251\177.txtQ"));
 }else if(strstr(scenario,"mkdir")){
  fb_mode=FB_OPTIONS;fb_choice=8;fb_option(8);assert(fb_text_active&&fbm_new_directory);text_replace("Created");text_finish(RISC_SCENE_KEY_DONE);mount_idle();assert(remote?peer_backend_remote_exists("/Created"):peer_backend_local_exists("/Downloads/Created"));
  fb_mode=FB_OPTIONS;fb_choice=8;fb_option(8);text_replace("discard");text_finish(RISC_SCENE_KEY_CANCEL);assert(fb_mode==FB_OPTIONS&&fb_choice==8&&!fbm_new_directory);
 }else if(strstr(scenario,"unavailable")){
  text_denied=true;mount_action(name,3);assert(!fb_text_active&&fb_mode==FB_ACTIONS&&!text_grants&&strstr(fb_status,"unavailable"));text_denied=false;fb_choice=3;fbx_activate();assert(fb_text_active);text_finish(RISC_SCENE_KEY_CANCEL);
 }else{
  text_open_action(name);
  if(strstr(scenario,"home")){
   text_replace("discard");text_presenting=true;text_event_kind=RISC_SCENE_SUSPEND_EVENT;text_step();assert(fb_text_closing&&fb_text_active&&!test_subs&&!text_closes);text_presenting=false;text_step();assert(fb_text_home&&!fb_text_active&&!text_grants);
  }else if(strstr(scenario,"cancel")){
   text_replace("discard");assert(!fb_back());assert(!fb_text_active&&fb_mode==FB_ACTIONS&&!strcmp(fb_editor,name));
  }else if(strstr(scenario,"retained")){
   text_fail_close=true;text_key(RISC_SCENE_KEY_CANCEL);assert(fb_text.retained&&portable_text_adapter_retained()&&retains==1&&text_grants==1&&!test_subs);unsigned before=calls;assert(!portable_file_browser_close());app_module_fini();assert(calls==before);puts("Files shared text retained close: no provider I/O or unload cleanup PASS");return 0;
  }else if(strstr(scenario,"expiry")){
   text_replace("discard");test_ticks+=300001;for(unsigned i=0;i<30000&&(fsh_phase!=FSH_OFF||fb_text_active);i++)text_step();assert(!fbm_mounted&&!fb_text_active&&fb_storage_index==0&&!strcmp(fb_path,"/Downloads")&&!text_grants);assert(peer_backend_local_exists("/Downloads/local.txt"));
  }else if(strstr(scenario,"invalid")){
   text_replace("../bad");text_finish(RISC_SCENE_KEY_DONE);assert(fb_mode==FB_RENAME&&!strcmp(fb_editor,"../bad")&&strstr(fb_status,"Invalid"));fb_draw();fbx_activate();assert(fb_text_active);text_replace("corrected.txt");text_finish(RISC_SCENE_KEY_DONE);mount_idle();assert(fb_mode==FB_LIST);
  }else if(strstr(scenario,"long")){
   char long_name[128];memset(long_name,'Q',127);long_name[127]=0;text_presenting=true;unsigned before=test_presents;text_replace(long_name);text_key('Z');assert(strlen(fb_text_state.text)==127&&!strcmp(fb_text_state.text,long_name)&&test_presents==before);
   text_key(RISC_SCENE_KEY_DONE);assert(fb_text_active&&fb_text_closing&&!text_closes&&!test_subs);text_presenting=false;text_step();assert(!fb_text_active);mount_idle();assert(fb_mode==FB_LIST);text_open_action(long_name);assert(!strcmp(fb_text_state.text,"")&&!strcmp(fb_editor,long_name));text_finish(RISC_SCENE_KEY_CANCEL);
  }else{
   text_replace("renamed.txt");text_finish(RISC_SCENE_KEY_DONE);mount_idle();assert(fb_mode==FB_LIST);assert(remote?peer_backend_remote_exists("/renamed.txt"):peer_backend_local_exists("/Downloads/renamed.txt"));
   text_open_action("renamed.txt");text_replace("cancel.txt");text_finish(RISC_SCENE_KEY_CANCEL);text_open_action("renamed.txt");text_replace("final.txt");text_finish(RISC_SCENE_KEY_DONE);mount_idle();assert(fb_mode==FB_LIST);
  }
 }
 text_clean();printf("Files shared text %s: drafts, mutation boundary, checked host close and browser restoration PASS\n",scenario);return 0;
}
