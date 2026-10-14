#ifdef PORTABLE_INSTALLED_CATALOG
#if !defined(PORTABLE_INSTALLED_CATALOG_HOST) && (!defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || !defined(PORTABLE_RESIDENT_SHELL_CLIENT))
#error Installed catalog requires the selected resident Springboard
#endif
#include "PortableInstalledCatalog.h"
#include "PortableNativeCustody.h"
static portable_installed_catalog installed_catalog_client;
#ifndef PORTABLE_INSTALLED_CATALOG_HOST
static const char *installed_deferred_return;
#endif
#ifdef PORTABLE_INSTALLED_CATALOG_HOST
static bool installed_home_started;
#endif
#include "home_provision.inc"
int portable_installed_prepare_launch(void){
#ifdef PORTABLE_HOME_PROVISION
 return home_provision_close();
#else
 int rc=portable_installed_close(&installed_catalog_client,rt);
 if(rc==INSTALLED_APPS_RETAINED)portable_adapter_retain();
 return rc;
#endif
}
void portable_installed_resume_discovery(void){portable_installed_resume(&installed_catalog_client);}
static void installed_progress(void){
#ifdef PORTABLE_HOME_PROVISION
 home_provision_progress();
#else
#ifdef PORTABLE_INSTALLED_CATALOG_HOST
 if(!installed_home_started){
  installed_home_started=true;
  int rc=portable_installed_refresh(&installed_catalog_client,rt,NULL,0);
  if(rc==INSTALLED_APPS_RETAINED){portable_adapter_retain();return;}
 }
 int rc=portable_installed_step(&installed_catalog_client,rt,PORTABLE_INSTALLED_VOLUME_INSTANCE,NULL,0);
#else
 int rc=portable_installed_step(&installed_catalog_client,rt,PORTABLE_INSTALLED_VOLUME_INSTANCE,
                              portable_catalog,portable_catalog_count);
#endif
 if(rc==INSTALLED_APPS_RETAINED)portable_adapter_retain();
#endif
}
#endif
