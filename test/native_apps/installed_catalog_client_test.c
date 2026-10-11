#include "PortableInstalledCatalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_INSTALLED_AUTHORITY_STYLES
const t5_app_manifest_t portable_catalog_legacy_styles[]={ {.display_name="Legacy appearance",.file_name="Apps/new/",.icon="solid:f02d"}, {.display_name="Not installed",.file_name="Apps/absent/",.icon="solid:f02d"} };
const unsigned portable_catalog_legacy_style_count=2;
#endif
static unsigned volume_acquires,releases,cancels,steps;static bool pending=true,refuse_release,dirty_settings;
static bool settings_pending(void*p){(void)p;return dirty_settings;}
static int refresh(void*p){(void)p;return 0;}
static uint32_t generation(void*p){(void)p;return 7;}
static uint32_t count(void*p){(void)p;return 2;}
static bool get(void*p,uint32_t i,installed_app_v1*out){(void)p;*out=(installed_app_v1){.available=true};strcpy(out->name,"Discovered");strcpy(out->icon,"solid:f15b");strcpy(out->launch_path,i?"Apps/new/new.elf":"reader.elf");return true;}
static int step(void*p,const risc_storage_volume_api_v1*v){(void)p;assert(v);++steps;return pending?1:0;}
static int cancel(void*p){(void)p;++cancels;return pending?1:0;}
static installed_apps_v1 api={1,sizeof(api),NULL,refresh,generation,count,get,step,cancel,NULL,NULL,NULL,NULL,NULL,NULL};
static bool acquire(const char*cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){assert(version==1);if(!strcmp(cap,"storage.volume")){assert(instance==42);++volume_acquires;g->api=(void*)1;}else {assert(!strcmp(cap,INSTALLED_APPS_CAPABILITY));g->api=&api;}return true;}
static bool release(risc_runtime_capability_v1*g){assert(!pending);if(refuse_release)return false;++releases;g->api=NULL;return true;}
int main(void){
 const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
 const t5_app_manifest_t base[]={ {.display_name="Reader",.file_name="reader.elf",.icon="solid:f02d",.compatible=true} };
 portable_installed_catalog c={0};
 assert(portable_installed_refresh(&c,&runtime,base,1)==0);
 assert(volume_acquires==0&&steps==0&&c.count==2);
#ifdef PORTABLE_INSTALLED_AUTHORITY_STYLES
 assert(!strcmp(c.rows[1].display_name,"Legacy appearance")&&!strcmp(c.rows[1].icon,"solid:f02d"));
#endif
 assert(!strcmp(c.rows[0].icon,"solid:f02d")&&!strcmp(c.rows[1].file_name,"Apps/new/new.elf"));
 for(unsigned i=0;i<1000;++i)assert(portable_installed_step(&c,&runtime,42,base,1)==1);
 assert(volume_acquires==1&&steps==1000&&releases==0);
 assert(portable_installed_close(&c,&runtime)==1&&releases==0);
 assert(portable_installed_refresh(&c,&runtime,base,1)==1);
 pending=false;assert(portable_installed_step(&c,&runtime,42,base,1)==0);
 assert(releases==2&&!c.service&&!c.volume_grant.api);
 /* A dismissed modal rearms the original scan after checked cancellation. */
 assert(c.wanted);portable_installed_resume(&c);
 pending=true;assert(portable_installed_step(&c,&runtime,42,base,1)==1&&volume_acquires==2);
 assert(portable_installed_close(&c,&runtime)==1);pending=false;
 assert(portable_installed_step(&c,&runtime,42,base,1)==0&&releases==4);
 /* A refused launch likewise resumes discovery instead of losing intent. */
 portable_installed_resume(&c);pending=true;
 assert(portable_installed_step(&c,&runtime,42,base,1)==1&&volume_acquires==3);
 pending=false;assert(portable_installed_step(&c,&runtime,42,base,1)==0&&releases==5&&!c.wanted);
 portable_installed_resume(&c);assert(!c.resume_requested);
 /* Old providers have no idle suffix and must not reacquire the card. */
 unsigned idle_acquires=volume_acquires;api.struct_size=offsetof(installed_apps_v1,settings_pending);api.settings_pending=settings_pending;dirty_settings=true;
 assert(portable_installed_step(&c,&runtime,42,base,1)==0&&volume_acquires==idle_acquires);
 api.struct_size=sizeof(api);pending=true;
 assert(portable_installed_step(&c,&runtime,42,base,1)==1&&volume_acquires==idle_acquires+1);
 /* USB/Home cancellation holds both grants until the owned write drains. */
 assert(portable_installed_close(&c,&runtime)==1&&c.volume_grant.api);
 dirty_settings=pending=false;assert(portable_installed_step(&c,&runtime,42,base,1)==0&&!c.volume_grant.api);
 assert(portable_installed_refresh(&c,&runtime,base,1)==0);
 assert(portable_installed_step(&c,&runtime,42,base,1)==0);
 refuse_release=true;assert(portable_installed_close(&c,&runtime)==INSTALLED_APPS_RETAINED);
 unsigned before=cancels;assert(portable_installed_close(&c,&runtime)==INSTALLED_APPS_RETAINED&&cancels==before);
 puts("Installed catalog client: cache-first snapshot, preserved icons, unknown app, 1000 polls, cancellation and retained lease PASS");
}
