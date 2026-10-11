/* Actual resident checkpoint code and catalog client, synthetic peripherals. */
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
#include "PortableInstalledCatalog.h"
#define PORTABLE_INSTALLED_CATALOG
static portable_installed_catalog installed_catalog_client;
static bool storage_pending=true;
static unsigned resumed,storage_releases;
static int cancel_scan(void*c){(void)c;return storage_pending?INSTALLED_APPS_WORKING:INSTALLED_APPS_OK;}
static bool release_scan(risc_runtime_capability_v1*g){assert(!storage_pending);++storage_releases;g->api=NULL;return true;}
static const installed_apps_v1 catalog_service={.api_version=1,.struct_size=sizeof(catalog_service),.cancel=cancel_scan};
static const risc_runtime_api_v1 catalog_runtime={.release=release_scan};
int portable_installed_prepare_launch(void){return portable_installed_close(&installed_catalog_client,&catalog_runtime);}
void portable_installed_resume_discovery(void){++resumed;portable_installed_resume(&installed_catalog_client);}
#include "wifi_resident_seam.inc"
int main(void){
 workflow_start(true);resident_fixture_start();
 installed_catalog_client.service=&catalog_service;
 installed_catalog_client.service_grant.api=&catalog_service;
 installed_catalog_client.volume_grant.api=(void*)1;
 installed_catalog_client.requested=installed_catalog_client.wanted=true;
 resident_controls_requested=true;
 assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY);
 for(unsigned i=0;i<1000;++i){
  int status=resident_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS);
  assert(status==RISC_RESIDENT_BUSY);assert(resident_controls_complete(status));
  assert(resident_controls_requested&&!checkpoints&&!storage_releases&&!resumed);
 }
 storage_pending=false;
 int status=resident_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS);
 assert(status==RISC_RESIDENT_OK);assert(resident_controls_complete(status));
 assert(!resident_controls_requested&&checkpoints==1&&storage_releases==2);
 assert(resumed==1&&installed_catalog_client.resume_requested&&installed_catalog_client.wanted);
 assert(!native_custody_retained);
 puts("Installed resident controls: 1000 BUSY retries preserve one gesture; dispatch after cleanup; dismissal rearms discovery PASS");
}
