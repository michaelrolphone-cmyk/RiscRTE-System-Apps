#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "SoftwareUpdateV1.h"
static bool retained;
#define PORTABLE_UPDATE_FIRMWARE 0
#define PORTABLE_APP_RETAIN() (retained=true)
static const risc_runtime_api_v1 *ur;
static const software_update_v1 *us;
#include "../../Apps/update_installed_catalog.inc"
static unsigned refreshes,begins,releases,release_rows=1;
static bool fail_release;
static uint32_t release_state=SOFTWARE_UPDATE_LIST;
static bool status(void*c,software_update_status_v1*out){(void)c;*out=(software_update_status_v1){.struct_size=sizeof(*out),.state=release_state,.count=release_rows};return true;}
static bool get(void*c,uint32_t i,software_update_row_v1*out){(void)c;assert(i<release_rows);*out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_AVAILABLE};strcpy(out->id,"release");return true;}
static bool begin(void*c,uint32_t i,uint64_t utc){(void)c;assert(i<release_rows&&utc==1);++begins;return true;}
static bool yes(void*c){(void)c;return true;}
static bool refresh(void*c,uint64_t utc){(void)c;(void)utc;return true;}
static const software_update_v1 original={1,sizeof(original),NULL,refresh,yes,status,get,begin,yes,yes,yes};
static int cached_refresh(void*c){(void)c;++refreshes;return INSTALLED_APPS_OK;}
static unsigned cached_count(void*c){(void)c;return 2;}
static bool cached_get(void*c,uint32_t i,installed_app_v1*out){(void)c;if(i>=2)return false;*out=(installed_app_v1){.available=i==0};strcpy(out->name,i?"Removed media":"New SD App");strcpy(out->version,"1.0.0");return true;}
#ifdef PORTABLE_APP_STORE_ACTIONS
static unsigned generation=1,removals,pending_steps,pending_cancel;
static uint32_t cached_generation(void*c){(void)c;return generation;}
static int cached_remove(void*c,const char*id){(void)c;(void)id;++removals;pending_steps=1000;return INSTALLED_APPS_OK;}
static int cached_step(void*c,const risc_storage_volume_api_v1*v){(void)c;assert(v);if(pending_steps){--pending_steps;return INSTALLED_APPS_WORKING;}return INSTALLED_APPS_OK;}
static const risc_storage_volume_api_v1 volume={.api_version=1,.struct_size=sizeof(volume)};
#endif
static int cached_cancel(void*c){(void)c;
#ifdef PORTABLE_APP_STORE_ACTIONS
 if(pending_cancel){--pending_cancel;return INSTALLED_APPS_WORKING;}
#endif
 return INSTALLED_APPS_OK;}
static const installed_apps_v1 cache={.api_version=1,.struct_size=sizeof(cache),.refresh=cached_refresh,.count=cached_count,.get=cached_get,.cancel=cached_cancel
#ifdef PORTABLE_APP_STORE_ACTIONS
 ,.generation=cached_generation,.remove=cached_remove,.step=cached_step
#endif
};
static bool acquire(const char*cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1*out){
#ifdef PORTABLE_APP_STORE_ACTIONS
 if(!strcmp(cap,"storage.volume")){assert(api==1&&instance==9);out->api=&volume;return true;}
#endif
 assert(!strcmp(cap,INSTALLED_APPS_CAPABILITY)&&api==1&&!instance);out->api=&cache;return true;}
static bool release(risc_runtime_capability_v1*g){++releases;if(fail_release)return false;g->api=NULL;return true;}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
int main(void){ur=&runtime;us=&original;assert(inventory_open()&&us==&inventory_view&&refreshes==1);
 software_update_status_v1 state={.struct_size=sizeof(state)};assert(us->status(NULL,&state)&&state.count==3);
 software_update_row_v1 row={.struct_size=sizeof(row)};assert(us->get(NULL,0,&row)&&!strcmp(row.id,"release"));
 assert(us->get(NULL,1,&row)&&!strcmp(row.id,"Installed: New SD App")&&row.availability==
#ifdef PORTABLE_APP_STORE_ACTIONS
 SOFTWARE_UPDATE_REMOVE
#else
 SOFTWARE_UPDATE_CURRENT
#endif
);
 assert(us->get(NULL,2,&row)&&strstr(row.reason,"not confirmed"));
 assert(us->begin(NULL,0,1)&&begins==1);assert(!us->begin(NULL,1,1)&&!us->begin(NULL,2,1)&&begins==1);
 for(unsigned i=0;i<1000;++i){assert(us->status(NULL,&state));assert(us->get(NULL,1,&row));}assert(refreshes==1);
#ifdef PORTABLE_APP_STORE_ACTIONS
 assert(inventory_choose(1));++generation;assert(!inventory_remove_selected(1)&&!removals);
 assert(inventory_choose(1)&&inventory_remove_selected(1)&&removals==1&&!retained);
 for(unsigned i=0;i<1000;++i){assert(inventory_step(NULL));assert(inventory_status(NULL,&state)&&state.state==SOFTWARE_UPDATE_PREPARING);}
 pending_cancel=3;assert(!inventory_step(NULL)&&inventory_removing&&inventory_volume_grant.api&&!retained);
 assert(!inventory_drain()&&!inventory_drain()&&inventory_volume_grant.api&&!retained);
 assert(inventory_drain()&&!inventory_removing&&!inventory_volume_grant.api&&!strcmp(inventory_result,"App removed; settings kept")&&begins==1);
#endif
 release_rows=SOFTWARE_UPDATE_ROWS_MAX;assert(us->status(NULL,&state)&&state.count==SOFTWARE_UPDATE_ROWS_MAX);assert(us->get(NULL,SOFTWARE_UPDATE_ROWS_MAX-1,&row));
 fail_release=true;assert(!inventory_close()&&installed_inventory);fail_release=false;assert(inventory_close()&&us==&original&&!installed_inventory);
 assert(releases==
#ifdef PORTABLE_APP_STORE_ACTIONS
 3
#else
 2
#endif
 &&!retained);puts("App Store cached inventory: installed rows, offline state, unchanged release authority, 1000 memory-only lookups and checked release PASS");return 0;
}
