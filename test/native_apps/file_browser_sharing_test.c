/* Actual Files controller + adapter + saved-profile helper + HTTP client.
 * Synthetic capability backends model explicit resources; no network I/O. */
#define PORTABLE_FILE_SHARING
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_ALARM_TERMINAL_RETENTION
#define PORTABLE_FILE_SHARING_EXPORT_INSTANCE 32
#define PORTABLE_FILE_SHARING_TCP_INSTANCE 33
#define PORTABLE_FILE_SHARING_ENTROPY_INSTANCE 34
#ifdef FILE_SETUP_TEST
#define PORTABLE_FILE_SETUP
#define PORTABLE_FILE_SHARING_SETUP_INSTANCE 35
#endif
#ifdef FILE_BROWSER_PAPER_PROFILE
#define PORTABLE_DISPLAY_ROTATION 90
#else
#define PORTABLE_FILE_BROWSER_RGB_ONLY
#endif
#define main unused_browser_fixture_main
#include "portable_file_browser_test.c"
#undef main
#include "RiscTcpConnectionV1.h"
#include "RiscEntropySourceV1.h"
static bool policy_off,policy_busy,policy_context,owned,quiescent,borrowed,empty,lease_busy,end_busy,tcp_fault;
static unsigned calls,begin_calls,cancel_calls,lease_calls,end_calls,listeners,closed_listeners,retains;
static struct {char key[16];uint8_t data[64];uint32_t size;} store[16];
static unsigned store_count;
static bool live_check(void){assert(!native_custody_retained);++calls;return true;}
static int32_t kv_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;live_check();*n=0;
 if(!strcmp(k,PORTABLE_RADIO_KEY)){
  if(policy_context)return RISC_KEY_VALUE_CONTEXT;
  if(policy_busy)return RISC_KEY_VALUE_BUSY;
#ifdef FILE_SETUP_TEST
  if(!policy_off){const uint8_t flags[4]={0x51,1,3,0xa6};assert(cap>=4);memcpy(b,flags,4);*n=4;return 0;}
#else
  if(!policy_off)return RISC_KEY_VALUE_NOT_FOUND;
#endif
  const uint8_t off[4]={0x51,1,0,0xa5};assert(cap>=4);memcpy(b,off,4);*n=4;return 0;
 }
 for(unsigned i=0;i<store_count;i++)if(!strcmp(store[i].key,k)){*n=store[i].size;if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,store[i].data,*n);return 0;}
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t kv_put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(n<=64&&store_count<16);unsigned i;for(i=0;i<store_count;i++)if(!strcmp(k,store[i].key))break;if(i==store_count)store_count++;strcpy(store[i].key,k);memcpy(store[i].data,b,n);store[i].size=n;return 0;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static wifi_link_t net_status(void*c){(void)c;live_check();return borrowed?WIFI_LINK_UP:WIFI_LINK_DOWN;}
static int8_t net_rssi(void*c){(void)c;live_check();return -40;}
static bool net_addresses(void*c,wifi_ipv4_v1*s,wifi_ipv4_v1*a){(void)c;live_check();memset(a,0,sizeof(*a));*s=(wifi_ipv4_v1){{192,0,2,4},{192,0,2,1},{255,255,255,0}};return true;}
static int32_t net_begin(void*c,const risc_radio_request_v1*r,uint32_t*id){(void)c;live_check();assert(!owned&&!borrowed&&!empty&&!strcmp(r->ssid,"fixture"));owned=true;++begin_calls;*id=11;return RISC_RADIO_ACCEPTED;}
static int32_t net_poll(void*c,uint32_t id,risc_radio_progress_v1*p){(void)c;live_check();assert(owned&&id==11);*p=(risc_radio_progress_v1){.struct_size=sizeof(*p),.operation_id=id,.phase=quiescent?RISC_RADIO_IDLE:cancel_calls?RISC_RADIO_STOPPING:RISC_RADIO_CONNECTED,.owned=!quiescent,.quiescent=quiescent,.station_state=quiescent?WIFI_LINK_DOWN:WIFI_LINK_UP,.scan={.struct_size=sizeof(garden_radio_scan_result_v1)}};if(!quiescent){p->station[0]=192;p->station[2]=2;p->station[3]=4;}else owned=false;return quiescent?RISC_RADIO_QUIESCENT:RISC_RADIO_PENDING;}
static int32_t net_cancel(void*c,uint32_t id){(void)c;live_check();assert(owned&&id==11&&!listeners);++cancel_calls;return RISC_RADIO_PENDING;}
static int32_t net_lease(void*c,uint32_t*id){(void)c;live_check();++lease_calls;*id=lease_busy?0:9;return lease_busy?RISC_RADIO_BUSY:RISC_RADIO_ACCEPTED;}
static int32_t net_end(void*c,uint32_t id){(void)c;live_check();assert(id==9);++end_calls;return end_busy?RISC_RADIO_AGAIN:RISC_RADIO_ACCEPTED;}
static const wifi_async_v1 wifi={.base={.api_version=1,.struct_size=sizeof(wifi),.status=net_status,.rssi=net_rssi,.addresses=net_addresses},.async_tag=RISC_RADIO_ASYNC_TAG,.async_version=1,.begin=net_begin,.poll=net_poll,.cancel=net_cancel,.service_begin=net_lease,.service_end=net_end};
static int32_t entropy_fill(void*c,void*out,uint32_t n){(void)c;live_check();assert(n==32);for(unsigned i=0;i<n;i++)((uint8_t*)out)[i]=(uint8_t)(i+test_ticks);return 0;}
static const risc_entropy_source_v1 entropy={1,sizeof(entropy),(void*)1,entropy_fill};
static int32_t listen_tcp(void*c,const risc_tcp_connection_listen_v1*b,uint64_t*t){(void)c;live_check();assert(b->port==8080&&b->address[3]==4&&!listeners&&!fb_grant.api);*t=7;listeners++;return 0;}
static int32_t accept_tcp(void*c,uint64_t t,uint64_t*out){(void)c;live_check();assert(t==7&&listeners==1);*out=0;return RISC_TCP_CONNECTION_WOULD_BLOCK;}
static int32_t read_tcp(void*c,uint64_t t,void*b,uint32_t cap,uint32_t*n){(void)c;(void)t;(void)b;(void)cap;(void)n;assert(false);return 0;}
static int32_t write_tcp(void*c,uint64_t t,const void*b,uint32_t cap,uint32_t*n){(void)c;(void)t;(void)b;(void)cap;(void)n;assert(false);return 0;}
static int32_t close_tcp(void*c,uint64_t t){(void)c;live_check();assert(t==7&&listeners==1);if(tcp_fault)return RISC_TCP_CONNECTION_RETAINED;listeners--;closed_listeners++;return 0;}
static const risc_tcp_connection_v1 tcp={1,sizeof(tcp),(void*)1,listen_tcp,accept_tcp,read_tcp,write_tcp,close_tcp};
static int32_t catalog(void*c,uint32_t i,risc_app_data_export_entry_v1*out){(void)c;(void)i;(void)out;assert(false);return 0;}
static risc_app_data_export_v1 export_api;
#ifdef FILE_SETUP_TEST
#include "file_browser_setup_fixture.inc"
#endif
static bool get_cap(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 live_check();if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(id==1||id==6);g->api=&kv;}
 else if(!strcmp(n,"net.wifi")){assert(id==15);g->api=&wifi;}
 else if(!strcmp(n,RISC_APP_DATA_EXPORT_CAPABILITY)){assert(id==32&&!fb_grant.api);g->api=&export_api;}
 else if(!strcmp(n,RISC_TCP_CONNECTION_CAPABILITY)){assert(id==33);g->api=&tcp;}
 else if(!strcmp(n,RISC_ENTROPY_SOURCE_CAPABILITY)){assert(id==34);g->api=&entropy;}
