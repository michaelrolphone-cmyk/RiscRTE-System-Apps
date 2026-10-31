#pragma once
#include <RiscRuntimeV1.h>
#include "InstalledAppsFeedV1.h"
#include "SoftwareUpdateV1.h"
enum { APP_STORE_INVENTORY_OK=0, APP_STORE_INVENTORY_PENDING=1, APP_STORE_INVENTORY_UNAVAILABLE=2, APP_STORE_INVENTORY_INVALID=3, APP_STORE_INVENTORY_RETAINED=-9 };
enum { APP_STORE_RELEASE=0, APP_STORE_INSTALLED=1, APP_STORE_STAGED=2, APP_STORE_FEED=3 };
#define APP_STORE_REMOVE 4u
#define APP_STORE_INSTALL 5u
typedef struct {void *context;bool (*alive)(void*);void (*retain)(void*);} app_store_inventory_hooks;
typedef struct {uint32_t kind,index,local_index,generation,feed_generation;installed_app_v1 app;software_update_row_v1 release;} app_store_inventory_selection;
typedef struct {
 const risc_runtime_api_v1 *runtime;app_store_inventory_hooks hooks;
 risc_runtime_capability_v1 catalog_grant,volume_grant;
 const installed_apps_v1 *catalog;const installed_apps_feed_v1 *feed,*available_feed;
 const software_update_v1 *release;software_update_v1 facade;
 bool retained,feed_busy,operation,draining,need_network,installing;
 software_update_status_v1 copied_status;
 const char *result;
} app_store_inventory;
void app_store_inventory_init(app_store_inventory*,const risc_runtime_api_v1*,app_store_inventory_hooks);
int app_store_inventory_open(app_store_inventory*,const software_update_v1*);
const software_update_v1 *app_store_inventory_view(app_store_inventory*);
int app_store_inventory_choose(app_store_inventory*,uint32_t,app_store_inventory_selection*);
/* Confirm is only installed/staged/feed mutation; release begin remains on facade. */
int app_store_inventory_confirm(app_store_inventory*,const app_store_inventory_selection*);
bool app_store_inventory_network_needed(const app_store_inventory*);
int app_store_inventory_network_ready(app_store_inventory*,uint64_t);
int app_store_inventory_cancel(app_store_inventory*);
int app_store_inventory_close(app_store_inventory*);

/* Default Check remains release-bank catalog. Feed is an explicit optional source. */
int app_store_inventory_set_feed(app_store_inventory*,bool enabled);
/* Copies only an exact freshly revalidated installed executable route. Caller
 * closes every owner before request_launch; staged descriptors never qualify. */
int app_store_inventory_launch_route(app_store_inventory*,const app_store_inventory_selection*,char *out,size_t capacity);
