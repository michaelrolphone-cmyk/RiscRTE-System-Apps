#include "PortableInstalledCatalog.h"
#include <string.h>
static int release_volume(portable_installed_catalog*c,const risc_runtime_api_v1*r){
 if(c->volume_grant.api&&!r->release(&c->volume_grant)){c->retained=true;return INSTALLED_APPS_RETAINED;}
 return INSTALLED_APPS_OK;
}
static void snapshot(portable_installed_catalog*c,const t5_app_manifest_t*base,unsigned n){
 if(n>INSTALLED_APPS_MAX)n=INSTALLED_APPS_MAX;
 c->count=n;for(unsigned i=0;i<n;++i)c->rows[i]=base[i];
 if(!c->service)return;
 unsigned count=c->service->count(c->service->context);
 if(count>INSTALLED_APPS_MAX)return;
 for(unsigned i=0;i<count;++i){
  installed_app_v1 row={0};if(!c->service->get(c->service->context,i,&row))continue;
  /* Never truncate an admitted launch identity. The provider bounds strings. */
  if(!memchr(row.launch_path,0,sizeof(row.launch_path))||strlen(row.launch_path)>=sizeof(c->rows[0].file_name)||
     !memchr(row.name,0,sizeof(row.name))||!memchr(row.icon,0,sizeof(row.icon)))continue;
  unsigned at=0;for(;at<c->count;++at)if(!strcmp(c->rows[at].file_name,row.launch_path))break;
  if(at==INSTALLED_APPS_MAX)continue;
  if(at==c->count){++c->count;c->rows[at]=(t5_app_manifest_t){0};
   strcpy(c->rows[at].file_name,row.launch_path);strcpy(c->rows[at].display_name,row.name);strcpy(c->rows[at].icon,row.icon);}
  /* Deployment icon identities survive rehydration of an existing app. */
  c->rows[at].compatible=row.available;
 }
 c->generation=c->service->generation(c->service->context);
}
int portable_installed_refresh(portable_installed_catalog*c,const risc_runtime_api_v1*r,const t5_app_manifest_t*base,unsigned n){
 if(c->retained)return INSTALLED_APPS_RETAINED;
 if(c->closing)return INSTALLED_APPS_WORKING;
 snapshot(c,base,n);
 if(!c->service){
  c->service_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->service_grant)};
  if(!r->acquire(INSTALLED_APPS_CAPABILITY,1,0,&c->service_grant))return INSTALLED_APPS_UNAVAILABLE;
  const installed_apps_v1*s=c->service_grant.api;
  if(!s||s->api_version!=1||s->struct_size<sizeof(*s)||!s->refresh||!s->count||!s->get||!s->generation||!s->step||!s->cancel){
   if(!r->release(&c->service_grant)){c->retained=true;return INSTALLED_APPS_RETAINED;}
   return INSTALLED_APPS_INVALID;
  }
  c->service=s;
 }
 int rc=c->service->refresh(c->service->context);c->wanted=c->requested=true;c->resume_requested=false;snapshot(c,base,n);
 if(rc==INSTALLED_APPS_RETAINED)c->retained=true;
 return rc;
}
void portable_installed_resume(portable_installed_catalog*c){
 if(!c->retained&&c->wanted)c->resume_requested=true;
}
int portable_installed_close(portable_installed_catalog*c,const risc_runtime_api_v1*r){
 if(c->retained)return INSTALLED_APPS_RETAINED;
 c->closing=true;c->requested=false;
 if(c->service){int rc=c->service->cancel(c->service->context);
  if(rc==INSTALLED_APPS_WORKING)return rc;
  if(rc!=INSTALLED_APPS_OK){c->retained=true;return INSTALLED_APPS_RETAINED;}}
 if(release_volume(c,r)!=INSTALLED_APPS_OK)return INSTALLED_APPS_RETAINED;
 if(c->service_grant.api&&!r->release(&c->service_grant)){c->retained=true;return INSTALLED_APPS_RETAINED;}
 c->service=NULL;c->closing=false;return INSTALLED_APPS_OK;
}
int portable_installed_step(portable_installed_catalog*c,const risc_runtime_api_v1*r,uint64_t instance,const t5_app_manifest_t*base,unsigned n){
 if(c->retained)return INSTALLED_APPS_RETAINED;
 if(c->closing)return portable_installed_close(c,r);
 if(c->resume_requested){
  int rc=portable_installed_refresh(c,r,base,n);
  if(rc!=INSTALLED_APPS_OK)return rc;
 }
 if(!c->requested||!c->service)return INSTALLED_APPS_OK;
 if(!c->volume_grant.api){
  c->volume_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->volume_grant)};
  if(!r->acquire("storage.volume",1,instance,&c->volume_grant)){c->requested=false;return INSTALLED_APPS_UNAVAILABLE;}
 }
 int rc=c->service->step(c->service->context,c->volume_grant.api);
 snapshot(c,base,n);
 if(rc==INSTALLED_APPS_RETAINED){c->retained=true;return rc;}
 if(rc==INSTALLED_APPS_WORKING)return rc;
 /* Even failed enumeration must prove cleanup before the lease is released. */
 int ended=c->service->cancel(c->service->context);
 if(ended==INSTALLED_APPS_WORKING)return ended;
 if(ended!=INSTALLED_APPS_OK){c->retained=true;return INSTALLED_APPS_RETAINED;}
 if(rc==INSTALLED_APPS_OK)c->wanted=false;
 c->requested=false;if(release_volume(c,r)!=INSTALLED_APPS_OK)return INSTALLED_APPS_RETAINED;
 return rc;
}
