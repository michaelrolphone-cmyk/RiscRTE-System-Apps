#include "app_store_inventory.h"
#include <string.h>
#include <stdio.h>
static int retain(app_store_inventory*s){if(!s->retained){s->retained=true;if(s->hooks.retain)s->hooks.retain(s->hooks.context);}return APP_STORE_INVENTORY_RETAINED;}
static bool live(app_store_inventory*s){if(s->retained)return false;if(!s->runtime||risc_runtime_get_api(1)!=s->runtime||s->runtime->api_version!=1||s->runtime->struct_size<offsetof(risc_runtime_api_v1,confirm_boot)||!s->runtime->acquire||!s->runtime->release||(s->hooks.alive&&!s->hooks.alive(s->hooks.context))){retain(s);return false;}return true;}
#define CALL(s,expr,out) do {if(!live(s))return APP_STORE_INVENTORY_RETAINED;(out)=(expr);if(!live(s))return APP_STORE_INVENTORY_RETAINED;}while(0)
static bool empty(const risc_runtime_capability_v1*g){return g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation;}
static int release_grant(app_store_inventory*s,risc_runtime_capability_v1*g){
 if(empty(g))return APP_STORE_INVENTORY_OK;
 if(!g->api||!g->slot||!g->generation||g->struct_size!=sizeof(*g))return retain(s);
 bool ok;CALL(s,s->runtime->release(g),ok);
 return ok&&empty(g)?APP_STORE_INVENTORY_OK:retain(s);
}
static int acquire(app_store_inventory*s,const char*name,risc_runtime_capability_v1*g){
 if(!empty(g))return retain(s);
 bool ok;CALL(s,s->runtime->acquire(name,1,0,g),ok);
 if(!ok)return empty(g)?APP_STORE_INVENTORY_UNAVAILABLE:retain(s);
 return g->struct_size==sizeof(*g)&&g->api&&g->slot&&g->generation?APP_STORE_INVENTORY_OK:retain(s);
}
static int result(app_store_inventory*s,int rc){
 switch(rc){case INSTALLED_APPS_OK:return APP_STORE_INVENTORY_OK;case INSTALLED_APPS_WORKING:return APP_STORE_INVENTORY_PENDING;case INSTALLED_APPS_NEEDS_NETWORK:s->need_network=true;return APP_STORE_INVENTORY_PENDING;case INSTALLED_APPS_UNAVAILABLE:return APP_STORE_INVENTORY_UNAVAILABLE;case INSTALLED_APPS_INVALID:return APP_STORE_INVENTORY_INVALID;default:return retain(s);}
}
static bool valid_row(const installed_app_v1*a){return a->id[0]&&memchr(a->id,0,sizeof(a->id))&&memchr(a->name,0,sizeof(a->name))&&memchr(a->version,0,sizeof(a->version))&&memchr(a->icon,0,sizeof(a->icon))&&memchr(a->launch_path,0,sizeof(a->launch_path));}
static int release_status(app_store_inventory*s,software_update_status_v1*out){
 if(!s->release)return APP_STORE_INVENTORY_UNAVAILABLE;
 *out=(software_update_status_v1){.struct_size=sizeof(*out)};bool ok;CALL(s,s->release->status(s->release->context,out),ok);
 if(!ok)return APP_STORE_INVENTORY_UNAVAILABLE;
 if(out->struct_size!=sizeof(*out)||out->count>SOFTWARE_UPDATE_ROWS_MAX||out->state>SOFTWARE_UPDATE_ACTIVATION_UNKNOWN)return retain(s);
 if(out->state==SOFTWARE_UPDATE_RETAINED)return retain(s);
 return APP_STORE_INVENTORY_OK;
}
static int counts(app_store_inventory*s,uint32_t*installed,uint32_t*staged,uint32_t*feed){
 *installed=*staged=*feed=0;if(!s->catalog)return APP_STORE_INVENTORY_OK;
 CALL(s,s->catalog->count(s->catalog->context),*installed);
 if(s->catalog->staged_count){CALL(s,s->catalog->staged_count(s->catalog->context),*staged);}
 if(s->feed){CALL(s,s->feed->count(s->catalog->context),*feed);}
 if(*installed>INSTALLED_APPS_MAX||*staged>INSTALLED_APPS_MAX||*feed>INSTALLED_APPS_MAX)return retain(s);
 return APP_STORE_INVENTORY_OK;
}
int app_store_inventory_choose(app_store_inventory*s,uint32_t index,app_store_inventory_selection*out){
 if(!out)return APP_STORE_INVENTORY_INVALID;
 if(!live(s))return APP_STORE_INVENTORY_RETAINED;
 if(s->operation||s->draining||s->feed_busy)return APP_STORE_INVENTORY_PENDING;
 software_update_status_v1 status;int rc=release_status(s,&status);if(rc)return rc;
 if(index>=SOFTWARE_UPDATE_ROWS_MAX)return APP_STORE_INVENTORY_INVALID;
 app_store_inventory_selection v={.index=index};bool ok;
 if(index<status.count){v.kind=APP_STORE_RELEASE;v.local_index=index;v.release.struct_size=sizeof(v.release);CALL(s,s->release->get(s->release->context,index,&v.release),ok);if(!ok)return APP_STORE_INVENTORY_INVALID;if(v.release.struct_size!=sizeof(v.release)||!memchr(v.release.id,0,sizeof(v.release.id))||!memchr(v.release.version,0,sizeof(v.release.version)))return retain(s);*out=v;return 0;}
 if(!s->catalog)return APP_STORE_INVENTORY_UNAVAILABLE;
 if(s->catalog->generation){CALL(s,s->catalog->generation(s->catalog->context),v.generation);}
 if(s->feed){CALL(s,s->feed->generation(s->catalog->context),v.feed_generation);}
 uint32_t n,t,f;rc=counts(s,&n,&t,&f);if(rc)return rc;
 uint32_t i=index-status.count;
 if(i<n){v.kind=APP_STORE_INSTALLED;v.local_index=i;CALL(s,s->catalog->get(s->catalog->context,i,&v.app),ok);}
 else if(i-n<t&&s->catalog->staged_get){v.kind=APP_STORE_STAGED;v.local_index=i-n;CALL(s,s->catalog->staged_get(s->catalog->context,i-n,&v.app),ok);}
 else if(i>=n+t&&i-n-t<f&&s->feed){v.kind=APP_STORE_FEED;v.local_index=i-n-t;CALL(s,s->feed->get(s->catalog->context,i-n-t,&v.app),ok);}
 else return APP_STORE_INVENTORY_INVALID;
 if(!ok)return APP_STORE_INVENTORY_INVALID;
 if(!valid_row(&v.app))return retain(s);
 uint32_t generation;
 if(s->catalog->generation){CALL(s,s->catalog->generation(s->catalog->context),generation);if(generation!=v.generation)return APP_STORE_INVENTORY_INVALID;}
 if(s->feed){CALL(s,s->feed->generation(s->catalog->context),generation);if(generation!=v.feed_generation)return APP_STORE_INVENTORY_INVALID;}
 *out=v;return APP_STORE_INVENTORY_OK;
}
static bool same(const app_store_inventory_selection*a,const app_store_inventory_selection*b){return a->kind==b->kind&&a->index==b->index&&a->local_index==b->local_index&&a->generation==b->generation&&a->feed_generation==b->feed_generation&&a->app.available==b->app.available&&!strcmp(a->app.id,b->app.id)&&!strcmp(a->app.version,b->app.version)&&!strcmp(a->app.launch_path,b->app.launch_path);}
int app_store_inventory_confirm(app_store_inventory*s,const app_store_inventory_selection*chosen){
 if(!chosen||chosen->kind==APP_STORE_RELEASE||!valid_row(&chosen->app))return APP_STORE_INVENTORY_INVALID;
 app_store_inventory_selection fresh;int rc=app_store_inventory_choose(s,chosen->index,&fresh);if(rc)return rc;if(!same(chosen,&fresh))return APP_STORE_INVENTORY_INVALID;
 if(!s->catalog->generation||!s->catalog->step||!s->catalog->cancel||(fresh.kind==APP_STORE_INSTALLED&&!s->catalog->remove)||(fresh.kind==APP_STORE_STAGED&&!s->catalog->install)||(fresh.kind==APP_STORE_FEED&&!fresh.app.available))return APP_STORE_INVENTORY_UNAVAILABLE;
 rc=acquire(s,"storage.volume",&s->volume_grant);if(rc)return rc;
 const risc_storage_volume_api_v1*v=s->volume_grant.api;
 if(v->api_version!=1||v->struct_size<sizeof(*v))return retain(s);
 int native;
 if(fresh.kind==APP_STORE_INSTALLED){CALL(s,s->catalog->remove(s->catalog->context,fresh.app.id),native);}
 else if(fresh.kind==APP_STORE_STAGED){CALL(s,s->catalog->install(s->catalog->context,fresh.app.id,fresh.app.version),native);}
 else {CALL(s,s->feed->install(s->catalog->context,fresh.local_index,fresh.feed_generation),native);}
 rc=result(s,native);if(rc==APP_STORE_INVENTORY_RETAINED)return rc;
 s->installing=fresh.kind!=APP_STORE_INSTALLED;s->result=NULL;
 /* Any accepted/pending submission owns the volume until checked cancel. */
 if(rc==0||rc==APP_STORE_INVENTORY_PENDING){s->operation=true;return 0;}
 int closed=release_grant(s,&s->volume_grant);return closed?closed:rc;
}
int app_store_inventory_cancel(app_store_inventory*s){
 if(!live(s))return APP_STORE_INVENTORY_RETAINED;
 if(s->feed_busy){s->draining=true;int native;CALL(s,s->feed->cancel(s->catalog->context),native);int rc=result(s,native);if(rc)return rc;s->feed_busy=false;}
 if(s->catalog){
  s->draining=true;int native;CALL(s,s->catalog->cancel(s->catalog->context),native);int rc=result(s,native);if(rc==APP_STORE_INVENTORY_RETAINED)return rc;if(rc)return APP_STORE_INVENTORY_PENDING;
  rc=release_grant(s,&s->volume_grant);if(rc)return rc;
  s->operation=s->draining=s->need_network=false;
 }
 return APP_STORE_INVENTORY_OK;
}
static int tick(app_store_inventory*s){
 if(!live(s))return APP_STORE_INVENTORY_RETAINED;
 if(s->draining)return app_store_inventory_cancel(s);
 if(s->feed_busy){int native;CALL(s,s->feed->step(s->catalog->context),native);int rc=result(s,native);if(rc==APP_STORE_INVENTORY_RETAINED||rc==APP_STORE_INVENTORY_PENDING)return rc;s->result=rc?"Catalog unavailable; previous list kept":"Catalog ready; select an app";/* cancel settles provider custody even after terminal result */return app_store_inventory_cancel(s);}
 if(s->operation){int native;CALL(s,s->catalog->step(s->catalog->context,s->volume_grant.api),native);int rc=result(s,native);if(rc==APP_STORE_INVENTORY_RETAINED||rc==APP_STORE_INVENTORY_PENDING)return rc;s->result=s->installing?(rc==0?"App installed; ready to open":rc==APP_STORE_INVENTORY_INVALID?"Package changed or is not admissible":"Install incomplete; return Home to retry"):(rc==0?"App removed; settings kept":rc==APP_STORE_INVENTORY_INVALID?"This app cannot be removed":"Removal incomplete; reconnect SD and reopen");s->draining=true;return app_store_inventory_cancel(s);}
 bool ok;CALL(s,s->release->step(s->release->context),ok);return ok?0:APP_STORE_INVENTORY_PENDING;
}
static bool view_step(void*c){int rc=tick(c);return rc==0||rc==APP_STORE_INVENTORY_PENDING;}
static bool view_status(void*c,software_update_status_v1*out){
 app_store_inventory*s=c;if(!out||out->struct_size<sizeof(*out)||!live(s))return false;
 if(s->draining){*out=s->copied_status;out->struct_size=sizeof(*out);out->state=SOFTWARE_UPDATE_PREPARING;out->resources_open=true;return true;}
 if(release_status(s,out))return false;
 uint32_t n,t,f;if(counts(s,&n,&t,&f))return false;
 uint32_t room=SOFTWARE_UPDATE_ROWS_MAX-out->count,extra=n+t+f;out->count+=extra<room?extra:room;
 if(s->feed&&!out->resources_open&&(out->state==SOFTWARE_UPDATE_IDLE||out->state==SOFTWARE_UPDATE_LIST||out->state==SOFTWARE_UPDATE_ERROR)){out->state=SOFTWARE_UPDATE_LIST;out->error=0;out->done=out->total=0;}
 if(s->feed_busy){out->state=SOFTWARE_UPDATE_CATALOG;out->resources_open=true;}
 if(s->operation||s->draining){out->state=SOFTWARE_UPDATE_PREPARING;out->resources_open=true;}
 s->copied_status=*out;return true;
}
static bool view_get(void*c,uint32_t index,software_update_row_v1*out){
 app_store_inventory*s=c;if(!out||out->struct_size<sizeof(*out))return false;
 app_store_inventory_selection v;if(app_store_inventory_choose(s,index,&v))return false;
 if(v.kind==APP_STORE_RELEASE){*out=v.release;return true;}
 *out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_CURRENT};
 if(s->catalog->step&&s->catalog->generation){if(v.kind==APP_STORE_INSTALLED&&s->catalog->remove)out->availability=APP_STORE_REMOVE;else if((v.kind==APP_STORE_STAGED&&s->catalog->install)||(v.kind==APP_STORE_FEED&&v.app.available))out->availability=APP_STORE_INSTALL;}
 snprintf(out->id,sizeof(out->id),"%s: %.53s",v.kind==APP_STORE_FEED?"Available":v.kind==APP_STORE_STAGED?"Staged":"Installed",v.app.name);
 snprintf(out->version,sizeof(out->version),"%.31s",v.app.version);snprintf(out->installed,sizeof(out->installed),"%.31s",v.app.version);
 snprintf(out->reason,sizeof(out->reason),"%s",v.kind==APP_STORE_FEED?"Verified catalog; confirm install/update":v.kind==APP_STORE_STAGED?"Staged SD package; confirm to install":v.app.available?"Installed":"Installed; availability not confirmed");return true;
}
static bool view_refresh(void*c,uint64_t utc){app_store_inventory*s=c;if(!live(s)||s->operation||s->draining||s->feed_busy)return false;s->result=NULL;if(s->feed){int native=s->feed->refresh(s->catalog->context,utc);if(!live(s))return false;int rc=result(s,native);if(rc==0||rc==1){s->feed_busy=true;return true;}return false;}bool ok=s->release->refresh(s->release->context,utc);return live(s)&&ok;}
static bool view_begin(void*c,uint32_t index,uint64_t utc){app_store_inventory*s=c;software_update_status_v1 st;if(s->operation||s->draining||s->feed_busy||release_status(s,&st)||index>=st.count)return false;bool ok=s->release->begin(s->release->context,index,utc);return live(s)&&ok;}
static bool view_cancel(void*c){app_store_inventory*s=c;if(app_store_inventory_cancel(s))return false;bool ok=s->release->cancel(s->release->context);return live(s)&&ok;}
static bool view_activate(void*c){app_store_inventory*s=c;if(!live(s)||s->operation||s->draining||s->feed_busy)return false;bool ok=s->release->activate(s->release->context);return live(s)&&ok;}
static bool view_restart(void*c){app_store_inventory*s=c;if(!live(s)||s->operation||s->draining||s->feed_busy)return false;bool ok=s->release->restart(s->release->context);return live(s)&&ok;}
void app_store_inventory_init(app_store_inventory*s,const risc_runtime_api_v1*rt,app_store_inventory_hooks hooks){memset(s,0,sizeof(*s));s->runtime=rt;s->hooks=hooks;s->catalog_grant.struct_size=sizeof(s->catalog_grant);s->volume_grant.struct_size=sizeof(s->volume_grant);s->facade=(software_update_v1){1,sizeof(s->facade),s,view_refresh,view_step,view_status,view_get,view_begin,view_cancel,view_activate,view_restart};}
int app_store_inventory_open(app_store_inventory*s,const software_update_v1*release){
 if(!live(s))return APP_STORE_INVENTORY_RETAINED;
 if(s->release||!empty(&s->catalog_grant)||!empty(&s->volume_grant))return APP_STORE_INVENTORY_INVALID;
 if(!release||release->api_version!=1||release->struct_size<sizeof(*release)||!release->refresh||!release->step||!release->status||!release->get||!release->begin||!release->cancel||!release->activate||!release->restart)return APP_STORE_INVENTORY_UNAVAILABLE;
 s->release=release;int rc=acquire(s,INSTALLED_APPS_CAPABILITY,&s->catalog_grant);if(rc)return rc;
 const installed_apps_v1*a=s->catalog_grant.api;
 if(a->api_version!=1||a->struct_size<INSTALLED_APPS_LOCAL_PREFIX_SIZE||!a->refresh||!a->count||!a->get||!a->cancel){int closed=release_grant(s,&s->catalog_grant);return closed?closed:APP_STORE_INVENTORY_UNAVAILABLE;}
 int native;CALL(s,a->refresh(a->context),native);rc=result(s,native);if(rc==APP_STORE_INVENTORY_RETAINED)return rc;
 if(rc!=0&&rc!=APP_STORE_INVENTORY_PENDING){int closed=release_grant(s,&s->catalog_grant);return closed?closed:APP_STORE_INVENTORY_UNAVAILABLE;}
 s->catalog=a;s->available_feed=installed_apps_feed(a);s->feed=NULL;return APP_STORE_INVENTORY_OK;
}
const software_update_v1*app_store_inventory_view(app_store_inventory*s){return live(s)&&s->release?&s->facade:NULL;}
bool app_store_inventory_network_needed(const app_store_inventory*s){return !s->retained&&s->need_network;}
int app_store_inventory_network_ready(app_store_inventory*s,uint64_t utc){if(!live(s))return APP_STORE_INVENTORY_RETAINED;if(!s->need_network||!s->catalog||s->catalog->struct_size<offsetof(installed_apps_v1,settings_pending)||!s->catalog->network_ready)return APP_STORE_INVENTORY_UNAVAILABLE;int native;CALL(s,s->catalog->network_ready(s->catalog->context,utc),native);int rc=result(s,native);if(!rc)s->need_network=false;return rc;}
int app_store_inventory_close(app_store_inventory*s){int rc=app_store_inventory_cancel(s);if(rc)return rc;rc=release_grant(s,&s->catalog_grant);if(rc)return rc;s->catalog=NULL;s->feed=NULL;s->available_feed=NULL;s->release=NULL;return APP_STORE_INVENTORY_OK;}

