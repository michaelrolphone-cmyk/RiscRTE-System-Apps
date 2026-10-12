/* Production tagged native-UTC catalog alarm, matching the delivered 0.4.8
 * profile. Hardware dependencies reuse the Utilities production-source test
 * fixture. Native storage refuses unleased access during active radio work. */
#define ALARM_NATIVE_UTC
#define main unused_catalog_alarm_test_main
#include "points_catalog_service_test.c"
#undef main
extern bool peer_resident_native_busy(void),peer_resident_lease_live(void);
static unsigned unleased_reads,storage_reads,rtc_reads;
static int32_t guarded_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 ++storage_reads;
 if(peer_resident_native_busy()&&!peer_resident_lease_live()){
  ++unleased_reads;*n=0;return RISC_BOUND_KEY_VALUE_BUSY;
 }
 return get(c,k,b,cap,n);
}
static int32_t guarded_realtime(void*c,risc_realtime_snapshot_v1*out){++rtc_reads;return realtime_read(c,out);}
static const risc_bound_key_value_v1 guarded_bound={1,sizeof(guarded_bound),NULL,guarded_get,put};
static const risc_platform_realtime_api_v1 guarded_time={1,sizeof(guarded_time),NULL,guarded_realtime};
const alarm_service_v1 *peer_alarm_start(void){
 boot(true);assert(provider->quiesce());
 risc_provider_dependency_v1 guarded_deps[sizeof(deps)/sizeof(deps[0])];memcpy(guarded_deps,deps,sizeof(deps));guarded_deps[0].api=&guarded_bound;guarded_deps[3].api=&guarded_time;
 assert(provider->start(guarded_deps,sizeof(guarded_deps)/sizeof(guarded_deps[0])));idle();unleased_reads=0;return client;
}
void peer_alarm_time(uint64_t value){if(value>ms)ms=value;}
void peer_alarm_busy(bool value){kv_result=value?RISC_BOUND_KEY_VALUE_BUSY:0;}
unsigned peer_alarm_reads(void){return storage_reads;}
unsigned peer_alarm_rtc_reads(void){return rtc_reads;}
unsigned peer_alarm_unleased_reads(void){return unleased_reads;}
