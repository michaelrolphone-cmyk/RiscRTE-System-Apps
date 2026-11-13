/* Actual Files root/background and production scene provider. Runtime and
 * hardware endpoints are explicit doubles; no target instructions execute. */
#define main inherited_scene_main
#define risc_runtime_get_api inherited_scene_runtime
#include "../scene/host_test.c"
#undef risc_runtime_get_api
#undef main
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t);
#include "../../Apps/files_scene.c"
static risc_runtime_api_v1 root_runtime;
static unsigned app_grants,app_releases,yields,stage;
static bool volume_present,modal_case,contexts_case,held_modal_case;
#include "ContextsServiceV1.h"
static unsigned context_steps,context_captures,context_pauses;
static bool context_step(void*c,const contexts_policy_v1*p){(void)c;assert(p->enabled&&p->audio_allowed&&p->radio_allowed&&p->awake);++context_steps;return true;}
static bool context_pause(void*c){(void)c;++context_pauses;return true;}
static bool context_status(void*c,contexts_status_v1*out){(void)c;*out=(contexts_status_v1){.struct_size=sizeof(*out)};return true;}
static bool context_capture(void*c){(void)c;++context_captures;return true;}
static bool context_claim(void*c,uint32_t source,uint32_t slot,const char*name,uint32_t generation){(void)c;(void)source;(void)slot;(void)name;(void)generation;assert(0);return false;}
static bool context_result(void*c,uint32_t source,uint32_t generation,uint32_t result){(void)c;(void)source;(void)generation;(void)result;assert(0);return false;}
static const contexts_service_v1 context_service={.api_version=1,.struct_size=sizeof(context_service),.step=context_step,.pause=context_pause,.status=context_status,.capture_audio=context_capture,.claim_preset=context_claim,.preset_result=context_result};
static int32_t context_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;*n=0;if(!strcmp(k,"contexts_on")){assert(cap>=4);memcpy(b,(uint8_t[]){0x43,1,1,0xa4},4);*n=4;return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t context_put(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;assert(0);return -1;}
static const risc_key_value_v1 context_preferences={1,sizeof(context_preferences),NULL,context_get,context_put};
static bool root_ready(void*c){(void)c;return false;}
static risc_storage_file_t root_read_open(void*c,const char*p,uint64_t*n){(void)c;(void)p;(void)n;assert(0);return 0;}
static size_t root_read(void*c,risc_storage_file_t f,void*b,size_t n){(void)c;(void)f;(void)b;(void)n;assert(0);return 0;}
static bool root_file_close(void*c,risc_storage_file_t f,bool commit){(void)c;(void)f;(void)commit;assert(0);return false;}
static risc_storage_volume_api_v1 volume;
static bool root_label(void*c,char*out,size_t cap){(void)c;assert(cap>7);strcpy(out,"Volume");return true;}
static bool root_error(void*c,char*out,size_t cap){(void)c;assert(cap>13);strcpy(out,"Media absent");return true;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1&&native_alive?&root_runtime:NULL;}
static bool root_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 assert(!files_retained&&g->struct_size==sizeof(*g));const void*p=NULL;
 if(!strcmp(n,"ui.scene")){assert(version==1&&!instance);p=api;}
 if(volume_present&&!strcmp(n,"storage.volume")){assert(version==1&&!instance);p=&volume;}
 if(contexts_case&&!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)){assert(version==1&&!instance);p=&context_service;}
 if(contexts_case&&!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(version==1&&instance==1);p=&context_preferences;}
 if(!p)return false;
 *g=(risc_runtime_capability_v1){sizeof(*g),++app_grants,1,p};return true;
}
static bool root_release(risc_runtime_capability_v1*g){assert(!files_retained&&g->api&&g->slot&&g->generation);++app_releases;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool root_time(uint32_t*out){*out=(uint32_t)ms;return true;}
static bool root_health(risc_runtime_health_v1*out){out->uptime_ms=(uint32_t)ms;return true;}
static bool root_retain(void){assert(0&&"clean composition must not retain");return true;}
static void root_yield(uint32_t n){
 ms+=n;assert(++yields<4000);
 if(held_modal_case&&stage==2&&nav_buttons){nav_released=nav_buttons;nav_buttons=0;}
 if(submits&&!presenting&&!held&&files_ui.token&&(!contexts_case||context_captures>20)){
  if(modal_case&&stage==0){const char message[33]="Preserved workflow message";strcpy(fb_editor,"unfinished draft");assert(files_scene_background_message(message));assert(!files_scene_background_message(message));stage=1;}
  else if(modal_case&&stage==1&&files_message_pending&&files_ui.committed.revision==files_message_revision){nav_pressed=RISC_NAV_BACK;if(held_modal_case)nav_buttons=RISC_NAV_BACK;stage=2;}
  else if((!modal_case||stage==2)&&!files_message_pending){nav_pressed=RISC_NAV_HOME;stage=3;}
 }
}
int main(int argc,char**argv){
 assert(argc==3);
 if(!strcmp(argv[2],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,90,0,0};}
 else if(!strcmp(argv[2],"gray")){w=480;h=800;format=3;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,0,0,0};}
 else {assert(!strcmp(argv[2],"watch"));}
 volume_present=!strcmp(argv[1],"volume-unready");modal_case=strstr(argv[1],"message")!=NULL;contexts_case=strstr(argv[1],"contexts")!=NULL;held_modal_case=!strcmp(argv[1],"message-held");
 volume=fbm_volume;volume.refresh=volume.ready=root_ready;volume.label=root_label;volume.last_error=root_error;volume.file_open_read=root_read_open;volume.file_read=root_read;volume.file_close=root_file_close;
 root_runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(root_runtime),.acquire=root_acquire,.release=root_release,.monotonic_ms=root_time,.health=root_health,.yield_ms=root_yield,.retain_invocation=root_retain};
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;expected_queue=RISC_DISPLAY_QUEUE_FIFO;
 driver=t5_driver_get(2);assert(driver);api=driver->capability;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));
 app_main();
 if(!strcmp(argv[1],"reentry")){assert(files_clean&&stage==3);stage=0;app_main();}
 assert(files_clean&&!files_retained&&!files_ui.token&&!files_text_suspended&&!held&&!presenting&&!subscribed&&app_grants==app_releases&&submits&&stage==3);
 if(volume_present)assert(fb_storage_index==2);
 if(modal_case)assert(!strcmp(fb_editor,"unfinished draft"));
 if(contexts_case)assert(context_steps&&context_captures>20&&context_pauses);
 assert(driver->quiesce());driver->stop();
 printf("Files full root + production scene: %s/%s PASS (%u grants, %u frames)\n",argv[1],argv[2],app_grants,submits);
}
