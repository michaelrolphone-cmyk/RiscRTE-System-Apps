#define main original_fixture_main
#define risc_runtime_get_api original_fixture_runtime
#include "resident_shell_test.c"
#undef main
#undef risc_runtime_get_api
#include "InstalledAppsV1.h"
static unsigned cached,scanned,cancelled,launched,slept;static bool sd_live,cache_live,cancel_ready;
static int cache_refresh(void*c){(void)c;assert(host_frames&&cache_live);++cached;return INSTALLED_APPS_OK;}
static uint32_t cache_count(void*c){(void)c;return 1;}
static uint32_t cache_generation(void*c){(void)c;return 1;}
static bool cache_get(void*c,uint32_t i,installed_app_v1*r){(void)c;assert(!i);*r=(installed_app_v1){.available=true};strcpy(r->launch_path,"Apps/points_in_time/image.elf");strcpy(r->name,"Points");strcpy(r->icon,"solid:f017");return true;}
static int cache_step(void*c,const risc_storage_volume_api_v1*v){(void)c;assert(sd_live&&v);++scanned;return INSTALLED_APPS_WORKING;}
static int cache_cancel(void*c){(void)c;++cancelled;return cancel_ready?INSTALLED_APPS_OK:INSTALLED_APPS_WORKING;}
static const installed_apps_v1 cache_api={1,sizeof(cache_api),NULL,cache_refresh,cache_generation,cache_count,cache_get,cache_step,cache_cancel,NULL,NULL,NULL,NULL,NULL,NULL};
static const risc_storage_volume_api_v1 sd={.api_version=1,.struct_size=sizeof(sd)};
static bool catalog_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!strcmp(n,INSTALLED_APPS_CAPABILITY)){assert(!cache_live&&v==1&&id==0);cache_live=true;g->api=&cache_api;g->slot=++grants;return true;}
 if(!strcmp(n,"storage.volume")){assert(host_frames&&cache_live&&!sd_live&&id==9);sd_live=true;g->api=&sd;g->slot=++grants;return true;}
 return acquire(n,v,id,g);
}
static bool catalog_release(risc_runtime_capability_v1*g){if(g->api==&sd){assert(cancel_ready);sd_live=false;}if(g->api==&cache_api){assert(cancel_ready&&!sd_live);cache_live=false;}return release(g);}
static int32_t catalog_child(uint64_t token,const char*path,risc_resident_result_v1*out){assert(token==1&&path&&!sd_live&&!cache_live);++launched;*out=(risc_resident_result_v1){.struct_size=sizeof(*out),.status=RISC_RESIDENT_OK};return RISC_RESIDENT_OK;}
static bool catalog_resident(risc_resident_client_v1*out){bool ok=resident_get(out);out->run_foreground=catalog_child;return ok;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){static risc_runtime_api_v1 selected;selected=runtime;selected.acquire=catalog_acquire;selected.release=catalog_release;selected.resident_shell=catalog_resident;return v==1?&selected:NULL;}
void catalog_assert(unsigned stage){
 if(stage==0)assert(!cached&&!scanned&&!sd_live&&!cache_live);
 if(stage==1)assert(cached==1&&scanned==1000&&sd_live&&cache_live&&!launched);
 if(stage==2){assert(cancelled==1000&&sd_live&&cache_live&&!launched);cancel_ready=true;}
 if(stage==3)assert(launched==1&&!sd_live&&!cache_live);
 if(stage==4){assert(!sd_live&&!cache_live);++slept;}
 if(stage==5)cancel_ready=false;
 if(stage==6){assert(!slept&&sd_live&&cache_live);cancel_ready=true;}
 if(stage==7)assert(slept==1&&!sd_live&&!cache_live);
}
int main(int argc,char**argv){assert(argc==2);fixture_dir=argv[1];load_app("host.elf",1);assert(!terminal&&!grants&&!subs&&!sd_live&&!cache_live);puts("Actual resident Home adapter: no catalog/SD before first frame, cached hydration, 1000 pending polls, 1000 launch-drain retries retain intent, release before child, deferred sleep drained before entry PASS");}
