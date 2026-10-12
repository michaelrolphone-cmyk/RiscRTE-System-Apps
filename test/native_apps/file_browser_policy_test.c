#define FILE_POLICY_FIXTURE
#ifdef FILE_POLICY_RESIDENT
#define FILE_BROWSER_RESIDENT_PROFILE
#define PORTABLE_RESIDENT_SHELL_CLIENT
#define PORTABLE_RESIDENT_POLICY
#else
#define FILE_BROWSER_FULL_PROFILE
#define PORTABLE_LOW_BATTERY
#define PORTABLE_CONTEXTS_CLIENT
#define PORTABLE_CHECKED_STORAGE_RUNTIME
#endif
#define PORTABLE_BLE_BROADCAST
#define PORTABLE_TEXT_INPUT_POLICY
#include "file_browser_shared_input_test.c"
#ifdef PORTABLE_CONTEXTS_CLIENT
static unsigned policy_message_input_calls;
static void policy_message_yield(uint32_t n){test_yield(n);test_steps++;}
static void policy_message_input(int*x,int*y){assert(++policy_message_input_calls<100);if(policy_message_input_calls>=6&&policy_message_input_calls<=8){*x=40;*y=100;}}
#endif
int main(int argc,char**argv){
 assert(argc==2);const char*scenario=argv[1];peer_backend_begin();peer_backend_folders();borrowed=strcmp(scenario,"remote-owned")!=0;
 if(!strcmp(scenario,"missing-descriptor"))text_scene.presentation.page.components.lifecycle.base.struct_size=sizeof(risc_scene_api_v1);
 text_driver=files_text_driver_get(2);risc_provider_dependency_v1 dependency={RISC_SCENE_CAPABILITY,1,&text_scene};assert(text_driver->start(&dependency,1));
 runtime=test_runtime;runtime.acquire=text_acquire;runtime.release=text_release;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;runtime.exit_requested=policy_exit_requested;test_runtime_override=&runtime;
#ifdef FILE_POLICY_RESIDENT
 runtime.resident_shell=policy_resident_bind;
#endif
 const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());strcpy(fb_path,"/Downloads");assert(fb_load(0));
 #ifndef FILE_POLICY_RESIDENT
 quick.idle_ms=60000u;
#endif
 for(unsigned i=0;i<3;i++){t5_app_input_t input={0};assert(t5_app_get_api(1)->poll(&input,1));}
 policy_battery_reads=0;
#ifndef FILE_POLICY_RESIDENT
 low_battery_sampled=false;
#endif
 bool remote=!strncmp(scenario,"remote",6);
 if(remote){fb_list_choice=0;fb_option(7);peer_until(PEER_DISCOVERING);peer_step();peer_frame();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);peer_frame();fpeer_choice=1;fpeer_action();peer_until(PEER_READY);assert(fbm_mounted&&fb_storage_index==2);}
 fb_draw();
 policy_scene_missing=!strcmp(scenario,"missing");
 fb_mode=FB_OPTIONS;fb_option(0);if(!fb_text_active)fprintf(stderr,"open failed status=%s retained=%u adapter=%u mode=%u grants=%u\n",fb_status,fb_text.retained,portable_text_adapter_retained(),fb_mode,test_grants);assert(fb_text_active);
 if(policy_scene_missing||!strcmp(scenario,"missing-descriptor")){text_step();assert(!fb_text_active&&!text_grants&&!retains);text_clean();puts("PASS missing checkpoint clean close");return 0;}
 text_replace("local");assert(!portable_file_browser_safe());policy_battery_reads=policy_pauses=policy_resumes=policy_background=0;
#ifdef PORTABLE_CONTEXTS_CLIENT
 policy_contexts=0;
#endif
#ifdef FILE_POLICY_RESIDENT
 policy_resident_polls=policy_resident_dispatches=0;text_policy_poll_at=test_ticks;
#else
 low_battery_sampled=true;low_battery_sampled_at=test_ticks;last_activity=test_ticks;
