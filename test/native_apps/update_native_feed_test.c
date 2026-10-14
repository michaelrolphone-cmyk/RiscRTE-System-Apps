#define PORTABLE_APP_STORE_ACTIONS
#define PORTABLE_NATIVE_PACKAGE_FEED
#define main legacy_inventory_main
#include "update_installed_catalog_test.c"
#undef main
static unsigned remote_generation=1,remote_installs,remote_steps;
static bool remote_held;
static int remote_refresh(void*c,uint64_t utc){(void)c;assert(utc==1800000000);remote_steps=2;return INSTALLED_APPS_OK;}
static int remote_step(void*c){(void)c;if(remote_steps){--remote_steps;return INSTALLED_APPS_WORKING;}return INSTALLED_APPS_OK;}
static int remote_cancel(void*c){(void)c;return remote_held?INSTALLED_APPS_RETAINED:INSTALLED_APPS_OK;}
static uint32_t remote_count(void*c){(void)c;return 1;}
static uint32_t remote_revision(void*c){(void)c;return remote_generation;}
static bool remote_get(void*c,uint32_t i,installed_app_v1*out){(void)c;if(i)return false;*out=(installed_app_v1){.available=true};strcpy(out->id,"online-app");strcpy(out->name,"Online app");strcpy(out->version,"2.0.0");return true;}
static int remote_install(void*c,uint32_t i,uint32_t rev){(void)c;assert(!i&&rev==remote_generation);++remote_installs;return INSTALLED_APPS_OK;}
int main(void){
 release_state=SOFTWARE_UPDATE_IDLE;ur=&runtime;us=&original;assert(inventory_open());
 installed_apps_feed_v1 feed={.refresh=remote_refresh,.step=remote_step,.cancel=remote_cancel,.generation=remote_revision,.count=remote_count,.get=remote_get,.install=remote_install};inventory_feed=&feed;
 software_update_status_v1 s={.struct_size=sizeof(s)};assert(inventory_status(NULL,&s)&&s.count==4&&s.state==SOFTWARE_UPDATE_LIST);
 software_update_row_v1 row={.struct_size=sizeof(row)};assert(inventory_get(NULL,3,&row)&&row.availability==SOFTWARE_UPDATE_INSTALL_SD);
 assert(inventory_refresh(NULL,1800000000));assert(!inventory_refresh(NULL,1800000000)&&inventory_feed_checking);assert(inventory_status(NULL,&s)&&s.resources_open&&s.state==SOFTWARE_UPDATE_CATALOG);
 assert(inventory_step(NULL)&&inventory_step(NULL)&&inventory_step(NULL)&&!inventory_feed_checking);
 assert(inventory_choose(3));++remote_generation;assert(!inventory_remove_selected(3)&&!remote_installs);
 assert(inventory_choose(3)&&inventory_remove_selected(3)&&remote_installs==1&&inventory_installing);
 assert(inventory_step(NULL)&&!inventory_removing&&!inventory_volume_grant.api);
 assert(inventory_refresh(NULL,1800000000));remote_held=true;assert(!inventory_close()&&retained&&inventory_feed);
 puts("Native feed UI: available rows, refresh ownership, stale confirmation, install dispatch and retained close PASS");
}
