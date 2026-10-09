/* Actual System app translation unit, production adapter/source/helpers and
 * tagged alarm contract over strict deterministic providers; no device I/O. */
#undef PORTABLE_QUICK_ACTIONS
#undef PORTABLE_ALARM_CLIENT
#define TEST_NATIVE_SOURCE_NO_MAIN
#include "portable_native_time_source_test.c"
#include "RiscStorageVolumeV1.h"
#include "WifiApi.h"
extern void app_main(void);
extern bool native_system_test_open(void);
static unsigned quick_opens,nav_polls,launches,storage_live,wifi_live,credentials_live,dirs,disconnects;
static bool refuse_launch,quick_case,refuse_finalize,apply_quick_action;
static int32_t alarm_outcome;
static int32_t tagged_status(void *c,alarm_status_v1 *out){(void)c;io();*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t tagged_step(void *c){(void)c;io();return alarm_outcome;}
static int32_t tagged_refresh(void *c){(void)c;io();++refreshes;return ALARM_OK;}
static int32_t tagged_stop(void *c){(void)c;io();++alarm_stops;return ALARM_OK;}
static alarm_service_descriptor_v2 tagged={.base={2,sizeof(tagged),NULL,tagged_status,tagged_step,tagged_refresh,fx_alarm_ack,fx_alarm_prepare,tagged_stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=0};
static bool app_nav(void *c,risc_input_navigation_frame_v1 *out){
 (void)c;io();*out=(risc_input_navigation_frame_v1){0};++nav_polls;assert(nav_polls<200);
 if(nav_polls==8 || (refuse_launch&&nav_polls==20))out->buttons=out->pressed=RISC_NAV_HOME;
 return true;
}
static bool app_foreground(void *c,const risc_input_foreground_v1 *claims,size_t n){(void)c;(void)claims;io();assert(n<=1);if(refuse_finalize&&!n){pending_fence=true;return false;}return true;}
static bool app_reset(void *c){(void)c;io();return true;}
static const risc_input_navigation_api_v1 app_navigation={1,sizeof(app_navigation),NULL,app_nav,app_foreground,app_reset};
static int32_t app_get(void *c,const char *key,void *out,uint32_t size,uint32_t *used){
 if(!strcmp(key,PORTABLE_TIMEZONE_KEY))return source_get(c,key,out,size,used);
 return fx_kv_get(c,key,out,size,used);
}
static int32_t credential_get(void *c,const char *key,void *out,uint32_t size,uint32_t *used){(void)c;(void)key;(void)out;(void)size;io();*used=0;return RISC_KEY_VALUE_NOT_FOUND;}
static const risc_key_value_v1 credential_kv={1,sizeof(credential_kv),NULL,credential_get,forbidden_put};
static bool volume_ok(void *c){(void)c;io();return true;}
static bool volume_label(void *c,char *out,size_t size){(void)c;io();snprintf(out,size,"Local fixture");return true;}
static bool volume_stat(void *c,const char *p,uint64_t *size,bool *dir){(void)c;(void)p;io();*size=0;*dir=true;return true;}
static risc_storage_dir_t volume_dir(void *c,const char *p){(void)c;(void)p;io();assert(!dirs);++dirs;return 1;}
static bool volume_next(void *c,risc_storage_dir_t h,risc_storage_dirent_v1 *out){(void)c;(void)out;io();assert(h==1&&dirs);return false;}
static void volume_close(void *c,risc_storage_dir_t h){(void)c;io();assert(h==1&&dirs);--dirs;}
static risc_storage_file_t volume_open(void *c,const char *p,uint64_t *size){(void)c;(void)p;io();*size=0;return 0;}
static size_t volume_read(void *c,risc_storage_file_t h,void *out,size_t size){(void)c;(void)h;(void)out;(void)size;io();return 0;}
static bool volume_file_close(void *c,risc_storage_file_t h,bool commit){(void)c;(void)h;(void)commit;io();return true;}
static bool volume_error(void *c,char *out,size_t size){(void)c;io();assert(size);*out=0;return true;}
static const risc_storage_volume_api_v1 volume={1,sizeof(volume),NULL,volume_ok,volume_ok,volume_label,volume_stat,volume_dir,volume_next,volume_close,volume_open,volume_read,NULL,NULL,volume_file_close,NULL,volume_error};
static bool wifi_connect_fixture(void *c,const char *ssid,const char *password){(void)c;(void)ssid;(void)password;io();return true;}
static wifi_link_t wifi_status_fixture(void *c){(void)c;io();return WIFI_LINK_DOWN;}
static int8_t wifi_rssi_fixture(void *c){(void)c;io();return -30;}
static bool wifi_addresses_fixture(void *c,wifi_ipv4_v1 *s,wifi_ipv4_v1 *a){(void)c;io();*s=(wifi_ipv4_v1){0};*a=(wifi_ipv4_v1){0};return true;}
static bool wifi_scan_fixture(void *c,garden_radio_scan_result_v1 *out){(void)c;io();*out=(garden_radio_scan_result_v1){0};return true;}
static bool wifi_disconnect_fixture(void *c){(void)c;io();++disconnects;if(refuse_finalize){pending_fence=true;return false;}return true;}
static const wifi_api_v1 app_wifi={.api_version=1,.struct_size=sizeof(app_wifi),.connect=wifi_connect_fixture,.status=wifi_status_fixture,.rssi=wifi_rssi_fixture,.addresses=wifi_addresses_fixture,.scan_start=volume_ok,.scan_poll=wifi_scan_fixture,.scan_cancel=volume_ok,.disconnect_checked=wifi_disconnect_fixture};
static bool app_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 const void *api=NULL;
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==2&&!instance);api=&tagged.base;}
 else if(!strcmp(name,"input.navigation")){assert(version==1&&!instance);api=&app_navigation;}
 else if(!strcmp(name,"storage.volume")){assert(version==1&&instance==11);api=&volume;++storage_live;}
 else if(!strcmp(name,"net.wifi")){assert(version==1&&instance==15);api=&app_wifi;++wifi_live;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&instance==6){assert(version==1);api=&credential_kv;++credentials_live;}
 else return source_acquire(name,version,instance,out);
 io();++acquires;++live;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=api};return true;
}
static bool app_release(risc_runtime_capability_v1 *g){
 if(g->api==&volume){assert(storage_live&&!dirs);if(refuse_finalize){io();pending_fence=true;return false;}--storage_live;}
 if(g->api==&app_wifi){assert(wifi_live);--wifi_live;}
 if(g->api==&credential_kv){assert(credentials_live);--credentials_live;}
 return source_release(g);
}
static bool app_launch(const char *name){io();assert(!strcmp(name,"default.elf"));assert(!storage_live&&!wifi_live&&!credentials_live&&!dirs);++launches;return !refuse_launch;}
static void app_yield(uint32_t delay){
 fx_yield(delay);
 if(quick_case && presents && !quick_modal && !launches){quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.neutral_gate=false;quick.ui.gesture=PQA_IDLE;quick_case=false;++quick_opens;}
 if(quick_modal&&zone_reads){
  if(apply_quick_action){quick.ui.action_dnd=true;quick.ui.pending|=PQA_DND;apply_quick_action=false;}
  else pqa_close(&quick.ui);
 }
}
int main(int argc,char **argv){
 assert(argc==2);const char *name=argv[1];scenario="valid";
 source_kv.get=app_get;source_kv.put=fx_kv_put;
 fx_runtime.acquire=app_acquire;fx_runtime.release=app_release;fx_runtime.request_launch=app_launch;fx_runtime.yield_ms=app_yield;
 if(!strcmp(name,"utc")){zone="UTC";sample.epoch_seconds=0;}
 if(!strcmp(name,"zone"))zone="Asia/Kathmandu";
 if(!strcmp(name,"missing-zone"))scenario="kv-missing";
 if(!strcmp(name,"native-unset"))scenario="native-unset";
 refuse_launch=!strcmp(name,"home-refused");quick_case=true;apply_quick_action=!strcmp(name,"quick");
 assert(app_module_init()==0);
 const paper_presentation *retained_view=paper_presentation_get();assert(retained_view);
 if(!strcmp(name,"kv-context"))scenario="kv-context";
 if(!strcmp(name,"native-release-false"))scenario="native-release-false";
 if(!strcmp(name,"alarm-retained"))alarm_outcome=ALARM_RETAINED;
 if(!strcmp(name,"fini-owned")||!strcmp(name,"fini-refused")){
  assert(native_system_test_open());refuse_finalize=!strcmp(name,"fini-refused");app_module_fini();
  if(refuse_finalize){check_retained(retained_view);puts("{\"case\":\"fini-refused\",\"retained\":true}");return 0;}
  assert(!live&&!storage_live&&!wifi_live&&!credentials_live&&!dirs&&!frames&&!subscriptions&&!retained);
  puts("{\"case\":\"fini-owned\",\"closed\":true}");return 0;
 }
 app_main();
 if(retained){assert(barriers==1&&!launches);check_retained(retained_view);}
 else {
  assert(launches==1);
  assert(quick_opens==1);
  if(!strcmp(name,"quick"))assert(kv_writes==1&&refreshes==1&&dnd_saved&&dnd_value);
  assert(native_reads>0);
  assert(!reader_live&&!zone_live&&!storage_live&&!wifi_live&&!credentials_live&&!dirs);
  app_module_fini();assert(!live&&!frames&&!subscriptions&&!retained);
 }
 no_legacy();
 printf("{\"case\":\"%s\",\"native_reads\":%u,\"launches\":%u,\"retained\":%s}\n",name,native_reads,launches,retained?"true":"false");
 return 0;
}
