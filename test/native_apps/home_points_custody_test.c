/* Actual overview projection adapter and canonical alarm client. */
#include <assert.h>
#include <stdio.h>
#define ALARM_SERVICE_TAGGED_V2
#define PORTABLE_ALARM_TERMINAL_RETENTION
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_ALARM_ACQUISITION_CUSTODY
#include "PortableAlarmClient.h"
#include "PointsServiceProjection.h"
static bool retained,lost,lose;
static unsigned calls,retains;
static int32_t result;
static portable_alarm_client alarms;
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime)};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!lost?&runtime:NULL;}
bool portable_adapter_retained(void){return retained;}
void portable_adapter_retain_silent(void){retained=true;retains++;}
void portable_adapter_retain(void){portable_adapter_retain_silent();}
#include "../../lib/PortableApps/src/home_points_catalog_adapter.inc"
static int32_t status(void*c,alarm_status_v1*out){(void)c;(void)out;assert(!"Projection must not pump status");return ALARM_OK;}
static int32_t operation(void*c){(void)c;assert(!"Projection must not mutate service");return ALARM_OK;}
static int32_t acknowledge(void*c,const alarm_token_v1*t){(void)t;return operation(c);}
static int32_t prepare(void*c,alarm_sleep_v1*t){(void)t;return operation(c);}
static int32_t project(void*c,points_catalog_projection*out){
 (void)c;assert(!retained&&!lost);calls++;if(lose)lost=true;
 (void)out;return result;
}
int main(void){
 points_service_v2 service={.base={.base={.api_version=2,.struct_size=sizeof(service),.status=status,.step=operation,.refresh=operation,.acknowledge=acknowledge,.prepare_sleep=prepare,.stop_only=operation},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION},
 .points={POINTS_SERVICE_PROJECTION_TAG,POINTS_SERVICE_PROJECTION_VERSION,project}};
 portable_points_catalog_view out={.struct_size=sizeof(out)},saved=out;
 alarms.native_owner=&runtime;assert(portable_home_points_catalog(&out)==0&&!calls);
 alarms.api=&service.base.base;service.base.base.struct_size=sizeof(service.base);
 assert(portable_home_points_catalog(&out)==0&&!calls&&!retained); /* Missing suffix. */
 service.base.base.struct_size=sizeof(service);
 result=ALARM_PENDING;assert(portable_home_points_catalog(&out)==2&&calls==1&&!memcmp(&out,&saved,sizeof(out)));
 result=ALARM_INVALID;assert(portable_home_points_catalog(&out)==0&&!retained);
 const int32_t faults[]={ALARM_OK,ALARM_PENDING,ALARM_INVALID,-99,99,ALARM_OUTPUT,ALARM_RETAINED};
 for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);i++){
  retained=lost=false;alarms.retained=false;alarms.native_owner=&runtime;result=faults[i];lose=i<3;
  unsigned before=calls,prior=retains;
  assert(portable_home_points_catalog(&out)==-2&&retained&&alarms.retained&&retains==prior+1&&calls==before+1);
  assert(!memcmp(&out,&saved,sizeof(out)));
  assert(portable_home_points_catalog(&out)==-2&&calls==before+1);
 }
 puts("Actual overview Points projection: absent suffix unavailable, pending unchanged, OK/PENDING/INVALID identity loss and unknown/terminal fences PASS");
}
