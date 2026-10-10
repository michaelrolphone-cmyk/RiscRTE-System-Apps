#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "PortableHomePointsCatalog.h"
#include "PointsServiceProjection.h"
static struct {const alarm_service_v1 *api;bool retained;} alarms;
static bool terminal;static unsigned calls,fences;static int32_t result;static bool invalid;
bool portable_adapter_retained(void){return terminal;}
void portable_adapter_retain_silent(void){terminal=true;fences++;}
#include "../../lib/PortableApps/src/home_points_catalog_adapter.inc"
static int32_t project(void *context,points_catalog_projection *out){assert(context==&calls&&!terminal);calls++;*out=(points_catalog_projection){.struct_size=sizeof(*out),.seconds=123,.valid_until=invalid?123:180,.catalog_revision=7};return result;}
static int32_t status(void*c,alarm_status_v1*s){(void)c;(void)s;assert(0);return 0;}
static int32_t step(void*c){(void)c;assert(0);return 0;}
static int32_t ack(void*c,const alarm_token_v1*t){(void)c;(void)t;assert(0);return 0;}
static int32_t prepare(void*c,alarm_sleep_v1*t){(void)c;(void)t;assert(0);return 0;}
int main(void){
 points_service_v1 v1={0};v1.base.base.api_version=1;v1.base.base.struct_size=sizeof(v1);v1.base.base.context=&calls;v1.points=(points_service_projection_suffix){POINTS_SERVICE_PROJECTION_TAG,1,project};
 points_service_v2 v2={0};v2.base.base=v1.base.base;v2.base.base.api_version=2;v2.base.base.struct_size=sizeof(v2);v2.base.tag=ALARM_SERVICE_DESCRIPTOR_TAG;v2.base.descriptor_version=1;v2.base.base.status=status;v2.base.base.step=step;v2.base.base.refresh=step;v2.base.base.acknowledge=ack;v2.base.base.prepare_sleep=prepare;v2.base.base.stop_only=step;v2.points=v1.points;
 for(unsigned api=0;api<2;api++){
  alarms.api=api?&v2.base.base:&v1.base.base;
  portable_points_catalog_view out={.struct_size=sizeof(out)};
  assert(portable_home_points_catalog(&out)==1&&out.catalog_revision==7);unsigned n=calls;
  portable_points_catalog_view saved_out=out;result=ALARM_PENDING;
  assert(portable_home_points_catalog(&out)==2&&!memcmp(&out,&saved_out,sizeof(out)));result=ALARM_OK;n=calls;
  out.struct_size=0;assert(portable_home_points_catalog(&out)==-1&&calls==n);out.struct_size=sizeof(out);
  invalid=true;assert(portable_home_points_catalog(&out)==-1);invalid=false;
  for(unsigned i=0;i<3;i++){result=i==0?ALARM_STORAGE:i==1?ALARM_RTC:ALARM_BUSY;assert(portable_home_points_catalog(&out)==-1);}result=0;
  const alarm_service_v1 *saved=alarms.api;alarms.api=NULL;assert(portable_home_points_catalog(&out)==0);alarms.api=saved;
  for(unsigned i=0;i<2;i++){result=i?ALARM_OUTPUT:ALARM_RETAINED;assert(portable_home_points_catalog(&out)==-2&&terminal&&alarms.retained);n=calls;assert(portable_home_points_catalog(&out)==-2&&calls==n);terminal=alarms.retained=false;result=0;}
 }
 assert(fences==4);puts("Home catalog adapter API1/API2 copied projection, invalid/error and silent terminal fence PASS");
}
