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
static bool status(void*c,software_update_status_v1*out){(void)c;*out=(software_update_status_v1){.struct_size=sizeof(*out),.state=SOFTWARE_UPDATE_LIST,.count=release_rows};return true;}
static bool get(void*c,uint32_t i,software_update_row_v1*out){(void)c;assert(i<release_rows);*out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_AVAILABLE};strcpy(out->id,"release");return true;}
static bool begin(void*c,uint32_t i,uint64_t utc){(void)c;assert(i<release_rows&&utc==1);++begins;return true;}
static bool yes(void*c){(void)c;return true;}
static bool refresh(void*c,uint64_t utc){(void)c;(void)utc;return true;}
static const software_update_v1 original={1,sizeof(original),NULL,refresh,yes,status,get,begin,yes,yes,yes};
static int cached_refresh(void*c){(void)c;++refreshes;return INSTALLED_APPS_OK;}
static unsigned cached_count(void*c){(void)c;return 2;}
static bool cached_get(void*c,uint32_t i,installed_app_v1*out){(void)c;if(i>=2)return false;*out=(installed_app_v1){.available=i==0};strcpy(out->name,i?"Removed media":"New SD App");strcpy(out->version,"1.0.0");return true;}
static int cached_cancel(void*c){(void)c;return INSTALLED_APPS_OK;}
static const installed_apps_v1 cache={.api_version=1,.struct_size=sizeof(cache),.refresh=cached_refresh,.count=cached_count,.get=cached_get,.cancel=cached_cancel};
static bool acquire(const char*cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1*out){assert(!strcmp(cap,INSTALLED_APPS_CAPABILITY)&&api==1&&!instance);out->api=&cache;return true;}
static bool release(risc_runtime_capability_v1*g){++releases;if(fail_release)return false;g->api=NULL;return true;}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
int main(void){ur=&runtime;us=&original;assert(inventory_open()&&us==&inventory_view&&refreshes==1);
 software_update_status_v1 state={.struct_size=sizeof(state)};assert(us->status(NULL,&state)&&state.count==3);
 software_update_row_v1 row={.struct_size=sizeof(row)};assert(us->get(NULL,0,&row)&&!strcmp(row.id,"release"));
 assert(us->get(NULL,1,&row)&&!strcmp(row.id,"Installed: New SD App")&&row.availability==SOFTWARE_UPDATE_CURRENT);
 assert(us->get(NULL,2,&row)&&strstr(row.reason,"not confirmed"));
 assert(us->begin(NULL,0,1)&&begins==1);assert(!us->begin(NULL,1,1)&&!us->begin(NULL,2,1)&&begins==1);
 for(unsigned i=0;i<1000;++i){assert(us->status(NULL,&state));assert(us->get(NULL,1,&row));}assert(refreshes==1);
 release_rows=SOFTWARE_UPDATE_ROWS_MAX;assert(us->status(NULL,&state)&&state.count==SOFTWARE_UPDATE_ROWS_MAX);assert(us->get(NULL,SOFTWARE_UPDATE_ROWS_MAX-1,&row));
 fail_release=true;assert(!inventory_close()&&installed_inventory);fail_release=false;assert(inventory_close()&&us==&original&&!installed_inventory);
 assert(releases==2&&!retained);puts("App Store cached inventory: installed rows, offline state, unchanged release authority, 1000 memory-only lookups and checked release PASS");
}
