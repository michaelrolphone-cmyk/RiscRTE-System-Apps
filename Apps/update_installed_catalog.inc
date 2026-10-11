/* Cached installed inventory alongside the existing release rows. This facade
 * never turns an installed row into bank-update authority. No SD grant or I/O
 * is needed to display the persisted catalog. Springboard owns async refresh. */
#if PORTABLE_UPDATE_FIRMWARE
#error "Installed inventory belongs to App Store"
#endif
#include "InstalledAppsV1.h"
static risc_runtime_capability_v1 installed_inventory_grant;
static const installed_apps_v1 *installed_inventory;
static const software_update_v1 *release_service;
static bool inventory_status(void*c,software_update_status_v1*out){
 (void)c;if(!release_service->status(release_service->context,out))return false;
 if(out->count>SOFTWARE_UPDATE_ROWS_MAX)return false;
 unsigned extra=installed_inventory?installed_inventory->count(installed_inventory->context):0;
 if(extra>INSTALLED_APPS_MAX)return false;
 unsigned room=SOFTWARE_UPDATE_ROWS_MAX-out->count;
 out->count+=extra<room?extra:room;return true;
}
static bool inventory_get(void*c,uint32_t i,software_update_row_v1*out){
 (void)c;software_update_status_v1 state={.struct_size=sizeof(state)};
 if(!out||out->struct_size<sizeof(*out)||!release_service->status(release_service->context,&state))return false;
 if(i<state.count)return release_service->get(release_service->context,i,out);
 if(i>=SOFTWARE_UPDATE_ROWS_MAX||!installed_inventory)return false;
 installed_app_v1 row={0};if(!installed_inventory->get(installed_inventory->context,i-state.count,&row))return false;
 *out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_CURRENT};
 snprintf(out->id,sizeof(out->id),"Installed: %.53s",row.name);
 snprintf(out->version,sizeof(out->version),"%.31s",row.version);
 snprintf(out->installed,sizeof(out->installed),"%.31s",row.version);
 snprintf(out->reason,sizeof(out->reason),"%s",row.available?"Installed on SD":"Installed on SD; availability not confirmed");
 return true;
}
static bool inventory_begin(void*c,uint32_t i,uint64_t utc){
 (void)c;software_update_status_v1 state={.struct_size=sizeof(state)};
 return release_service->status(release_service->context,&state)&&i<state.count&&release_service->begin(release_service->context,i,utc);
}
static bool inventory_refresh(void*c,uint64_t utc){(void)c;return release_service->refresh(release_service->context,utc);}
static bool inventory_step(void*c){(void)c;return release_service->step(release_service->context);}
static bool inventory_cancel(void*c){(void)c;return release_service->cancel(release_service->context);}
static bool inventory_activate(void*c){(void)c;return release_service->activate(release_service->context);}
static bool inventory_restart(void*c){(void)c;return release_service->restart(release_service->context);}
static const software_update_v1 inventory_view={1,sizeof(inventory_view),NULL,inventory_refresh,inventory_step,inventory_status,inventory_get,inventory_begin,inventory_cancel,inventory_activate,inventory_restart};
static bool inventory_open(void){
 release_service=us;installed_inventory=NULL;
 installed_inventory_grant=(risc_runtime_capability_v1){.struct_size=sizeof(installed_inventory_grant)};
 if(!ur->acquire(INSTALLED_APPS_CAPABILITY,1,0,&installed_inventory_grant)){
  if(installed_inventory_grant.api){PORTABLE_APP_RETAIN();return false;}
  return true;
 }
 const installed_apps_v1*c=installed_inventory_grant.api;
 if(!c||c->api_version!=1||c->struct_size<sizeof(*c)||!c->refresh||!c->count||!c->get||!c->cancel){
  if(!ur->release(&installed_inventory_grant)){PORTABLE_APP_RETAIN();return false;}
  return true;
 }
 int rc=c->refresh(c->context);
 if(rc==INSTALLED_APPS_RETAINED){PORTABLE_APP_RETAIN();return false;}
 if(rc<0){if(!ur->release(&installed_inventory_grant)){PORTABLE_APP_RETAIN();return false;}return true;}
 installed_inventory=c;us=&inventory_view;return true;
}
static bool inventory_close(void){
 if(installed_inventory_grant.api&&!ur->release(&installed_inventory_grant))return false;
 installed_inventory=NULL;us=release_service;release_service=NULL;return true;
}
