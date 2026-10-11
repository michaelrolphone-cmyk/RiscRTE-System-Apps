#include "RiscCivilClockV1.h"
#include "RiscRuntimeV1.h"
#include "RiscProviderV2.h"
#include "PortableTimeZone.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool live=true,bad,loss;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){static const risc_runtime_api_v1 r={.api_version=1,.struct_size=sizeof(r)};return live&&v==1?&r:NULL;}
#ifdef CIVIL_NATIVE_UTC
#include "RiscPlatformRealtimeV1.h"
#include "RiscBoundKeyValueV1.h"
#include "PortableTimeZonePreference.h"
static int read_utc(void*c,risc_realtime_snapshot_v1*s){(void)c;assert(live);portable_timezone_civil t={2026,10,10,18,34,0,0};int64_t epoch;assert(portable_timezone_civil_to_epoch(&t,&epoch)==0);*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=bad?0:1,.epoch_seconds=epoch};if(loss){live=false;return RISC_REALTIME_CONTEXT;}return 0;}
static int get(void*c,const char*k,void*p,uint32_t capacity,uint32_t*size){(void)c;assert(live&&!strcmp(k,"time_zone")&&capacity==44);uint8_t b[44]={'T','Z',1,0};memcpy(b+4,"America/Denver",14);b[3]=0xa5;for(unsigned i=0;i<44;i++)if(i!=3)b[3]^=b[i];memcpy(p,b,sizeof(b));*size=sizeof(b);return 0;}
static const risc_platform_realtime_api_v1 clock_api={1,sizeof(clock_api),NULL,read_utc};
static const risc_bound_key_value_v1 kv={1,sizeof(kv),NULL,get,NULL};
#else
#include "PortableRtcClock.h"
static bool read_rtc(void*c,twatch_rtc_time_v1*t){(void)c;assert(live);*t=(twatch_rtc_time_v1){2026,10,11,0,2,34,0};if(loss)live=false;return !bad;}
static const twatch_rtc_api_v1 clock_api={2,sizeof(clock_api),NULL,read_rtc,NULL,NULL,NULL};
#endif
int main(int argc,char**argv){assert(argc==2);loss=!strcmp(argv[1],"loss");const risc_driver_v2 *driver=t5_driver_get(2);assert(driver);
#ifdef CIVIL_NATIVE_UTC
 risc_provider_dependency_v1 deps[]={{"platform.realtime",1,&clock_api},{"storage.key-value.bound",1,&kv}};
#else
 risc_provider_dependency_v1 deps[]={{"rtc.clock",2,&clock_api}};
#endif
 assert(driver->start(deps,sizeof(deps)/sizeof(deps[0])));const risc_civil_clock_api_v1 *api=driver->capability;risc_civil_time_v1 sample={.struct_size=sizeof(sample)};
 if(loss){assert(api->read(NULL,&sample)==-9);assert(!driver->quiesce());puts("Civil clock retention PASS");return 0;}
 assert(!api->read(NULL,&sample));portable_timezone_civil date={2026,10,10,0,0,0,0};int64_t epoch;assert(!portable_timezone_civil_to_epoch(&date,&epoch));assert(sample.day==(epoch-946684800)/86400&&sample.minute==754&&!strcmp(sample.zone,"America/Denver"));
 bad=true;assert(api->read(NULL,&sample)==-1);assert(driver->quiesce());driver->stop();puts("Civil clock local date / readonly clock domains PASS");return 0;
}