#ifdef FILE_SETUP_TEST
 else if(!strcmp(n,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY)){assert(id==35&&!setup_owned);g->api=&setup_api;g->slot=35;g->generation=1;}
#endif
 else return test_acquire(n,v,id,g);
 test_grants++;return true;
}
static bool release_cap(risc_runtime_capability_v1*g){
#ifdef FILE_SETUP_TEST
 if(g->api==&setup_api){assert(!setup_owned);setup_releases++;test_grants--;memset(g,0,sizeof(*g));g->struct_size=sizeof(*g);return true;}
 assert(!setup_cleanup);
#endif
 live_check();if(g->api==&wifi)assert(!owned&&!listeners);return test_release(g);}
static bool retain_owner(void){retains++;return true;}
static risc_runtime_api_v1 runtime;
static void capture_paper(const char *name){
#ifdef FILE_BROWSER_PAPER_PROFILE
 if(test_directory){char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",test_directory,name);FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P4\n800 480\n");assert(fwrite(fb_pixels,1,sizeof(fb_pixels),out)==sizeof(fb_pixels));assert(!fclose(out));}
#else
 (void)name;
#endif
}
static void step(void){test_ticks+=10;fsh_tick();}
static void until_ready(void){for(unsigned i=0;i<60&&fsh_status.state!=PORTABLE_FILE_SHARING_LISTENING;i++)step();assert(fsh_status.state==PORTABLE_FILE_SHARING_LISTENING&&listeners==1&&strlen(fsh_status.password)==16);}
static void init(void){
 runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;test_runtime_override=&runtime;
 test_volume=(risc_storage_volume_api_v1){1,sizeof(test_volume),NULL,test_ready,test_ready,test_label,test_stat,test_diropen,test_dirnext,test_dirclose,test_open,test_read,NULL,NULL,test_close,NULL,test_error_get};
 export_api.volume.terminal.power.volume.base=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(export_api)};export_api.export_tag=RISC_APP_DATA_EXPORT_TAG;export_api.export_version=1;export_api.entry=catalog;
 if(!empty){const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);}
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());fb_draw();assert(fb_option_count()==7);
 fb_mode=FB_OPTIONS;fb_choice=0;for(unsigned i=0;i<6;i++)fb_move(1);assert(fb_choice==6);fb_draw();
