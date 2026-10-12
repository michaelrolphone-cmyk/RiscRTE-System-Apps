#define PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
#define FILE_PEER_FIXTURE
#define FILE_SETUP_TEST
#include "file_browser_sharing_test.c"
extern void peer_backend_begin(void),peer_backend_tick(uint64_t),peer_backend_end(void),peer_backend_direct(void),peer_backend_local_state(int);
extern void peer_backend_drop(bool);
extern const void*peer_backend_capability(const char*);
extern bool peer_backend_local_exists(const char*),peer_backend_remote_exists(const char*);
static bool source_available=true,source_full,source_write_busy,source_begin_busy,source_unsupported,source_active,source_finished,source_launched,source_multi,source_short;
static uint64_t source_serial,source_live,source_expected;
static unsigned source_length,source_begins,source_writes,source_discards,source_opens;
static unsigned char source_bytes[8192];
static const char*source_scenario;
static uint32_t source_count(const char*name){assert(!strchr(name,'/'));return strstr(name,".obj")||strstr(name,".txt")?(source_multi?3:1):0;}
static bool source_get(const char*name,uint32_t i,t5_file_handler_t*out){assert(i<source_count(name));memset(out,0,sizeof(*out));snprintf(out->app_id,sizeof(out->app_id),"declared_%u",i);snprintf(out->display_name,sizeof(out->display_name),"Declared handler %u",i);strcpy(out->icon,"text");return true;}
static bool source_supports(const char*name){return !source_unsupported&&!strcmp(name,source_multi?"declared_2":"declared_0");}
static int32_t source_begin(const char*name,uint64_t size,uint64_t*token){*token=0;assert(source_count(name));if(source_begin_busy)return RISC_FILE_SOURCE_BUSY;if(source_full)return RISC_FILE_SOURCE_NO_SPACE;assert(!source_active&&size<sizeof(source_bytes));source_active=true;source_finished=source_launched=false;source_expected=size;source_length=0;*token=source_live=++source_serial;source_begins++;return 0;}
static int32_t source_write(uint64_t token,const void*bytes,uint32_t n,uint32_t*out){*out=0;assert(source_active&&!source_finished&&token==source_live&&n&&n<=4096&&source_length+n<=source_expected);if(source_write_busy)return RISC_FILE_SOURCE_BUSY;if(source_full)return RISC_FILE_SOURCE_NO_SPACE;if(source_short&&n>3)n=3;memcpy(source_bytes+source_length,bytes,n);source_length+=n;*out=n;source_writes++;return 0;}
static int32_t source_finish(uint64_t token){assert(source_active&&token==source_live);if(source_length!=source_expected)return RISC_FILE_SOURCE_INCOMPLETE;source_finished=true;return 0;}
static int32_t source_discard(uint64_t token){assert(source_active&&token==source_live);source_active=false;source_finished=false;source_discards++;return 0;}
static bool source_open(uint64_t token,const char*handler,uint64_t cookie){assert(source_active&&source_finished&&token==source_live&&source_supports(handler)&&cookie==UINT64_C(0x4642524f57534552));assert(fsh_phase==FSH_OFF&&!fpeer&&!owned&&!fsource_input&&!fb_owned[0].handle);source_launched=true;source_opens++;return true;}
static const risc_file_open_source_api_v1 source_api={.api_version=1,.struct_size=sizeof(source_api),.handler_count=source_count,.handler_get=source_get,.handler_supported=source_supports,.begin=source_begin,.write=source_write,.finish=source_finish,.discard=source_discard,.open_request=source_open};
const void*peer_fixture_capability(const char*n){if(!strcmp(n,RISC_FILE_OPEN_SOURCE_CAPABILITY))return source_available?&source_api:NULL;return peer_backend_capability(n);}
static void source_step(void){test_ticks++;fsh_tick();if(!fb_terminal)fsource_tick();peer_backend_tick(fsh_now);if(cancel_calls)quiescent=true;}
static void source_until(unsigned state){for(unsigned i=0;i<30000;i++){source_step();if(fpeer_status.state==state)return;assert(!fb_terminal);}assert(!"source peer timeout");}
static void source_frame(void){for(unsigned i=0;i<20;i++){if(fb_dirty)fb_draw();test_ticks++;t5_app_input_t in={0};assert(fb_app->poll(&in,1));if(portable_file_browser_setup_frame_ready()&&!fb_dirty)return;}assert(!"source frame timeout");}
static void source_wait(void){for(unsigned i=0;i<30000&&!fb_terminal&&fsource_state!=FSOURCE_NONE;i++)source_step();assert(fb_terminal||fsource_state==FSOURCE_NONE);}
static void source_actions(const char*name){for(unsigned i=0;i<fb_count;i++)if(!strcmp(fb_rows[i].name,name)){fb_choice=i;fbx_actions();assert(fb_mode==FB_ACTIONS);return;}assert(!"selected file missing");}
int main(int argc,char**argv){assert(argc>=2);source_scenario=argv[1];test_directory=argc>2?argv[2]:NULL;peer_backend_begin();
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;
 borrowed=true;source_short=strstr(source_scenario,"short")!=NULL;source_available=strcmp(source_scenario,"unavailable")!=0;source_full=!strcmp(source_scenario,"full");source_unsupported=!strcmp(source_scenario,"unsupported");source_multi=!strcmp(source_scenario,"choose");
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());strcpy(fb_path,"/Downloads");assert(fb_load(0));
 const bool remote=!strncmp(source_scenario,"remote",6);
 if(remote){peer_backend_direct();borrowed=false;fb_list_choice=0;fb_option(7);source_until(PEER_DISCOVERING);source_step();source_frame();fpeer_choice=0;fpeer_action();source_until(PEER_PAIRING);source_frame();fpeer_choice=1;fpeer_action();source_until(PEER_READY);assert(fbm_mounted&&fb_mode==FB_LIST&&fb_storage_index==2);source_actions("incoming.obj");}
 else source_actions("local.txt");
 fb_choice=source_multi?1:0;fbx_activate();
 if(!strcmp(source_scenario,"unavailable")){assert(fsource_state==FSOURCE_NONE&&strstr(fb_status,"unavailable")&&!source_begins);}
 else if(source_unsupported){assert(fb_mode==FB_NOTICE&&!source_begins&&strstr(fb_status,"cannot read"));assert(!fb_back());}
 else{
  if(source_multi){assert(fb_mode==FB_HANDLERS&&fb_handler_count==3);fb_handler_offset=2;assert(fbx_handler_page());assert(!strcmp(fb_handlers[0].app_id,"declared_2"));fbx_dispatch(0);}
  assert(fsource_state==FSOURCE_START);
  if(!strcmp(source_scenario,"begin-busy")){source_begin_busy=true;for(unsigned i=0;i<100;i++)source_step();assert(fsource_state==FSOURCE_START&&!source_begins&&!source_opens);source_begin_busy=false;}
  if(!strcmp(source_scenario,"cancel")){source_step();assert(source_active);assert(!fb_back()&&fsource_state==FSOURCE_CANCEL);source_wait();assert(!source_active&&!source_opens&&strstr(fb_status,"cancelled"));}
  else if(!strcmp(source_scenario,"busy-cancel")){source_step();source_write_busy=true;for(unsigned i=0;i<100;i++)source_step();assert(fsource_state==FSOURCE_LOCAL&&!source_writes);assert(!fb_back());source_write_busy=false;source_wait();assert(!source_active&&!source_opens);}
  else if(!strcmp(source_scenario,"remote-cancel")){source_step();assert(fsource_state==FSOURCE_REMOTE&&fbm_operation==FBM_SOURCE);assert(!fb_back());source_wait();assert(!source_active&&!source_opens&&fbm_mounted);}
  else if(!strcmp(source_scenario,"remote-interrupted")||!strcmp(source_scenario,"remote-reopen")){source_step();assert(fsource_state==FSOURCE_REMOTE);if(!strcmp(source_scenario,"remote-reopen")){assert(!fb_back());}else peer_backend_drop(true);source_wait();peer_backend_drop(false);assert(!source_active&&!source_opens&&fbm_mounted);if(!strcmp(source_scenario,"remote-reopen")){assert(!fb_back()&&fb_mode==FB_ACTIONS);fb_choice=0;fbx_activate();source_wait();assert(fb_terminal&&source_opens==1&&source_launched&&source_active&&source_length==source_expected);assert(!strcmp((const char*)source_bytes,"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));assert(source_discard(source_live)==0);}}
  else if(!strcmp(source_scenario,"remote-full")){source_step();source_full=true;source_wait();assert(!source_active&&!source_opens&&fbm_mounted&&strstr(fb_status,"temporary storage"));}
  else {source_wait();if(source_full)assert(!source_opens&&!source_active&&strstr(fb_status,"temporary storage"));else {assert(fb_terminal&&source_opens==1&&source_launched&&source_active&&source_length==source_expected);assert(!fpeer&&!owned&&!fb_grant.api&&!fsource_grant.api);const char*expected=remote?"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n":"local upload";assert(source_length==strlen(expected)&&!memcmp(source_bytes,expected,source_length));assert(source_discard(source_live)==0);}}
 }
 if(!fb_terminal){(void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)source_step();assert(portable_file_browser_close());}
 assert(peer_backend_local_exists("/Downloads/local.txt")&&!peer_backend_local_exists("/Downloads/incoming.obj")&&peer_backend_remote_exists("/incoming.obj"));
 app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains&&!source_active);peer_backend_end();printf("General source Files controller %s: existing handler/action UI, bounded staging and checked cleanup PASS\n",source_scenario);return 0;
}
