#define main ordinary_clock_fixture_main
#define risc_runtime_get_api ordinary_fixture_runtime
#include "paper_clock_test.c"
#undef main
#undef risc_runtime_get_api
#include "PortableDeskClockApp.h"
#include "PortableAppSleep.h"
#ifdef PORTABLE_QUICK_RADIOS
#include "PortableQuickRadios.h"
#endif
#include "PortableDeskClockSettings.h"
#include "RiscInputNavigationV1.h"
#include <setjmp.h>
#include <time.h>
static jmp_buf entered;
static unsigned test,reset_calls,subscribe_calls,prepares,resumes,stages,seeds,checked_presents,face,hook_calls,prepared_reads,mode=PORTABLE_SLEEP_DEEP;
static bool in_main,terminal,prepared,has_record,seeded,failed_present;
static uint32_t epoch=1791331197u;
static portable_desk_record record;
static uint8_t physical[sizeof(pixels)];
static uint32_t base_ms;
static const char *state_path;
static const void *test_touch_address;
static bool radio_uncertain,config_changed;
static unsigned reader_flip,reader_language;
static bool test_health(risc_runtime_health_v1 *h){assert(!terminal);h->uptime_ms=ms;return polls<60;}
static void test_yield(uint32_t n){assert(!terminal&&!prepared);ms+=n;}
static bool test_rtc(void *c,twatch_rtc_time_v1 *out){
 (void)c;assert(!terminal);if(test==8)return false;
 if(prepared)prepared_reads++;
 ms+=(test==27&&prepared)?200:2;time_t seconds=(time_t)epoch+(test==9?0:(ms-base_ms)/1000u);
 if(prepared&&prepared_reads==1){if(test==25)seconds+=5;if(test==26)seconds-=5;if(test==34)seconds+=1;if(test==36)seconds-=1;}
 struct tm *t=gmtime(&seconds);assert(t);
 *out=(twatch_rtc_time_v1){(uint16_t)(t->tm_year+1900),(uint8_t)(t->tm_mon+1),
 (uint8_t)t->tm_mday,(uint8_t)t->tm_wday,(uint8_t)t->tm_hour,(uint8_t)t->tm_min,(uint8_t)t->tm_sec};
 return true;
}
static bool test_seed(void *c,risc_display_frame_v1 f){
 (void)c;assert(!terminal&&!subs&&f==1&&frames&&has_record);seeds++;
 assert(!memcmp(pixels,physical,sizeof(pixels)));seeded=true;
 return test!=14;
}
static bool test_acquire_frame(void *c,uint32_t format,risc_display_surface_v1 *out) {
 bool ok=acquire_frame(c,format,out);
 if(!subs&&test==37)return false; /* Ownership happened before uncertain leave. */
 if(!subs&&test==38)out->width++;
 return ok;
}
static void test_release_frame(void *c,risc_display_frame_v1 frame) {
 seeded=false;release_frame(c,frame);
}
static bool test_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
 const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){
 (void)c;assert(!terminal&&f==1&&frames&&!prepared);
 if(test==11&&!subs){failed_present=true;return false;}
 if(n)assert(r&&r->width&&r->height&&(unsigned)r->x+r->width<=PANEL_WIDTH&&(unsigned)r->y+r->height<=PANEL_HEIGHT);
 if(!subs){
  assert(o->intent==((has_record&&record.refresh_modulo&&!config_changed&&test!=14&&test!=29&&!resumes)?RISC_DISPLAY_PRESENT_QUALITY:RISC_DISPLAY_PRESENT_CLEAN));
  if(has_record&&record.refresh_modulo&&!config_changed&&test!=14&&test!=29&&!resumes)assert(seeded);
 }
 frames=0;*token=++presents;return true;
}
static bool test_wait(void *c,risc_display_present_token_v1 token,uint32_t timeout,risc_display_present_status_v1 *out){
 (void)c;assert(!terminal&&token==presents&&timeout);ms+=350;
 if(test==10&&!subs){failed_present=true;return false;}
 if(test==12&&!subs){failed_present=true;out->state=RISC_DISPLAY_PRESENT_FAILED;return true;}
 if(test==33&&!subs){failed_present=true;ms+=timeout;out->state=RISC_DISPLAY_PRESENT_QUEUED;return true;}
 checked_presents++;out->state=RISC_DISPLAY_PRESENT_COMPLETE;memcpy(physical,pixels,sizeof(pixels));
 const char *capture=getenv("PAPER_FRAME");
 if(capture){FILE *f=fopen(capture,"wb");assert(f);fprintf(f,"P4\n%u %u\n",PANEL_WIDTH,PANEL_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);}
 return true;
}
static bool test_nav_poll(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;assert(!terminal);memset(out,0,sizeof(*out));
 if(test!=1&&test!=2&&test!=3&&test!=15&&test!=16&&test!=17&&polls==6)out->pressed=out->released=RISC_NAV_HOME;
 if(test==15&&polls<8)out->buttons=RISC_NAV_HOME;
 return true;
}
static bool test_foreground(void *c,const risc_input_foreground_v1 *f,size_t n){(void)c;(void)f;(void)n;return true;}
static bool test_reset(void *c){(void)c;reset_calls++;return !((test==41&&reset_calls==2)||(test==43&&reset_calls==3));}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,test_nav_poll,test_foreground,test_reset};
static int32_t test_get(void*c,const char *key,void *out,uint32_t cap,uint32_t *size){
 (void)c;assert(!terminal);uint8_t value[4];
 if(!strcmp(key,"time_format")){value[0]=0x54;value[2]=test==18?0:1;}
 else if(!strcmp(key,"desk_clock_face")){value[0]=0x46;value[2]=(uint8_t)face;}
 else if(!strcmp(key,PORTABLE_READER_FLIP_KEY)){value[0]=0x52;value[2]=(uint8_t)reader_flip;}
 else if(!strcmp(key,PORTABLE_READER_LANGUAGE_KEY)){value[0]=0x4c;value[2]=(uint8_t)reader_language;}
 else if(!strcmp(key,"sleep_mode")){value[0]=0x53;value[2]=(uint8_t)mode;}
 else {*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
 assert(cap>=4);value[1]=1;value[3]=value[2]^0xa5u;memcpy(out,value,4);*size=4;return RISC_KEY_VALUE_OK;
}
static int32_t test_put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;assert(!"No settings writes");return -1;}
static bool test_snapshot(void *context,risc_touch_snapshot_v1 *out) {
 if(test!=30)return snapshot(context,out);
 memset(out,0,sizeof(*out));out->width=480;out->height=800;
 if(polls>=2&&polls<=6){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=polls==6?350:240,.y=300};}
 return true;
}
static uint64_t test_subscribe(void *c){subscribe_calls++;if(test==42&&subscribe_calls==2)return 0;return subscribe(c);}
static bool test_unsubscribe(void *c,uint64_t token){if(test==39)return false;return unsubscribe(c,token);}
#ifdef PORTABLE_QUICK_RADIOS
static bool radio_off(void *c){(void)c;assert(!radio_uncertain);return true;}
static wifi_link_t radio_status(void *c){(void)c;assert(!radio_uncertain);return WIFI_LINK_DOWN;}
static bool bluetooth_on;
static bool radio_bt_enable(void *c,bool enabled){(void)c;assert(!radio_uncertain);if(test==44){radio_uncertain=true;return false;}bluetooth_on=enabled;return true;}
static bool radio_bt_status(void *c,uint8_t *out){(void)c;assert(!radio_uncertain);*out=bluetooth_on?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;}
static const wifi_api_v1 test_wifi={.api_version=1,.struct_size=sizeof(test_wifi),.status=radio_status,.disconnect_checked=radio_off};
static const portable_bluetooth_control_v1 test_bt={.api_version=1,.struct_size=sizeof(test_bt),.set_enabled=radio_bt_enable,.status=radio_bt_status};
#endif
static bool test_obtain(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant){
 assert(!terminal&&!radio_uncertain);
#ifdef PORTABLE_QUICK_RADIOS
 if(!strcmp(name,"net.wifi")){grant->api=&test_wifi;grants++;return true;}
 if(!strcmp(name,"bluetooth.hci")){grant->api=&test_bt;grants++;return true;}
#endif
 if(!strcmp(name,"display.output")){
  static risc_display_output_api_v1_history panel;panel.base=d;panel.base.struct_size=sizeof(panel);
  panel.base.submit=test_submit;panel.base.wait_present=test_wait;panel.base.release=test_release_frame;panel.base.acquire=test_acquire_frame;
  panel.extension_tag=RISC_DISPLAY_HISTORY_TAG;panel.extension_version=1;panel.seed_previous=test_seed;
  grant->api=&panel;grants++;return true;
 }
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 touch;touch=t;touch.snapshot=test_snapshot;touch.subscribe=test_subscribe;touch.unsubscribe=test_unsubscribe;grant->api=&touch;test_touch_address=&touch;grants++;return true;}
 if(!strcmp(name,"rtc.clock")){static twatch_rtc_api_v1 clock;clock=rtc_api;clock.read=test_rtc;if(test==19)return false;grant->api=&clock;grants++;return true;}
 if(!strcmp(name,"input.navigation")){grant->api=&navigation;grants++;return true;}
 if(!strcmp(name,"storage.key-value")){static risc_key_value_v1 preferences;preferences=kv;preferences.get=test_get;preferences.put=test_put;grant->api=&preferences;grants++;return true;}
 return acquire(name,version,id,grant);
}
static bool test_drop(risc_runtime_capability_v1 *grant){
 assert(!terminal&&!radio_uncertain);
 if(test==40&&grant->api==test_touch_address&&!subs)return false;
#ifdef PORTABLE_QUICK_RADIOS
 if((test==45&&grant->api==&test_bt)||(test==46&&grant->api==&test_wifi)){radio_uncertain=true;return false;}
#endif
 return release(grant);
}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=test_health,.yield_ms=test_yield,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=test_obtain,.release=test_drop};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&runtime:NULL;}
int portable_desk_clock_boot_read(const risc_runtime_api_v1 *runtime_arg,portable_desk_record *out){
 assert(in_main&&runtime_arg==&runtime&&!presents);if(test==17)return -2;
 if(has_record&&test!=2&&test!=3&&test!=15){*out=record;return 1;}return 0;
}
static int cancelled(void *c){(void)c;assert(!terminal);if(test==24&&frames)return -2;return test==4 || (test==5&&prepared);}
static int stage_record(void *c,const portable_desk_record *value) {
 (void)c;assert(!terminal&&!prepared&&!frames&&!subs);assert(portable_desk_record_valid(value));return 1;
}
static int prepare_desk(void *c){(void)c;assert(!terminal&&!subs&&!frames&&!prepared);prepares++;if(test==6||test==30)return 0;if(test==7)return -2;prepared=true;if(test==20||(test==28&&prepares==1))ms+=61000;if(test==34||test==36)ms+=200;if(test==35)ms+=990;return 1;}
static int resume_desk(void *c){(void)c;assert(!terminal&&prepared);resumes++;prepared=false;return test==21?-2:1;}
static int stage_desk(void *c,const portable_desk_record *value,uint32_t duration,uint32_t sampled){
 (void)c;assert(!terminal&&prepared&&!subs&&!frames);assert(duration&&duration<=60000&&ms-sampled<100);stages++;
 assert(value->has_image&&value->displayed_minute==(int64_t)(epoch+(ms-base_ms)/1000u)-(epoch+(ms-base_ms)/1000u)%60u);
 if(test==21||test==22||test==42||test==43)return 0;
 if(test==23)return -2;
 record=*value;
 if(state_path){uint8_t bytes[80];assert(portable_desk_encode(value,bytes,sizeof(bytes)));FILE *file=fopen(state_path,"wb");assert(file);assert(fwrite(bytes,1,sizeof(bytes),file)==sizeof(bytes));assert(fwrite(physical,1,sizeof(physical),file)==sizeof(physical));fclose(file);}
 terminal=true;longjmp(entered,1);
}
int portable_app_alarm_sleep(const risc_runtime_api_v1 *runtime_arg,const risc_display_output_api_v1 *display,
 const risc_battery_gauge_api_v1 *gauge,const alarm_service_v1 *alarms){
 (void)display;(void)gauge;(void)alarms;assert(runtime_arg==&runtime&&!subs&&!frames);
 if(test==29&&hook_calls++==0){portable_desk_clock_refused();return 0;}
 portable_desk_sleep_ops ops={NULL,cancelled,stage_record,prepare_desk,resume_desk,stage_desk};
 return portable_desk_clock_run(runtime_arg,&ops);
}
int main(int argc,char **argv){
 assert(argc>=2);test=(unsigned)atoi(argv[1]);face=argc>3?(unsigned)atoi(argv[3]):0;
 reader_flip=getenv("READER_FLIP")?(unsigned)atoi(getenv("READER_FLIP")):0;
 reader_language=getenv("READER_LANGUAGE")?(unsigned)atoi(getenv("READER_LANGUAGE")):0;
 if(getenv("READER_EPOCH"))epoch=(uint32_t)strtoul(getenv("READER_EPOCH"),NULL,10);
 if(argc>2&&strcmp(argv[2],"-")){state_path=argv[2];FILE *file=fopen(state_path,"rb");if(file){uint8_t bytes[80];assert(fread(bytes,1,sizeof(bytes),file)==sizeof(bytes));has_record=portable_desk_decode(bytes,sizeof(bytes),&record);assert(has_record);assert(fread(physical,1,sizeof(physical),file)==sizeof(physical));fclose(file);epoch=(uint32_t)record.displayed_minute+(test==32?10u:60u);if(test==13)face=(record.config.face+1u)%6u;}}
 config_changed=has_record&&(record.config.face!=face || record.config.flip_ui!=reader_flip || record.config.language!=reader_language);
 if(test==16)mode=PORTABLE_SLEEP_LIGHT;
 assert(app_module_init()==0);assert(!presents);in_main=true;base_ms=ms;
 if(!setjmp(entered)){app_main();if(portable_app_sleep_retained()){unsigned held=grants;app_module_fini();assert(grants==held&&subs==((test==17||test==39||test==43||test>=44)?1u:0u)&&frames==((test==24||test==14||test==11||test==37||test==38)?1u:0u));assert(test==7||test==10||test==11||test==12||test==14||test==17||test==21||test==23||test==24||test==33||test==37||test==38||(test>=39&&test<=46));puts("Clock retained safely");return 0;}app_module_fini();assert(!grants&&!subs&&!frames);assert(!launches);assert(!terminal);if(test==10||test==11||test==12)assert(failed_present&&!stages);if(test==20)assert(prepares>=1&&prepares<=3&&resumes==prepares&&presents==5&&!stages);if(test==22)assert(stages==1&&resumes==1);}
 else {assert(record.config.flip_ui==reader_flip&&record.config.language==reader_language);if(config_changed)assert(!seeds);assert(terminal&&!subs&&!frames&&prepared&&grants&&stages==1);assert(checked_presents||seeds);if(test==32)assert(!presents&&seeds==1);}
 printf("Desk Clock %u: %u presents, %u seeds, %u preparations, %u resumes, %u stages\n",test,presents,seeds,prepares,resumes,stages);return 0;
}
