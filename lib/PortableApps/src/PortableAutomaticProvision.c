#include "PortableAutomaticProvision.h"
#include <string.h>
static bool safe(void*p){portable_automatic_provision*c=p;return !c->retained&&c->hooks.safe&&c->hooks.safe(c->hooks.context);}
static int retain(portable_automatic_provision*c){c->retained=true;if(c->hooks.retain)c->hooks.retain(c->hooks.context);return INSTALLED_APPS_RETAINED;}
static int acquire(portable_automatic_provision*c,const char*cap,uint64_t instance,risc_runtime_capability_v1*g){
 if(!safe(c))return retain(c);
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=c->hooks.runtime->acquire(cap,1,instance,g);
 if(!safe(c)||(!ok&&g->api))return retain(c);
 return ok?INSTALLED_APPS_OK:INSTALLED_APPS_UNAVAILABLE;
}
void portable_automatic_init(portable_automatic_provision*c,const portable_automatic_hooks*h){memset(c,0,sizeof(*c));c->hooks=*h;}
int portable_automatic_close(portable_automatic_provision*c){
 if(c->retained)return INSTALLED_APPS_RETAINED;
 c->closing=true;
 if(!c->service_grant.api&&!c->wifi_grant.api&&!c->settings_grant.api&&!c->volume_grant.api&&!c->network_started&&!c->lease){
  c->service=NULL;c->automatic=NULL;c->phase=0;c->closing=false;return INSTALLED_APPS_OK;
 }
 if(!safe(c))return retain(c);
 if(c->automatic){int rc=c->automatic->cancel(c->service->context);if(rc==INSTALLED_APPS_WORKING)return rc;if(rc!=INSTALLED_APPS_OK)return retain(c);}
 else if(c->service){int rc=c->service->cancel(c->service->context);if(rc==INSTALLED_APPS_WORKING)return rc;if(rc!=INSTALLED_APPS_OK)return retain(c);}
 if(c->lease){int rc=portable_network_session_service_end(&c->network);if(rc==PORTABLE_NETWORK_SERVICE_PENDING)return INSTALLED_APPS_WORKING;if(rc!=PORTABLE_NETWORK_SERVICE_OK)return retain(c);c->lease=false;}
 if(c->network_started){int rc=portable_network_session_close(&c->network);if(rc==PORTABLE_NETWORK_CLOSE_PENDING)return INSTALLED_APPS_WORKING;if(rc!=PORTABLE_NETWORK_CLOSED)return retain(c);c->network_started=false;}
 risc_runtime_capability_v1*grants[]={&c->volume_grant,&c->settings_grant,&c->wifi_grant,&c->service_grant};
 for(unsigned i=0;i<4;++i)if(grants[i]->api){if(!safe(c)||!c->hooks.runtime->release(grants[i])||!safe(c))return retain(c);memset(grants[i],0,sizeof(*grants[i]));}
 c->service=NULL;c->automatic=NULL;c->phase=0;c->closing=false;return INSTALLED_APPS_OK;
}
int portable_automatic_step(portable_automatic_provision*c,uint32_t now){
 if(!safe(c))return retain(c);
 if(c->closing)return portable_automatic_close(c);
 if(c->delayed&&(int32_t)(now-c->retry_at)<0)return INSTALLED_APPS_OK;
 c->delayed=false;int rc=INSTALLED_APPS_OK;
 if(!c->phase){
  if(!c->service){
   rc=acquire(c,INSTALLED_APPS_CAPABILITY,0,&c->service_grant);if(rc!=INSTALLED_APPS_OK)goto finish;
   c->service=c->service_grant.api;c->automatic=installed_apps_automatic(c->service);
   if(!c->automatic){c->service=NULL;rc=INSTALLED_APPS_INVALID;goto finish;}
  }
  if(!c->hooks.prepare||!c->hooks.prepare(c->hooks.context))return safe(c)?INSTALLED_APPS_WORKING:retain(c);
  rc=c->service->refresh(c->service->context);if(rc!=INSTALLED_APPS_OK&&rc!=INSTALLED_APPS_WORKING)goto finish;
  if(c->hooks.sd_enabled){rc=acquire(c,"storage.volume",c->hooks.volume_instance,&c->volume_grant);if(rc==INSTALLED_APPS_RETAINED)return rc;}
  c->phase=1;return INSTALLED_APPS_WORKING;
 }
 if(c->phase==1){
  // Discover locally before requiring a network; cached publication already
  // happened in refresh. A missing/exported SD does not suppress internal work.
  rc=c->service->step(c->service->context,c->volume_grant.api);
  if(rc==INSTALLED_APPS_WORKING)return rc;
  if(rc==INSTALLED_APPS_RETAINED)return retain(c);
  // A durable interrupted download asks for networking before discovery.
  c->phase=2;return INSTALLED_APPS_WORKING;
 }
 if(c->phase==2){
  if(c->hooks.network_allowed&&!c->hooks.network_allowed(c->hooks.context)){rc=INSTALLED_APPS_UNAVAILABLE;goto finish;}
  uint64_t utc=0;if(!c->hooks.utc||!c->hooks.utc(c->hooks.context,&utc)){rc=INSTALLED_APPS_UNAVAILABLE;goto finish;}
  rc=acquire(c,"net.wifi",c->hooks.wifi_instance,&c->wifi_grant);if(rc!=INSTALLED_APPS_OK)goto finish;
  rc=acquire(c,"storage.key-value",c->hooks.settings_namespace,&c->settings_grant);if(rc!=INSTALLED_APPS_OK)goto finish;
  portable_network_session_init(&c->network,c->wifi_grant.api,c->settings_grant.api,safe,c);c->network_started=true;
  int n=portable_network_session_start(&c->network,now);if(n==PORTABLE_NETWORK_RETAINED)return retain(c);
  if(n!=PORTABLE_NETWORK_PENDING&&n!=PORTABLE_NETWORK_READY){rc=INSTALLED_APPS_UNAVAILABLE;goto finish;}
  c->phase=3;return INSTALLED_APPS_WORKING;
 }
 if(c->phase==3){
  int n=portable_network_session_step(&c->network,now);if(n==PORTABLE_NETWORK_PENDING)return INSTALLED_APPS_WORKING;
  if(n==PORTABLE_NETWORK_RETAINED)return retain(c);
  if(n!=PORTABLE_NETWORK_READY){rc=INSTALLED_APPS_UNAVAILABLE;goto finish;}
  int lease=portable_network_session_service_begin(&c->network);if(lease==PORTABLE_NETWORK_SERVICE_PENDING)return INSTALLED_APPS_WORKING;
  if(lease!=PORTABLE_NETWORK_SERVICE_OK)return retain(c);
  c->lease=true;
  uint64_t utc=0;if(!c->hooks.utc(c->hooks.context,&utc)){rc=INSTALLED_APPS_UNAVAILABLE;goto finish;}
  rc=c->automatic->begin(c->service->context,c->hooks.profile,utc);if(rc!=INSTALLED_APPS_OK)goto finish;
  c->phase=4;return INSTALLED_APPS_WORKING;
 }
 rc=c->automatic->step(c->service->context,c->volume_grant.api);
 if(rc==INSTALLED_APPS_WORKING)return rc;
finish:
 if(rc==INSTALLED_APPS_RETAINED)return retain(c);
 c->retry_at=now+30000u;c->delayed=true;
 int closed=portable_automatic_close(c);return closed==INSTALLED_APPS_OK?rc:closed;
}
