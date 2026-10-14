#include "PortableInstalledCatalog.h"
#include <string.h>
#ifdef PORTABLE_INSTALLED_AUTHORITY_STYLES
extern const t5_app_manifest_t portable_catalog_legacy_styles[];
extern const unsigned portable_catalog_legacy_style_count;
#endif
/* Retry unavailable/exported media at most every two monotonic seconds.
 * Minimal test/legacy runtimes without health use a bounded 128-poll cooldown. */
static void maintenance_delay(portable_installed_catalog*c,const risc_runtime_api_v1*r){
 risc_runtime_health_v1 h={.struct_size=sizeof(h)};
 c->maintenance_backoff=true;c->maintenance_retry_polls=128;
 c->maintenance_clock=r->health&&r->health(&h);
 if(c->maintenance_clock)c->maintenance_retry_at=h.uptime_ms+2000u;
}
static bool maintenance_wait(portable_installed_catalog*c,const risc_runtime_api_v1*r){
 if(!c->maintenance_backoff)return false;
 if(c->maintenance_clock&&r->health){risc_runtime_health_v1 h={.struct_size=sizeof(h)};
  if(r->health(&h)){if((int32_t)(h.uptime_ms-c->maintenance_retry_at)<0)return true;c->maintenance_backoff=false;return false;}
 }
 if(c->maintenance_retry_polls){--c->maintenance_retry_polls;return true;}
 c->maintenance_backoff=false;return false;
}
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
#ifdef PORTABLE_INSTALLED_AUTHORITY_STYLES
  /* Compatibility presentation only: these rows never create installed apps.
   * A versioned SD image keeps the old directory's label and licensed icon. */
  for(unsigned j=0;j<portable_catalog_legacy_style_count;++j){
   const t5_app_manifest_t*style=&portable_catalog_legacy_styles[j];
   if(!strncmp(row.launch_path,style->file_name,strlen(style->file_name))){
    strcpy(c->rows[at].display_name,style->display_name);strcpy(c->rows[at].icon,style->icon);break;
   }
  }
#endif
  /* Deployment icon identities survive rehydration of an existing app. */
  c->rows[at].compatible=row.available;
 }
 c->generation=c->service->generation(c->service->context);
}
int portable_installed_refresh(portable_installed_catalog*c,const risc_runtime_api_v1*r,const t5_app_manifest_t*base,unsigned n){
 if(c->retained)return INSTALLED_APPS_RETAINED;
 if(c->closing)return INSTALLED_APPS_WORKING;
 c->maintenance_backoff=false;c->maintenance_retry_polls=0;
 snapshot(c,base,n);
 if(!c->service){
  c->service_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->service_grant)};
  if(!r->acquire(INSTALLED_APPS_CAPABILITY,1,0,&c->service_grant))return INSTALLED_APPS_UNAVAILABLE;
  const installed_apps_v1*s=c->service_grant.api;
  if(!s||s->api_version!=1||s->struct_size<INSTALLED_APPS_LOCAL_PREFIX_SIZE||!s->refresh||!s->count||!s->get||!s->generation||!s->step||!s->cancel){
   if(!r->release(&c->service_grant)){c->retained=true;return INSTALLED_APPS_RETAINED;}
   return INSTALLED_APPS_INVALID;
  }
  c->service=s;
  c->maintenance_enabled=s->struct_size>=INSTALLED_APPS_SETTINGS_PREFIX_SIZE&&s->settings_pending;
 }
 int rc=c->service->refresh(c->service->context);c->wanted=c->requested=true;c->resume_requested=false;snapshot(c,base,n);
 if(rc==INSTALLED_APPS_RETAINED)c->retained=true;
 return rc;
}
void portable_installed_resume(portable_installed_catalog*c){
 if(!c->retained&&(c->wanted||c->maintenance_enabled))c->resume_requested=true;
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
  if(c->wanted){int rc=portable_installed_refresh(c,r,base,n);if(rc!=INSTALLED_APPS_OK)return rc;}
  else {
   if(maintenance_wait(c,r))return INSTALLED_APPS_OK;
   c->service_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->service_grant)};
   if(!r->acquire(INSTALLED_APPS_CAPABILITY,1,0,&c->service_grant)){maintenance_delay(c,r);return INSTALLED_APPS_UNAVAILABLE;}
   const installed_apps_v1*s=c->service_grant.api;
   if(!s||s->api_version!=1||s->struct_size<INSTALLED_APPS_SETTINGS_PREFIX_SIZE||!s->settings_pending||!s->count||!s->get||!s->generation||!s->step||!s->cancel){
    if(!r->release(&c->service_grant)){c->retained=true;return INSTALLED_APPS_RETAINED;}
    c->resume_requested=c->maintenance_enabled=false;return INSTALLED_APPS_INVALID;
   }
   c->service=s;c->resume_requested=false;snapshot(c,base,n);
  }
 }
 if(!c->service)return INSTALLED_APPS_OK;
 if(!c->requested&&c->rescan_clock&&r->health){
  risc_runtime_health_v1 h={.struct_size=sizeof(h)};
  if(r->health(&h)&&(int32_t)(h.uptime_ms-c->rescan_at)>=0){
   c->rescan_clock=false;int refreshed=c->service->refresh(c->service->context);
   if(refreshed==INSTALLED_APPS_RETAINED){c->retained=true;return refreshed;}
   c->requested=c->wanted=true;
  }
 }
 if(!c->requested){
  if(maintenance_wait(c,r))return INSTALLED_APPS_OK;
  /* Poll only the append-only RAM query. Old providers remain scan-only. */
  if(c->service->struct_size<INSTALLED_APPS_SETTINGS_PREFIX_SIZE||!c->service->settings_pending||!c->service->settings_pending(c->service->context))return INSTALLED_APPS_OK;
  c->requested=true;
 }
 if(!c->volume_grant.api){
  c->volume_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->volume_grant)};
  if(!r->acquire("storage.volume",1,instance,&c->volume_grant)&&c->volume_grant.api){c->retained=true;return INSTALLED_APPS_RETAINED;}
  /* A missing SD grant is not proof that the configured internal package
   * volume is absent. The service may scan it without borrowing an SD lease. */
 }
 int rc=c->service->step(c->service->context,c->volume_grant.api);
 snapshot(c,base,n);
 if(rc==INSTALLED_APPS_RETAINED){c->retained=true;return rc;}
 if(rc==INSTALLED_APPS_WORKING)return rc;
 /* Even failed enumeration must prove cleanup before the lease is released. */
 int ended=c->service->cancel(c->service->context);
 if(ended==INSTALLED_APPS_WORKING)return ended;
 if(ended!=INSTALLED_APPS_OK){c->retained=true;return INSTALLED_APPS_RETAINED;}
 if(rc==INSTALLED_APPS_OK){c->wanted=false;c->maintenance_backoff=false;}else maintenance_delay(c,r);
 risc_runtime_health_v1 h={.struct_size=sizeof(h)};
 c->rescan_clock=r->health&&r->health(&h);if(c->rescan_clock)c->rescan_at=h.uptime_ms+30000u;
 c->requested=false;if(release_volume(c,r)!=INSTALLED_APPS_OK)return INSTALLED_APPS_RETAINED;
 return rc;
}
