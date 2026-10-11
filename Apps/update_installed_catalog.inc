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
#ifdef PORTABLE_APP_STORE_ACTIONS
#ifndef PORTABLE_INSTALLED_VOLUME_INSTANCE
#define PORTABLE_INSTALLED_VOLUME_INSTANCE 9u
#endif
#define SOFTWARE_UPDATE_REMOVE 4u
#define SOFTWARE_UPDATE_INSTALL_SD 5u
static risc_runtime_capability_v1 inventory_volume_grant;
static installed_app_v1 inventory_choice;
static uint32_t inventory_choice_generation;
static bool inventory_chosen,inventory_removing,inventory_choice_install,inventory_installing;
static const char *inventory_result;
static bool inventory_drain(void){
 if(!inventory_removing&&!inventory_volume_grant.api)return true;
 int rc=installed_inventory->cancel(installed_inventory->context);
 if(rc==INSTALLED_APPS_RETAINED){PORTABLE_APP_RETAIN();return false;}
 if(rc!=INSTALLED_APPS_OK)return false;
 if(inventory_volume_grant.api&&!ur->release(&inventory_volume_grant))return false;
 inventory_volume_grant=(risc_runtime_capability_v1){.struct_size=sizeof(inventory_volume_grant)};
 inventory_removing=false;return true;
}
static bool inventory_choose(uint32_t index){
 inventory_chosen=false;
 software_update_status_v1 state={.struct_size=sizeof(state)};
 if(!installed_inventory||!installed_inventory->remove||!installed_inventory->step||!installed_inventory->generation||
    !release_service->status(release_service->context,&state)||index<state.count)return false;
 uint32_t local=index-state.count,installed=installed_inventory->count(installed_inventory->context);inventory_choice_install=local>=installed;
 if(inventory_choice_install){if(!installed_inventory->staged_get||!installed_inventory->install||!installed_inventory->staged_get(installed_inventory->context,local-installed,&inventory_choice))return false;}
 else if(!installed_inventory->get(installed_inventory->context,local,&inventory_choice))return false;
 inventory_choice_generation=installed_inventory->generation(installed_inventory->context);inventory_chosen=true;return true;
}
static bool inventory_remove_selected(uint32_t index){
 installed_app_v1 chosen=inventory_choice;bool installing=inventory_choice_install;uint32_t generation=inventory_choice_generation;
 if(!inventory_chosen||!inventory_choose(index)||installing!=inventory_choice_install||generation!=inventory_choice_generation||
    strcmp(chosen.id,inventory_choice.id)||strcmp(chosen.version,inventory_choice.version)||strcmp(chosen.launch_path,inventory_choice.launch_path))return false;
 inventory_chosen=false;inventory_result=NULL;
 inventory_volume_grant=(risc_runtime_capability_v1){.struct_size=sizeof(inventory_volume_grant)};
 if(!ur->acquire("storage.volume",1,PORTABLE_INSTALLED_VOLUME_INSTANCE,&inventory_volume_grant)){
  if(inventory_volume_grant.api)PORTABLE_APP_RETAIN();return false;
 }
 const risc_storage_volume_api_v1*v=inventory_volume_grant.api;
 if(!v||v->api_version!=1||v->struct_size<sizeof(*v)||(installing?installed_inventory->install(installed_inventory->context,chosen.id,chosen.version):installed_inventory->remove(installed_inventory->context,chosen.id))!=INSTALLED_APPS_OK){
  if(!ur->release(&inventory_volume_grant))PORTABLE_APP_RETAIN();return false;
 }
 inventory_installing=installing;inventory_removing=true;return true;
}
#endif
static bool inventory_status(void*c,software_update_status_v1*out){
 (void)c;if(!release_service->status(release_service->context,out))return false;
 if(out->count>SOFTWARE_UPDATE_ROWS_MAX)return false;
 unsigned extra=installed_inventory?installed_inventory->count(installed_inventory->context):0;
 if(extra>INSTALLED_APPS_MAX)return false;
 unsigned staged=installed_inventory&&installed_inventory->staged_count?installed_inventory->staged_count(installed_inventory->context):0;
 if(staged>INSTALLED_APPS_MAX)return false;extra+=staged;
 unsigned room=SOFTWARE_UPDATE_ROWS_MAX-out->count;
 out->count+=extra<room?extra:room;
#ifdef PORTABLE_APP_STORE_ACTIONS
 if(inventory_removing){out->state=SOFTWARE_UPDATE_PREPARING;out->resources_open=true;}
#endif
 return true;
}
static bool inventory_get(void*c,uint32_t i,software_update_row_v1*out){
 (void)c;software_update_status_v1 state={.struct_size=sizeof(state)};
 if(!out||out->struct_size<sizeof(*out)||!release_service->status(release_service->context,&state))return false;
 if(i<state.count)return release_service->get(release_service->context,i,out);
 if(i>=SOFTWARE_UPDATE_ROWS_MAX||!installed_inventory)return false;
 installed_app_v1 row={0};uint32_t local=i-state.count,installed=installed_inventory->count(installed_inventory->context);bool staged=local>=installed;
 if(staged){if(!installed_inventory->staged_get||!installed_inventory->staged_get(installed_inventory->context,local-installed,&row))return false;}
 else if(!installed_inventory->get(installed_inventory->context,local,&row))return false;
 *out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_CURRENT};
 #ifdef PORTABLE_APP_STORE_ACTIONS
 if(installed_inventory->remove&&installed_inventory->step&&installed_inventory->generation)out->availability=staged&&installed_inventory->install?SOFTWARE_UPDATE_INSTALL_SD:staged?SOFTWARE_UPDATE_CURRENT:SOFTWARE_UPDATE_REMOVE;