#endif
#ifndef FILE_POLICY_RESIDENT
 if(!strcmp(scenario,"remote-low")){policy_percent=9;test_ticks+=5000;for(unsigned i=0;i<30000&&(fsh_phase!=FSH_OFF||fb_text_active);i++)text_step();assert(!fb_text_active&&!fbm_mounted&&!text_grants&&!strcmp(fb_query,"")&&low_battery_transition_pending);t5_app_input_t input={0};assert(t5_app_get_api(1)->poll(&input,1));assert(low_battery.observed&&low_battery.low&&!low_battery_transition_pending);text_clean();puts("PASS low battery closes sharing and cancels invalidated draft");return 0;}
#endif
 if(!strcmp(scenario,"retained")){policy_resume_retained=true;test_ticks+=5000;text_step();assert(fb_text.retained&&retains==1&&policy_resumes==1);unsigned before=calls;for(unsigned i=0;i<10;i++)fb_text_step();assert(calls==before&&policy_resumes==1);puts("PASS resume retained no redispatch");return 0;}
 if(!strcmp(scenario,"paused-home")){policy_scene_again=true;test_ticks+=5000;text_step();assert(fb_text_checkpoint.pausing);policy_exit=true;policy_scene_again=false;text_step();assert(!fb_text_active&&!text_grants&&!policy_resumes&&!retains);text_clean();puts("PASS paused Runtime exit");return 0;}
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!strcmp(scenario,"context-message")){strcpy(context_message,"Fixture workflow message");context_message_open=true;test_touch_script=policy_message_input;runtime.yield_ms=policy_message_yield;text_step();runtime.yield_ms=test_yield;test_touch_script=NULL;assert(!context_message_open&&policy_resumes==1&&!file_policy_sleep_count&&policy_message_input_calls>=3);}
 else
#endif
 if(!strcmp(scenario,"busy-frame")){text_presenting=true;test_ticks+=65000;for(unsigned i=0;i<5;i++)text_step();assert(!policy_battery_reads&&!file_policy_sleep_count&&!policy_pauses);text_presenting=false;}
 else if(!strcmp(scenario,"owned-handle")){fb_owned[0].handle=99;test_ticks+=65000;for(unsigned i=0;i<5;i++)text_step();assert(!policy_battery_reads&&!file_policy_sleep_count&&!policy_pauses);fb_owned[0].handle=0;}
 else if(!strcmp(scenario,"remote-expiry")){test_ticks+=300001;for(unsigned i=0;i<30000&&(fsh_phase!=FSH_OFF||fb_text_active);i++)text_step();assert(!fbm_mounted&&!fb_text_active&&fb_storage_index==0&&!text_grants);text_clean();puts("PASS policy draft invalidated after remote expiry");return 0;}
 else {for(unsigned i=0;i<6500;i++){test_ticks+=9;text_step();}
#ifdef FILE_POLICY_RESIDENT
 assert(!policy_battery_reads&&!file_policy_sleep_count);assert(policy_resident_polls==(!strcmp(scenario,"remote-owned")?0u:130u));assert(policy_resident_dispatches==(remote?0u:130u));
#else
 assert(policy_battery_reads==13);assert(file_policy_sleep_count==(remote?0u:1u));
 assert(remote?policy_background==0:policy_background>6000);
 assert(remote?policy_contexts==0:policy_contexts>6000);
#endif
 }
 if(!strcmp(scenario,"busy-frame")||!strcmp(scenario,"owned-handle")){text_step();
#ifdef FILE_POLICY_RESIDENT
 assert(policy_resident_polls==1&&policy_resident_dispatches==1);
#else
 assert(policy_battery_reads==1&&file_policy_sleep_count==1);
#endif
 }
 assert(fb_text_active&&!fb_text.retained&&!fb_text_policy_boundary&&!strcmp(fb_text_state.text,"local"));
 if(remote)text_finish(RISC_SCENE_KEY_CANCEL);else{text_finish(RISC_SCENE_KEY_DONE);assert(!strcmp(fb_query,"local")&&fb_count==1);}
 unsigned reads=policy_battery_reads,sleeps=file_policy_sleep_count; text_clean();printf("PASS %s: battery=%u sleep=%u background=%u draft and cleanup preserved\n",scenario,reads,sleeps,policy_background);return 0;
}
