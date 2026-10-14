#ifdef PORTABLE_PACKAGE_DISCOVERY
#include "PortableInstalledCatalog.h"
static portable_installed_catalog package_discovery;
static bool package_discovery_started,package_frame_submitted;
static bool package_discovery_close(void){
 if(!package_discovery_started)return true;
 for(;;){
  if(app_invocation_retained())return false;
  int rc=portable_installed_close(&package_discovery,rt);
  if(rc==INSTALLED_APPS_OK){package_discovery_started=false;return true;}
  if(rc!=INSTALLED_APPS_WORKING){optional_services_retain();return false;}
  risc_runtime_health_v1 health={.struct_size=sizeof(health)};
  if(!rt->health(&health)){optional_services_retain();return false;}
  rt->yield_ms(1);
 }
}
static bool package_discovery_step(void){
 if(!package_frame_submitted||surface.frame||failed||app_invocation_retained())return !failed;
#ifdef PORTABLE_ASYNC_RGB565
 if(canvas_token||canvas_sealed||canvas_uncertain)return true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
 if(!display_settled||alarm_modal)return true;
#endif
 if(!package_discovery_started){
  int rc=portable_installed_refresh(&package_discovery,rt,NULL,0);
  if(rc==INSTALLED_APPS_RETAINED){optional_services_retain();return false;}
  package_discovery_started=true;
 }
 uint32_t before=package_discovery.generation;
 int rc=portable_installed_step(&package_discovery,rt,PORTABLE_PACKAGE_VOLUME,NULL,0);
 if(rc==INSTALLED_APPS_RETAINED){optional_services_retain();return false;}
 // Native publication owns all routes, including built-ins; the copied service
 // client only discovers/publishes packages and never replaces that authority.
 if(before!=package_discovery.generation&&!refresh()&&app_invocation_retained())return false;
 return true;
}
#endif
