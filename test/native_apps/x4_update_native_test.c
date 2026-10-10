#define TEST_CORE_PAPER_MOTION
#include "native_system_apps_test.c"
extern bool native_update_test_utc(uint64_t *);
extern bool native_update_test_check(void);
int main(int argc,char **argv){
 assert(argc==2);scenario="valid";
 source_kv.get=app_get;source_kv.put=fx_kv_put;
 fx_runtime.acquire=app_acquire;fx_runtime.release=app_release;fx_runtime.request_launch=app_launch;fx_runtime.yield_ms=app_yield;
 assert(app_module_init()==0);
 const paper_presentation *view=paper_presentation_get();assert(view);
#ifdef PORTABLE_BLE_BROADCAST
 if(!strncmp(argv[1],"broadcast-",10)){
  assert(native_system_test_open());fixture_broadcast_live=true;
  if(!strcmp(argv[1],"broadcast-close-retained")){
   fixture_broadcast_pause_fail=true;assert(!native_update_test_check());check_retained(view);
  }else{
   assert(native_update_test_check()&&!fixture_broadcast_live);
   if(!strcmp(argv[1],"broadcast-busy")){
    update_fixture_state=SOFTWARE_UPDATE_DOWNLOAD;assert(portable_update_services_safe());
    unsigned old=calls;assert(!portable_update_broadcast_safe()&&calls==old);
    update_fixture_state=SOFTWARE_UPDATE_LIST;assert(portable_update_services_safe());
    old=calls;assert(portable_update_broadcast_safe()&&calls==old);
   }
   app_module_fini();assert(!live&&!update_live&&!credentials_live&&!retained);
  }
 }else
#endif
 if(!strcmp(argv[1],"no-feed")){
  assert(native_system_test_open());unsigned before=native_reads;
  assert(native_update_test_check()&&update_checks==1&&native_reads==before&&!wifi_live&&!radio_connects);
  app_module_fini();assert(!live&&!update_live&&!credentials_live&&!frames&&!retained);
 }else if(!strcmp(argv[1],"utc")){
  uint64_t epoch=0;zone="Asia/Kathmandu";
  assert(native_update_test_utc(&epoch)&&epoch==(uint64_t)sample.epoch_seconds&&!zone_reads&&!reader_live);
  sample.epoch_seconds=0;assert(!native_update_test_utc(&epoch));
  scenario="native-unset";assert(!native_update_test_utc(&epoch)&&!reader_live);
  app_module_fini();assert(!live&&!retained);
 }else if(!strcmp(argv[1],"release-retained")){
  scenario="native-release-false";uint64_t epoch=0;assert(!native_update_test_utc(&epoch));check_retained(view);
 }else{
  quick_case=true;if(!strcmp(argv[1],"alarm-retained"))alarm_outcome=ALARM_RETAINED;
  app_main();
  if(retained){assert(barriers==1&&!launches);check_retained(view);}
  else {assert(launches==1&&!update_live&&!credentials_live);app_module_fini();assert(!live&&!frames&&!retained);}
 }
 no_legacy();printf("X4 native update %s passed\n",argv[1]);
}