#endif
 snprintf(out->id,sizeof(out->id),"%s: %.53s",staged?"Staged":"Installed",row.name);
 snprintf(out->version,sizeof(out->version),"%.31s",row.version);
 snprintf(out->installed,sizeof(out->installed),"%.31s",row.version);
 snprintf(out->reason,sizeof(out->reason),"%s",staged?"Staged SD package; confirm to install":row.available?"Installed on SD":"Installed on SD; availability not confirmed");
 return true;
}
static bool inventory_begin(void*c,uint32_t i,uint64_t utc){
 (void)c;software_update_status_v1 state={.struct_size=sizeof(state)};
 return release_service->status(release_service->context,&state)&&i<state.count&&release_service->begin(release_service->context,i,utc);
}
static bool inventory_refresh(void*c,uint64_t utc){(void)c;
#ifdef PORTABLE_APP_STORE_ACTIONS
 inventory_result=NULL;
#endif
 return release_service->refresh(release_service->context,utc);}
static bool inventory_step(void*c){(void)c;
#ifdef PORTABLE_APP_STORE_ACTIONS
 if(inventory_removing){
  int rc=installed_inventory->step(installed_inventory->context,inventory_volume_grant.api);
  if(rc==INSTALLED_APPS_WORKING)return true;
  if(rc==INSTALLED_APPS_RETAINED){PORTABLE_APP_RETAIN();return false;}
  inventory_result=inventory_installing?(rc==INSTALLED_APPS_OK?"App installed; ready to open":rc==INSTALLED_APPS_INVALID?"Package changed or is not admissible":"Install incomplete; return Home to retry"):
   rc==INSTALLED_APPS_OK?"App removed; settings kept":rc==INSTALLED_APPS_INVALID?"This app cannot be removed":"Removal incomplete; reconnect SD and reopen";
  return inventory_drain();
 }
#endif
 return release_service->step(release_service->context);
}
static bool inventory_cancel(void*c){(void)c;return release_service->cancel(release_service->context);}
static bool inventory_activate(void*c){(void)c;return release_service->activate(release_service->context);}
static bool inventory_restart(void*c){(void)c;return release_service->restart(release_service->context);}
static const software_update_v1 inventory_view={1,sizeof(inventory_view),NULL,inventory_refresh,inventory_step,inventory_status,inventory_get,inventory_begin,inventory_cancel,inventory_activate,inventory_restart};
static bool inventory_open(void){
 release_service=us;installed_inventory=NULL;
#ifdef PORTABLE_APP_STORE_ACTIONS
 inventory_chosen=inventory_removing=false;inventory_result=NULL;inventory_volume_grant=(risc_runtime_capability_v1){.struct_size=sizeof(inventory_volume_grant)};
#endif
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
#ifdef PORTABLE_APP_STORE_ACTIONS
 if(!inventory_drain())return false;
#endif
 if(installed_inventory_grant.api&&!ur->release(&installed_inventory_grant))return false;
 installed_inventory=NULL;us=release_service;release_service=NULL;return true;
}
