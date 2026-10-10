/* Actual controllers and production adapter; every provider call and free after
 * retention aborts the fixture. Faults are armed only after adapter startup. */
#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
#include "PortableWifiCredentials.h"
extern bool native_custody_test_close(void);
extern void native_custody_test_action(unsigned);
static const char *fault,*target;
static uint64_t target_instance;
static bool armed_fault,fault_hit,queued_handoff;
static unsigned handoffs;
static unsigned kv_gets,kv_puts,kv_fault_at=1,file_reads,file_writes,closed_files;
static uint8_t records[5][64];
static uint32_t lengths[5];
static wifi_link_t link;
static risc_key_value_v1 test_kv;
#ifdef PORTABLE_BLE_BROADCAST
static bool broadcast_saved;
static uint8_t broadcast_bytes[4];
static int32_t policy_get(void *c,const char *key,void *data,uint32_t size,uint32_t *used){
 if(!strcmp(key,TELEMETRY_BROADCAST_KEY)&&broadcast_saved){source_io();assert(size>=4);memcpy(data,broadcast_bytes,4);*used=4;return RISC_KEY_VALUE_OK;}
 return app_get(c,key,data,size,used);
}
static int32_t policy_put(void *c,const char *key,const void *data,uint32_t size){
 if(!strcmp(key,TELEMETRY_BROADCAST_KEY)){source_io();assert(size==4);memcpy(broadcast_bytes,data,4);broadcast_saved=true;return RISC_KEY_VALUE_OK;}
 return fx_kv_put(c,key,data,size);
}
#endif
static wifi_api_v1 test_wifi;
#ifdef PORTABLE_FILE_BROWSER_APP
static risc_storage_volume_api_v1_ext test_volume;
static t5_file_open_api_v1 test_handlers;
static bool test_handler_request(const char *path,const char *handler,uint64_t cookie){
 (void)path;(void)handler;(void)cookie;source_io();queued_handoff=true;++handoffs;return true;
}
#endif
#ifdef PORTABLE_UPDATE_APP
static software_update_v1 test_update;
#endif
static bool fault_is(const char *name){return armed_fault&&!strcmp(fault,name);}
static unsigned key_index(const char *key){
 if(!strcmp(key,PORTABLE_WIFI_CREDENTIALS_SELECTOR_KEY))return 0;
 for(unsigned i=0;i<4;++i)if(!strcmp(key,portable_wifi_credentials_chunk_key(i/2,i%2)))return i+1;
 assert(false);return 0;
}
static int32_t test_get(void *c,const char *key,void *out,uint32_t size,uint32_t *used){
 (void)c;source_io();++kv_gets;*used=0;
 if(kv_gets==kv_fault_at&&(fault_is("kv-context")||fault_is("kv-unknown")||fault_is("kv-io"))){fault_hit=true;if(!fault_is("kv-io"))pending_fence=true;return fault_is("kv-context")?RISC_KEY_VALUE_CONTEXT:fault_is("kv-unknown")?42:RISC_KEY_VALUE_IO;}
 unsigned i=key_index(key);if(!lengths[i])return RISC_KEY_VALUE_NOT_FOUND;
 assert(size>=lengths[i]);memcpy(out,records[i],lengths[i]);*used=lengths[i];return RISC_KEY_VALUE_OK;
}
static int32_t test_put(void *c,const char *key,const void *data,uint32_t size){
 (void)c;source_io();++kv_puts;
 if(kv_puts==kv_fault_at&&(fault_is("put-context")||fault_is("put-unknown"))){fault_hit=true;pending_fence=true;return fault_is("put-context")?RISC_KEY_VALUE_CONTEXT:42;}
 unsigned i=key_index(key);assert(size<=64);memcpy(records[i],data,size);lengths[i]=size;return RISC_KEY_VALUE_OK;
}
static bool test_connect(void *c,const char *s,const char *p){
 (void)c;source_io();assert(!strcmp(s,"Fixture network")&&!strcmp(p,"testpass123"));++radio_connects;
 if(fault_is("connect-error")){fault_hit=true;return false;}link=WIFI_LINK_JOINING;return true;
}
static wifi_link_t test_status(void *c){(void)c;source_io();return link;}
static bool test_addresses(void *c,wifi_ipv4_v1 *s,wifi_ipv4_v1 *a){(void)c;source_io();*s=(wifi_ipv4_v1){.address={10,0,0,2}};*a=(wifi_ipv4_v1){0};return true;}
static bool test_disconnect(void *c){
 (void)c;source_io();++disconnects;
 if(fault_is("disconnect")){fault_hit=true;pending_fence=true;return false;}link=WIFI_LINK_DOWN;return true;
}
static bool test_cancel_scan(void *c){
 (void)c;source_io();if(fault_is("scan-cancel")){fault_hit=true;pending_fence=true;return false;}return true;
}
#ifdef PORTABLE_UPDATE_APP
static bool test_cancel(void *c){(void)c;source_io();if(fault_is("cancel")){fault_hit=true;pending_fence=true;return false;}return true;}
static bool test_refresh(void *c,uint64_t seconds){(void)c;source_io();assert(seconds>=1704067200);++update_checks;return true;}
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
static bool test_stat(void *c,const char *p,uint64_t *size,bool *dir){(void)c;source_io();*size=4;*dir=false;return strcmp(p,"/target")!=0;}
static risc_storage_file_t test_open(void *c,const char *p,uint64_t *size){(void)c;(void)p;source_io();*size=4;return 1;}
static risc_storage_file_t test_create(void *c,const char *p){(void)c;(void)p;source_io();return 2;}
static size_t test_read(void *c,risc_storage_file_t h,void *out,size_t n){(void)c;source_io();assert(h==1&&n==4);++file_reads;if(fault_is("read-error")){fault_hit=true;return 0;}memcpy(out,"data",4);return 4;}
static size_t test_write(void *c,risc_storage_file_t h,const void *out,size_t n){(void)c;source_io();assert(h==2&&n==4&&!memcmp(out,"data",4));++file_writes;if(fault_is("write-error")){fault_hit=true;return 0;}return n;}
static bool test_file_close(void *c,risc_storage_file_t h,bool commit){(void)c;(void)commit;source_io();assert(h==1||h==2);++closed_files;if(fault_is("file-close")){fault_hit=true;pending_fence=true;return false;}return true;}
static bool test_dir_close(void *c,risc_storage_dir_t h){
 (void)c;source_io();assert(h==1&&dirs);if(fault_is("dir-close")){fault_hit=true;pending_fence=true;return false;}--dirs;return true;
}
#endif
static bool test_ble_set(void *c,bool enabled){(void)c;source_io();assert(!enabled);return true;}
static bool test_ble_status(void *c,uint8_t *state){(void)c;source_io();*state=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 test_ble={.api_version=1,.struct_size=sizeof(test_ble),.set_enabled=test_ble_set,.status=test_ble_status};
static bool test_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 assert(!pending_fence&&!retained);
 bool match=armed_fault&&target&&!strcmp(name,target)&&instance==target_instance;
 if(match&&fault_is("acquire-empty")){source_io();fault_hit=true;pending_fence=true;return false;}
 if(!strcmp(name,"bluetooth.hci")){
  assert(version==1&&instance==16);source_io();++acquires;++live;
  *out=(risc_runtime_capability_v1){.struct_size=sizeof(*out),.slot=acquires,.generation=1,.api=&test_ble};return true;
 }
 bool ok=app_acquire(name,version,instance,out);assert(ok);
 if(out->api==&credential_kv)out->api=&test_kv;
 if(out->api==&app_wifi)out->api=&test_wifi;
#ifdef PORTABLE_FILE_BROWSER_APP
 if(out->api==&volume)out->api=&test_volume;
 if(out->api==&handlers)out->api=&test_handlers;
#endif
#ifdef PORTABLE_UPDATE_APP
 if(out->api==&update_service)out->api=&test_update;
#endif
 if(match){
  if(fault_is("acquire-dirty")){fault_hit=true;pending_fence=true;return false;}
  if(fault_is("grant-generation")){out->generation=0;fault_hit=true;pending_fence=true;}
  if(fault_is("grant-slot")){out->slot=0;fault_hit=true;pending_fence=true;}
  if(fault_is("grant-size")){out->struct_size=4;fault_hit=true;pending_fence=true;}
  if(fault_is("grant-api")){out->api=NULL;fault_hit=true;pending_fence=true;}
 }
 return true;
}
static bool test_release(risc_runtime_capability_v1 *g){
 assert(!pending_fence&&!retained);
 bool owned=g->api==&test_kv||g->api==&test_wifi;
#ifdef PORTABLE_FILE_BROWSER_APP
 owned|=g->api==&test_volume||g->api==&test_handlers;
#endif
#ifdef PORTABLE_UPDATE_APP
 owned|=g->api==&test_update;
#endif
 if(owned&&(fault_is("release-false")||fault_is("release-dirty")||fault_is("release-size")||fault_is("handoff-release"))){
  source_io();fault_hit=true;pending_fence=true;
  if(fault_is("release-size"))*g=(risc_runtime_capability_v1){.struct_size=4};
  return !fault_is("release-false");
 }
 if(g->api==&test_kv)g->api=&credential_kv;
 if(g->api==&test_wifi)g->api=&app_wifi;
#ifdef PORTABLE_FILE_BROWSER_APP
 if(g->api==&test_volume)g->api=&volume;
 if(g->api==&test_handlers)g->api=&handlers;
#endif
#ifdef PORTABLE_UPDATE_APP
 if(g->api==&test_update)g->api=&update_service;
#endif
 return app_release(g);
}
static bool test_retain(void){queued_handoff=false;return fx_retain();}
int main(int argc,char **argv){
 assert(argc==3);char fault_name[48];snprintf(fault_name,sizeof(fault_name),"%s",argv[1]);fault=fault_name;
 char *suffix=strrchr(fault_name,'-');if(suffix&&suffix[1]>='1'&&suffix[1]<='4'&&!suffix[2]){kv_fault_at=(unsigned)(suffix[1]-'0');*suffix=0;}
 scenario="valid";target_instance=0;
 if(!strcmp(argv[2],"credentials")){target=RISC_KEY_VALUE_CAPABILITY;target_instance=6;}
 else if(!strcmp(argv[2],"wifi")){target="net.wifi";target_instance=15;}
 else if(!strcmp(argv[2],"volume")){target="storage.volume";target_instance=9;}
 else if(!strcmp(argv[2],"handlers"))target="file.open";
#ifdef PORTABLE_UPDATE_APP
 else if(!strcmp(argv[2],"update"))target=PORTABLE_UPDATE_FIRMWARE?SOFTWARE_UPDATE_FIRMWARE_CAPABILITY:SOFTWARE_UPDATE_APPS_CAPABILITY;
#endif
 else assert(!strcmp(argv[2],"none"));
 test_kv=(risc_key_value_v1){1,sizeof(test_kv),NULL,test_get,test_put};
 portable_wifi_credentials seed={"Fixture network","testpass123"};assert(portable_wifi_credentials_save(&test_kv,&seed)==PORTABLE_WIFI_CREDENTIALS_LOADED);kv_gets=kv_puts=0;
 source_kv.get=app_get;source_kv.put=fx_kv_put;
#ifdef PORTABLE_BLE_BROADCAST
 source_kv.get=policy_get;source_kv.put=policy_put;
#endif
 test_wifi=app_wifi;test_wifi.connect=test_connect;test_wifi.status=test_status;test_wifi.addresses=test_addresses;test_wifi.disconnect_checked=test_disconnect;test_wifi.scan_cancel=test_cancel_scan;
#ifdef PORTABLE_FILE_BROWSER_APP
 app_display=fx_display;app_display.submit=app_submit;
 test_handlers=handlers;test_handlers.open_request=test_handler_request;
 test_volume.base=volume;test_volume.base.struct_size=sizeof(test_volume);test_volume.base.stat=test_stat;
 test_volume.base.file_open_read=test_open;test_volume.base.file_open_write=test_create;test_volume.base.file_read=test_read;test_volume.base.file_write=test_write;test_volume.base.file_close=test_file_close;test_volume.dir_close_checked=test_dir_close;
#endif
#ifdef PORTABLE_UPDATE_APP
 test_update=update_service;test_update.cancel=test_cancel;test_update.refresh=test_refresh;
#endif
 fx_runtime.retain_invocation=test_retain;fx_runtime.acquire=test_acquire;fx_runtime.release=test_release;fx_runtime.request_launch=app_launch;fx_runtime.yield_ms=app_yield;
 assert(app_module_init()==0);const paper_presentation *view=paper_presentation_get();assert(view);
 bool opening=target || !strncmp(fault,"kv-",3)||!strcmp(fault,"dir-close");
 armed_fault=opening;bool opened=native_system_test_open();
 if(!retained){
  assert(opened);armed_fault=true;
#ifdef PORTABLE_BLE_BROADCAST
  if(!strcmp(fault,"telemetry-recursion")){
   unsigned paused=fixture_broadcast_pauses;assert(portable_broadcast_enable(false));
   assert(fixture_broadcast_pauses==paused+1&&broadcast_saved&&!retained);
  }else
#endif
  if(!strcmp(fault,"normal")||!strcmp(fault,"reopen")){
   unsigned before=calls;assert(native_system_test_open());assert(calls==before);
   native_custody_test_action(0);
#ifdef PORTABLE_WIFI_SETTINGS_APP
   link=WIFI_LINK_UP;assert(portable_wifi_services_safe());assert(!retained);
#elif defined(PORTABLE_UPDATE_APP)
   link=WIFI_LINK_UP;native_custody_test_action(1);assert(!retained&&update_checks==1);
#endif
   assert(native_custody_test_close());assert(native_system_test_open());assert(native_custody_test_close());
  }else if(!strncmp(fault,"kv-",3)||target){native_custody_test_action(0);}
  else if(!strncmp(fault,"put-",4)){native_custody_test_action(2);}
  else if(!strcmp(fault,"scan-cancel")){native_custody_test_action(3);native_custody_test_action(4);}
  else if(!strcmp(fault,"disconnect")){native_custody_test_action(0);
#ifdef PORTABLE_UPDATE_APP
   assert(!native_custody_test_close());
#endif
  }
  else if(!strcmp(fault,"connect-error")){native_custody_test_action(0);assert(fault_hit&&!retained);}
  else if(!strcmp(fault,"handoff-release")){native_custody_test_action(6);assert(handoffs==1&&!queued_handoff);}
  else if(!strcmp(fault,"file-close")||!strcmp(fault,"read-error")||!strcmp(fault,"write-error")){native_custody_test_action(0);}
  else if(!strncmp(fault,"release-",8)||!strcmp(fault,"cancel")){assert(!native_custody_test_close());}
 }
 if(strcmp(fault,"normal")&&strcmp(fault,"reopen")&&strcmp(fault,"telemetry-recursion"))assert(fault_hit);
 bool expect_retained=strcmp(fault,"telemetry-recursion")&&strcmp(fault,"normal")&&strcmp(fault,"reopen")&&strcmp(fault,"connect-error")&&strcmp(fault,"kv-io")&&strcmp(fault,"read-error")&&strcmp(fault,"write-error");
 assert(retained==expect_retained);
 if(retained){
  assert(fault_hit&&barriers==1&&!launches&&!queued_handoff);
  unsigned before=calls;assert(!native_system_test_open());assert(!native_custody_test_close());
  native_custody_test_action(5);app_main();check_retained(view);assert(calls==before);
 }else{
  assert(!pending_fence);assert(native_custody_test_close());app_module_fini();assert(!live&&!frames&&!wifi_live&&!credentials_live&&!storage_live&&!dirs);
 }
 no_legacy();printf("%s / %s: retained=%u, KV=%u/%u, file=%u/%u, closes=%u\n",fault,argv[2],retained,kv_gets,kv_puts,file_reads,file_writes,closed_files);
}
