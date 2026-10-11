/* Actual Springboard + production adapter/client; deterministic providers and
 * resident dispatch boundary. Runtime/ELF admission has its separate test. */
#define TEST_NATIVE_SOURCE_NO_MAIN
#define TEST_CUSTOM_CATALOG
#include "portable_native_time_source_test.c"
#include "logical_touch_queue_fixture.h"
const t5_app_manifest_t portable_catalog[1]={{0}};
const unsigned portable_catalog_count=0;
extern void app_main(void);
static unsigned latency,submitted,steps,refreshes,cancels,storage_live,launches,controls;
static unsigned edge_buttons,cancel_started;
static bool panel,controls_case,refused_case,home_case;
static uint8_t immutable[sizeof(pixels)];
static int cache_refresh(void*c){(void)c;++refreshes;return INSTALLED_APPS_OK;}
static uint32_t cache_generation(void*c){(void)c;return 1;}
static uint32_t cache_count(void*c){(void)c;return 1;}
static bool cache_get(void*c,uint32_t i,installed_app_v1*out){(void)c;assert(i==0);*out=(installed_app_v1){.available=true};strcpy(out->id,"new-app");strcpy(out->name,"New SD App");strcpy(out->icon,"solid:f017");strcpy(out->launch_path,"Apps/new-app/new.elf");return true;}
static int cache_step(void*c,const risc_storage_volume_api_v1*v){(void)c;(void)v;assert(presents&&storage_live);++steps;return INSTALLED_APPS_WORKING;}
static int cache_cancel(void*c){(void)c;++cancels;if(!storage_live)return INSTALLED_APPS_OK;if(!cancel_started)cancel_started=ticks;return ticks-cancel_started<600?INSTALLED_APPS_WORKING:INSTALLED_APPS_OK;}
static const installed_apps_v1 cache={1,sizeof(cache),NULL,cache_refresh,cache_generation,cache_count,cache_get,cache_step,cache_cancel,NULL};
static const risc_storage_volume_api_v1 sd={.api_version=1,.struct_size=sizeof(sd)};
static bool display_info(void*c,risc_display_info_v1*out){bool ok=fx_info(c,out);out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool display_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){assert(!panel);bool ok=fx_submit(c,f,r,n,o,t);panel=true;submitted=ticks;memcpy(immutable,pixels,sizeof(pixels));return ok;}
static bool display_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){(void)c;assert(panel&&t==presents&&!memcmp(immutable,pixels,sizeof(pixels)));out->state=ticks-submitted<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)panel=false;return true;}
static bool nav(void*c,risc_input_navigation_frame_v1*out){(void)c;assert(ticks<20000);unsigned b=0;if(!controls_case&&!home_case&&ticks>=1100&&ticks<1160)b=RISC_NAV_CONFIRM;if((controls_case||home_case||refused_case)&&ticks>=6000&&ticks<6060)b=RISC_NAV_HOME;*out=(risc_input_navigation_frame_v1){.buttons=b,.pressed=b&~edge_buttons,.released=edge_buttons&~b};edge_buttons=b;return true;}
static bool focus(void*c,const risc_input_foreground_v1*q,size_t n){(void)c;(void)q;assert(n<=1);return true;}
static bool reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 navigation_fixture={1,sizeof(navigation_fixture),NULL,nav,focus,reset};
static bool touch_script(void*c,risc_touch_snapshot_v1*out){(void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};if(controls_case&&ticks>=100&&ticks<400){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=100,.y=(uint16_t)(10+(ticks-100)/2)};}return true;}
static int32_t installed_checkpoint(uint64_t token,const risc_resident_request_v1*q,risc_resident_reply_v1*out){(void)token;(void)out;assert(!storage_live&&!panel);if(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS){++controls;assert(controls==1);}return RISC_RESIDENT_OK;}
static bool resident_fixture(risc_resident_client_v1*out){*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=1,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=installed_checkpoint};return true;}
static int32_t installed_alarm_status(void*c,alarm_status_v1*out){(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t installed_alarm_ok(void*c){(void)c;return ALARM_OK;}
static int32_t installed_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t installed_alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;*t=(alarm_sleep_v1){0};return ALARM_OK;}
static const alarm_service_descriptor_v2 installed_alarm={.base={2,sizeof(installed_alarm),NULL,installed_alarm_status,installed_alarm_ok,installed_alarm_ok,installed_alarm_ack,installed_alarm_sleep,installed_alarm_ok},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION};
static bool acquire(const char*name,uint32_t v,uint64_t i,risc_runtime_capability_v1*out){const void*api=NULL;if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(v==2);api=&installed_alarm;}else if(!strcmp(name,INSTALLED_APPS_CAPABILITY)){assert(v==1&&!i);api=&cache;}else if(!strcmp(name,"storage.volume")){assert(v==1&&i==9&&presents);api=&sd;++storage_live;cancel_started=0;}else if(!strcmp(name,"input.navigation"))api=&navigation_fixture;if(api){++live;++acquires;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=api};return true;}if(!source_acquire(name,v,i,out))return false;if(!strcmp(name,"display.output")){static risc_display_output_api_v1 d;d=fx_display;d.get_info=display_info;d.submit=display_submit;d.present_status=display_status;out->api=&d;}if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 t;t=fx_touch;t.poll=logical_raw_poll;t.next=logical_raw_next;t.snapshot=logical_raw_snapshot;out->api=&t;}return true;}
static bool release(risc_runtime_capability_v1*g){if(g->api==&sd){assert(storage_live&&ticks-cancel_started>=600);--storage_live;}return source_release(g);}
static bool installed_launch(const char*name){assert(!storage_live&&!panel&&!frames);++launches;if(refused_case&&launches==1){assert(!strcmp(name,"Apps/new-app/new.elf"));return false;}assert(!strcmp(name,(controls_case||home_case||refused_case)?"default.elf":"Apps/new-app/new.elf"));return true;}
int main(int argc,char**argv){assert(argc==3);latency=(unsigned)atoi(argv[2]);controls_case=!strcmp(argv[1],"controls");home_case=!strcmp(argv[1],"home");refused_case=!strcmp(argv[1],"refused");scenario="valid";fx_runtime.acquire=acquire;fx_runtime.release=release;fx_runtime.request_launch=installed_launch;fx_runtime.resident_shell=resident_fixture;logical_raw_init(480,800,touch_script);assert(app_module_init()==0);app_main();assert(steps>10&&cancels>10&&!retained&&!panel&&!storage_live);assert(launches==((controls_case||home_case)?0u:1u));if(controls_case||home_case||refused_case)assert(handoff_requested);if(controls_case){assert(controls==1&&refreshes>=2);}if(refused_case)assert(refreshes>=2);app_module_fini();assert(!live&&!frames&&!subscriptions&&!barriers);printf("Springboard installed catalog: %s panel=%u ms steps=%u refreshes=%u PASS\n",argv[1],latency,steps,refreshes);}
