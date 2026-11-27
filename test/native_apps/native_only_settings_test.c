/* Controller boundary test: real Settings implementation and native time
 * client; controlled Runtime/IANA/KV providers. No RTC is admitted or called.
 * This test does not claim physical touch/UI behavior. */
#define PORTABLE_SETTINGS_APP
#define PORTABLE_SETTINGS_NATIVE_TIME
#define PORTABLE_SETTINGS_NATIVE_ONLY
#define PORTABLE_SETTINGS_TIME_ZONE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#include "../../lib/PortableApps/src/adapter.c"
#include <assert.h>
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static int64_t now_epoch;
static unsigned acquire_count,release_count,seed_count;
static int seed_status,read_status;static bool fail_release;
static int32_t snapshot(void*c,risc_realtime_snapshot_v1*out){(void)c;if(read_status)return read_status;*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.epoch_seconds=now_epoch,.validity=RISC_REALTIME_VALID};return 0;}
static int32_t seed(void*c,int64_t seconds,uint32_t microseconds){(void)c;assert(!microseconds);++seed_count;if(seed_status)return seed_status;now_epoch=seconds;return 0;}
static risc_realtime_control_api_v1 control;
static bool acquire(const char*cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out){assert(!strcmp(cap,"runtime.realtime-control")&&version==1&&!instance);++acquire_count;out->api=&control;out->slot=acquire_count;out->generation=1;return true;}
static bool release(risc_runtime_capability_v1*g){++release_count;if(fail_release)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_runtime_api_v1 runtime;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t api){return api==1?&runtime:NULL;}
int main(int argc,char**argv){
 (void)argc;
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};rt=&runtime;
 control=(risc_realtime_control_api_v1){1,sizeof(control),&now_epoch,snapshot,seed};
 snt_load_basis();assert(snt_basis.stores_utc&&snt_basis_status==PORTABLE_RTC_BASIS_MISSING);
 snt_zone_status=PORTABLE_TIMEZONE_MISSING;
 assert(portable_timezone_resolve("UTC",4,&snt_rule)==PORTABLE_TIMEZONE_OK);
 settings_draft=(twatch_rtc_time_v1){2026,10,25,0,12,34,56};
 if(!strcmp(argv[1],"io"))seed_status=RISC_REALTIME_IO;
 if(!strcmp(argv[1],"read-io"))read_status=RISC_REALTIME_IO;
 bool result=snt_save(-1);
 assert(result==(!seed_status&&!read_status));assert(seed_count==1&&acquire_count==1&&release_count==1);
 assert(strstr(snt_status_text,"Native time")&&strstr(snt_status_text,"no external RTC"));
 assert(!strstr(snt_status_text,"cold boot unset")&&!strcmp(snt_basis_label(),"Native UTC clock"));
 if(result){assert(snt_last_result.native.verified);assert(now_epoch>1700000000);}
 puts("PASS native-only Settings controller: exact native seed/readback/release, failure visible, zero RTC or KV acquisition");
}