int app_store_inventory_set_feed(app_store_inventory*s,bool enabled){
 if(!live(s))return APP_STORE_INVENTORY_RETAINED;
 if(s->operation||s->draining||s->feed_busy)return APP_STORE_INVENTORY_PENDING;
 if(enabled&&!s->available_feed)return APP_STORE_INVENTORY_UNAVAILABLE;
 s->feed=enabled?s->available_feed:NULL;return APP_STORE_INVENTORY_OK;
}
int app_store_inventory_launch_route(app_store_inventory*s,const app_store_inventory_selection*chosen,char*out,size_t capacity){
 if(!out||!capacity)return APP_STORE_INVENTORY_INVALID;
 out[0]=0;
 if(!live(s))return APP_STORE_INVENTORY_RETAINED;
 if(!chosen||chosen->kind!=APP_STORE_INSTALLED||!chosen->app.available||!valid_row(&chosen->app))return APP_STORE_INVENTORY_INVALID;
 app_store_inventory_selection fresh;int rc=app_store_inventory_choose(s,chosen->index,&fresh);if(rc)return rc;
 if(!same(chosen,&fresh)||fresh.kind!=APP_STORE_INSTALLED||!fresh.app.available)return APP_STORE_INVENTORY_INVALID;
 const char*p=fresh.app.launch_path;size_t n=strlen(p);
 if(n<5||n+1>capacity||p[0]=='/'||strcmp(p+n-4,".elf"))return APP_STORE_INVENTORY_INVALID;
 const char*part=p;
 for(size_t i=0;i<=n;i++){
  unsigned char ch=(unsigned char)p[i];if(ch=='\\'||(ch&&ch<32)||ch==127)return APP_STORE_INVENTORY_INVALID;
  if(ch=='/'||ch==0){size_t len=(size_t)(p+i-part);if(!len||(len==1&&part[0]=='.')||(len==2&&part[0]=='.'&&part[1]=='.'))return APP_STORE_INVENTORY_INVALID;part=p+i+1;}
 }
 memcpy(out,p,n+1);return APP_STORE_INVENTORY_OK;
}
