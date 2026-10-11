#ifdef PORTABLE_INSTALLED_CATALOG
#if !defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || !defined(PORTABLE_RESIDENT_SHELL_CLIENT)
#error Installed catalog requires the selected resident Springboard
#endif
#include "PortableInstalledCatalog.h"
#include "PortableNativeCustody.h"
static portable_installed_catalog installed_catalog_client;
static const char *installed_deferred_return;
int portable_installed_prepare_launch(void){
 int rc=portable_installed_close(&installed_catalog_client,rt);
 if(rc==INSTALLED_APPS_RETAINED)portable_adapter_retain();
 return rc;
}
static void installed_progress(void){
 int rc=portable_installed_step(&installed_catalog_client,rt,PORTABLE_INSTALLED_VOLUME_INSTANCE,
                              portable_catalog,portable_catalog_count);
 if(rc==INSTALLED_APPS_RETAINED)portable_adapter_retain();
}
#endif