#ifdef FILE_BROWSER_PAPER_PROFILE
 (void)fbp_touch(100,145);
#else
 (void)fb_touch(100,140);assert(fb_choice==6&&fb_mode==FB_OPTIONS); /* Blank last row. */
 (void)fb_touch(100,80);
#endif
 assert(fb_mode==FB_SHARING&&!fb_grant.api&&!portable_file_browser_sharing_active());fb_draw();capture_paper("sharing-choice");
}
#ifdef FILE_SETUP_TEST
#include "file_browser_setup_scenes.inc"
#endif
int main(int argc,char**argv){
 assert(argc>=2);const char*mode=argv[1];test_directory=argc>2?argv[2]:NULL;
 borrowed=!strcmp(mode,"borrowed");empty=!strcmp(mode,"empty");policy_off=!strcmp(mode,"policy-off");policy_busy=!strcmp(mode,"policy-busy");policy_context=!strcmp(mode,"policy-context");
 init();unsigned start_calls=calls;
 if(!strcmp(mode,"back-off")){assert(!fsh_back()&&fb_mode==FB_LIST);goto clean;}
 fsh_action();assert(portable_file_browser_sharing_active()&&!portable_file_browser_idle_ready());
 if(policy_context){step();assert(retains==1&&portable_adapter_retained());unsigned before=calls;fsh_tick();assert(!fsh_close());assert(calls==before);puts("Files sharing policy terminal fence PASS");return 0;}
 if(policy_busy){step();assert(fsh_phase==FSH_POLICY&&calls>start_calls&&!begin_calls);policy_busy=false;}
 if(!strcmp(mode,"cancel-prepare")){assert(fsh_close()&&!begin_calls);goto clean;}
 if(policy_off||empty){for(unsigned i=0;i<20&&fsh_phase!=FSH_OFF;i++)step();assert(fsh_phase==FSH_OFF&&!listeners&&!begin_calls&&fsh_message[0]);fb_draw();goto clean;}
 until_ready();assert(!strcmp(fsh_url,"http://192.0.2.4:8080/"));fb_draw();capture_paper("sharing-ready");assert(begin_calls==!borrowed);
#ifdef FILE_SETUP_TEST
 if(!strncmp(mode,"ble-",4)){if(run_setup_scene(mode))return 0;goto clean;}
#endif
 if(!strcmp(mode,"end-pending")){
  end_busy=true;unsigned before=listeners;step();assert(fsh_service_ending&&listeners==before);unsigned old_end=end_calls;assert(!fsh_close()&&!cancel_calls);assert(end_calls>old_end);end_busy=false;assert(!fsh_close());
 }
 if(!strcmp(mode,"expiry")){test_ticks+=FB_SHARE_LIFETIME_MS;for(unsigned i=0;i<30&&!cancel_calls;i++)step();assert(!listeners&&!fsh_status.password[0]);}
 else if(!strcmp(mode,"tcp-terminal")){tcp_fault=true;assert(!fsh_close()&&retains==1&&!fsh_status.password[0]);unsigned before=calls;fsh_tick();assert(!portable_file_browser_close());assert(calls==before);puts("Files sharing TCP terminal fence PASS");return 0;}
 else {if(!strcmp(mode,"lease-busy")){lease_busy=true;assert(!fsh_close()&&!cancel_calls&&listeners);lease_busy=false;} (void)fsh_back();}
 if(!borrowed){assert(fsh_phase==FSH_STOPPING&&cancel_calls==1&&!listeners);for(unsigned i=0;i<8;i++)step();assert(!retains&&fsh_phase==FSH_STOPPING);quiescent=true;step();}
 else assert(!cancel_calls&&!owned);
 assert(fsh_phase==FSH_OFF&&!fsh_status.password[0]&&closed_listeners==1&&portable_file_browser_idle_ready());
clean:
 assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains&&!owned&&!listeners);
 printf("Files sharing %s: actual controller/adapter/network/HTTP client PASS\n",mode);return 0;
}
