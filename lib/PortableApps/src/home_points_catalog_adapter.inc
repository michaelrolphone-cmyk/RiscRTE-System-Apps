/* Selected Home/desk profile: use the already owned alarm service grant.
 * This is a copied read-only projection, never catalog storage or a new grant. */
#include "PortableHomePointsCatalog.h"
#include "PointsServiceProjection.h"
#ifndef PORTABLE_ALARM_TERMINAL_RETENTION
#error "Catalog projection requires the silent terminal alarm fence"
#endif
int portable_home_points_catalog(portable_points_catalog_view *out) {
 if(portable_adapter_retained())return -2;
 if(!out || out->struct_size<sizeof(*out))return -1;
 if(!alarms.api)return 0;
 portable_points_catalog_view next={.struct_size=sizeof(next)};
 int32_t result=points_service_project(alarms.api,&next);
 if(result==ALARM_RETAINED || result==ALARM_OUTPUT) {
  alarms.retained=true;portable_adapter_retain_silent();return -2;
 }
 if(result==ALARM_INVALID)return 0;
 if(result==ALARM_PENDING)return 2;
 if(result!=ALARM_OK || !portable_points_catalog_view_valid(&next))return -1;
 *out=next;return 1;
}
