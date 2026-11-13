/* Shared Firmware Update intent controller. The scene provider owns every
 * pixel, input gesture and physical page. Product feeds and bank admission
 * remain in software.update.firmware, never in this application. */
#include "RiscRuntimeV1.h"
#include "RiscRuntimeClockV1.h"
#include "RiscRealtimeV1.h"
#include "RiscRtcUtcV1.h"
#include "RiscCivilClockV1.h"
#include "RiscAppCatalogV1.h"
#include "SoftwareUpdateV1.h"
#include "PortableWifiSavedNetwork.h"
#include "PortableRadioPolicy.h"
#include "files_scene_ui.h"
#include "files_scene_resident.h"
#include "files_scene_effects.h"
#include "ota_update_scene_background.h"
#include <stdio.h>
#include <string.h>

#define FW_WINDOW 16u
enum { FW_BACK=1, FW_CHECK, FW_CANCEL, FW_INSTALL, FW_RESTART,
       FW_PREVIOUS, FW_NEXT, FW_DISMISS, FW_RELEASE=1000 };
typedef struct {
 const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 service_grant,credentials_grant,wifi_grant;
 const software_update_v1 *service;
 const risc_key_value_v1 *credentials;
 const wifi_api_v1 *wifi;
 files_scene_ui ui;
 files_resident resident;
 files_effects effects;
 software_update_status_v1 status;
 software_update_row_v1 chosen,rows[FW_WINDOW],candidate_rows[FW_WINDOW];
 bool row_valid[FW_WINDOW],candidate_valid[FW_WINDOW];
 uint32_t row_first,row_count,candidate_first,candidate_count;
 uint32_t selected,first,revision,connect_started,handoff,cancel_revision_floor;
 bool retained,invoked,clean,dirty,connecting,radio_owned,confirming;
 bool cleanup_pending,install_after_connect,restart_pending,exit_requested;
 bool background_started,message_pending,launch_pending;
 uint32_t idle_at,idle_ms;
 char workflow_message[33],workflow_launch[RISC_APP_CATALOG_ROUTE_MAX];
 char message[160];
} firmware_scene;
static firmware_scene fw;
static risc_components_document_v1 fw_document;
static bool fw_events(void);

static bool fw_alive(void) {
 if(fw.retained)return false;
 if(!fw.runtime||risc_runtime_get_api(1)!=fw.runtime){fw.retained=true;return false;}
 return true;
}
static void fw_retain(void) {
 if(fw.retained)return;
 fw.retained=true;
 if(fw.runtime&&risc_runtime_get_api(1)==fw.runtime&&fw.runtime->retain_invocation)
  (void)fw.runtime->retain_invocation();
}
static bool fw_ui_alive(void *owner){(void)owner;return fw_alive();}
static void fw_ui_retain(void *owner){(void)owner;fw_retain();}
static void fw_note(const char *text){snprintf(fw.message,sizeof(fw.message),"%s",text);fw.dirty=true;}
static bool fw_string(const char *s,unsigned n){for(unsigned i=0;i<n;i++)if(!s[i])return true;return false;}
static bool fw_empty(const risc_runtime_capability_v1 *g){return !g->api&&!g->slot&&!g->generation;}
/* Clean optional absence is not retained custody. A partially returned grant,
 * lost invocation, failed release, or malformed grant is not clean absence. */
static int fw_acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!fw_alive())return -1;
 if(!fw_empty(out)){fw_retain();return -1;}
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 bool ok=fw.runtime->acquire(name,api,instance,&g);
 if(!fw_alive())return -1;
 if(g.struct_size!=sizeof(g)||(ok?(!g.api||!g.slot||!g.generation):!fw_empty(&g))){fw_retain();return -1;}
 if(!ok)return 0;
 *out=g;return 1;
}
static bool fw_release(risc_runtime_capability_v1 *grant){
 if(!fw_alive())return false;
 if(fw_empty(grant))return true;
 risc_runtime_capability_v1 copy=*grant;
 bool ok=fw.runtime->release(&copy);
 if(!fw_alive())return false;
 if(!ok||copy.struct_size!=sizeof(copy)||!fw_empty(&copy)){fw_retain();return false;}
 *grant=copy;return true;
}
static bool fw_now(uint32_t *out){
 if(!fw_alive())return false;
 bool ok=risc_runtime_monotonic_ms_v1(fw.runtime,out);
 if(!fw_alive())return false;
 if(!ok)fw_retain();return ok;
}
static bool fw_status(void){
 if(!fw_alive()||!fw.service)return false;
 software_update_status_v1 next={.struct_size=sizeof(next)};
 bool ok=fw.service->status(fw.service->context,&next);
 if(!fw_alive())return false;
 if(!ok){fw.cleanup_pending=true;fw_note("Update status unavailable; retry cleanup");return false;}
 if(next.struct_size!=sizeof(next)||next.state>SOFTWARE_UPDATE_ACTIVATION_UNKNOWN||next.count>SOFTWARE_UPDATE_ROWS_MAX){fw_retain();return false;}
 if(memcmp(&fw.status,&next,sizeof(next)))fw.dirty=true;
 fw.status=next;
 if(next.state==SOFTWARE_UPDATE_RETAINED){fw_retain();return false;}
 if(next.state==SOFTWARE_UPDATE_ACTIVATED||next.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN)fw.restart_pending=true;
 return true;
}
static bool fw_get(uint32_t index,software_update_row_v1 *out){
 if(!fw_alive()||!fw.service||index>=fw.status.count)return false;
 software_update_row_v1 row={.struct_size=sizeof(row)};
 bool ok=fw.service->get(fw.service->context,index,&row);
 if(!fw_alive())return false;
 if(!ok)return false;
 if(row.struct_size!=sizeof(row)||row.availability>SOFTWARE_UPDATE_USB_ONLY||
 !fw_string(row.id,sizeof(row.id))||!fw_string(row.version,sizeof(row.version))||
 !fw_string(row.installed,sizeof(row.installed))||!fw_string(row.reason,sizeof(row.reason))){fw_retain();return false;}
 *out=row;return true;
}
static bool fw_same(const software_update_row_v1 *a,const software_update_row_v1 *b){
 return a->availability==b->availability&&a->size==b->size&&!strcmp(a->id,b->id)&&
 !strcmp(a->version,b->version)&&!strcmp(a->installed,b->installed)&&!strcmp(a->reason,b->reason);
}
static bool fw_selected_current(void){
 software_update_row_v1 current;
 if(fw_status()&&fw_get(fw.selected,&current)&&current.availability==SOFTWARE_UPDATE_AVAILABLE&&fw_same(&current,&fw.chosen))return true;
 if(fw_alive()){fw.confirming=fw.install_after_connect=false;fw_note("Release changed; choose and confirm again");}
 return false;
}
static bool fw_radio_close(void){
 if(!fw_alive())return false;
 if(fw.wifi&&fw.radio_owned){
  bool ok=fw.wifi->disconnect_checked(fw.wifi->context);
  if(!fw_alive())return false;
  if(!ok){fw_retain();return false;}
 }
 if(!fw_release(&fw.wifi_grant))return false;
 fw.wifi=NULL;fw.radio_owned=fw.connecting=false;return true;
}
static bool fw_cancel(void){
 if(!fw_alive())return false;
 if(fw.restart_pending){fw_note(fw.status.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Update activated; restart required");return false;}
 bool cancelled=true;
 if(fw.service){cancelled=fw.service->cancel(fw.service->context);if(!fw_alive())return false;}
 /* A failed cancel may mean retained HTTP/bank custody. Never disconnect its
 * radio or run a storage-backed host checkpoint on that uncertain result. */
 if(!cancelled){fw_retain();return false;}
 if(!fw_radio_close())return false;
 if(fw.service&&!fw_status())return false;
 if(fw.service&&fw.status.resources_open){fw_retain();return false;}
 fw.cleanup_pending=fw.confirming=fw.install_after_connect=false;fw.cancel_revision_floor=0;
 fw_note("Cancelled; check again when ready");return true;
}
static bool fw_working(void){
 return fw.connecting||fw.radio_owned||fw.cleanup_pending||fw.confirming||fw.install_after_connect||fw.restart_pending||
 (fw.status.state>=SOFTWARE_UPDATE_CATALOG&&fw.status.state<=SOFTWARE_UPDATE_READY&&fw.status.state!=SOFTWARE_UPDATE_LIST);
}
bool ota_update_scene_background_safe(void){
 return fw_alive()&&!fw.connecting&&!fw.radio_owned&&!fw.cleanup_pending&&!fw.install_after_connect&&!fw.restart_pending&&
 !(fw.status.state>=SOFTWARE_UPDATE_CATALOG&&fw.status.state<=SOFTWARE_UPDATE_READY&&fw.status.state!=SOFTWARE_UPDATE_LIST);
}
bool ota_update_scene_background_workflow_ready(void){
 return ota_update_scene_background_safe()&&!fw.confirming&&!fw.message_pending&&!fw.launch_pending&&!fw.effects.phase&&!fw.resident.phase;
}
void ota_update_scene_background_idle_update(uint32_t milliseconds){fw.idle_ms=milliseconds;}
bool ota_update_scene_background_message(const char text[33]){
 if(!text||!fw_string(text,33)||!ota_update_scene_background_workflow_ready())return false;
 memcpy(fw.workflow_message,text,33);fw.message_pending=true;fw.dirty=true;return true;
}
bool ota_update_scene_background_launch(const char name[33]){
 if(!name||!fw_string(name,33)||!ota_update_scene_background_workflow_ready())return false;
 risc_runtime_capability_v1 grant={0};int available=fw_acquire(RISC_APP_CATALOG_CAPABILITY,1,0,&grant);
 if(available<=0)return false;
 const risc_app_catalog_v1 *api=grant.api;
 if(api->api_version!=1||api->struct_size<sizeof(*api)||!api->snapshot||!api->get){(void)fw_release(&grant);return false;}
 risc_app_catalog_snapshot_v1 before={.struct_size=sizeof(before)},after={.struct_size=sizeof(after)};
 int rc=api->snapshot(api->context,&before);if(!fw_alive())return false;
 bool found=false;
 if(rc==RISC_APP_CATALOG_OK){
  if(before.struct_size!=sizeof(before)||!before.generation||before.count>RISC_APP_CATALOG_MAX){fw_retain();return false;}
  for(unsigned i=0;i<before.count;i++){
   risc_app_catalog_entry_v1 entry={.struct_size=sizeof(entry)};rc=api->get(api->context,before.generation,i,&entry);
   if(!fw_alive())return false;if(rc)break;
   if(entry.struct_size!=sizeof(entry)||(entry.flags&~RISC_APP_CATALOG_AVAILABLE)||!fw_string(entry.file_name,sizeof(entry.file_name))){fw_retain();return false;}
   if((entry.flags&RISC_APP_CATALOG_AVAILABLE)&&!strcmp(name,entry.file_name)){memcpy(fw.workflow_launch,entry.file_name,sizeof(entry.file_name));found=true;break;}
  }
  if(!rc){rc=api->snapshot(api->context,&after);if(!fw_alive())return false;
   if(!rc&&(after.struct_size!=sizeof(after)||after.generation!=before.generation||after.count!=before.count))found=false;}
 }
 if(rc<RISC_APP_CATALOG_STALE||rc>RISC_APP_CATALOG_NOT_FOUND){fw_retain();return false;}
 if(!fw_release(&grant))return false;
 fw.launch_pending=!rc&&found;return fw.launch_pending;
}
/* Every helper read crosses a checked facade. A retention callback in the
 * middle of the existing credential codec cannot cause a later storage read. */
static int32_t fw_credentials_get(void *context,const char *key,void *buffer,uint32_t cap,uint32_t *size){
 (void)context;if(!fw_alive())return RISC_KEY_VALUE_CONTEXT;
 int32_t rc=fw.credentials->get(fw.credentials->context,key,buffer,cap,size);
 if(!fw_alive())return RISC_KEY_VALUE_CONTEXT;
 if(rc==RISC_KEY_VALUE_CONTEXT)fw_retain();return rc;
}
static int32_t fw_no_write(void *context,const char *key,const void *data,uint32_t size){
 (void)context;(void)key;(void)data;(void)size;return RISC_KEY_VALUE_INVALID;
}
static wifi_link_t fw_wifi_status(void *context){
 (void)context;if(!fw_alive()||!fw.wifi)return (wifi_link_t)3;
 wifi_link_t s=fw.wifi->status(fw.wifi->context);return fw_alive()?s:(wifi_link_t)3;
}
static bool fw_wifi_connect(void *context,const char *ssid,const char *password){
 (void)context;if(!fw_alive()||!fw.wifi)return false;
 bool ok=fw.wifi->connect(fw.wifi->context,ssid,password);return fw_alive()&&ok;
}
/* Native UTC, when granted, is authoritative even when unset. Only clean
 * absence permits the separately granted, explicitly configured RTC UTC
 * suffix. Never infer a timezone from a product or write/seed either clock. */
static bool fw_utc(uint64_t *seconds){
 risc_runtime_capability_v1 grant={0};int available=fw_acquire(RISC_REALTIME_CAPABILITY,1,0,&grant);
 if(available<0)return false;
 if(available){
  const risc_realtime_api_v1 *clock=grant.api;
  bool valid=clock->api_version==1&&clock->struct_size>=sizeof(*clock)&&clock->read;
  risc_realtime_snapshot_v1 sample={.struct_size=sizeof(sample)},before=sample;int rc=RISC_REALTIME_INVALID;
  if(valid){rc=clock->read(clock->context,&sample);if(!fw_alive())return false;}
  if(rc==RISC_REALTIME_CONTEXT){fw_retain();return false;}
  if(valid&&rc!=RISC_REALTIME_OK&&memcmp(&sample,&before,sizeof(sample))){fw_retain();return false;}
  valid=valid&&rc==RISC_REALTIME_OK&&sample.struct_size==sizeof(sample)&&!sample.reserved&&sample.validity==RISC_REALTIME_VALID&&
   sample.nanoseconds<1000000000u&&sample.nanoseconds%1000u==0&&sample.monotonic_before_us<=sample.monotonic_after_us&&
   sample.epoch_seconds>=1704067200LL&&sample.epoch_seconds<=2147483647LL;
  if(!fw_release(&grant))return false;
  if(valid)*seconds=(uint64_t)sample.epoch_seconds;return valid;
 }
 available=fw_acquire("rtc.clock",2,0,&grant);if(available<=0)return false;
 int utc_suffix=risc_rtc_utc_status_v1(grant.api);
 if(utc_suffix==0){
  /* Old RTC prefixes are usable only with an independently granted calendar
   * interpretation policy. Read/release the device before policy work. */
  risc_rtc_clock_prefix_v2 rtc;memcpy(&rtc,grant.api,sizeof(rtc));
  risc_rtc_calendar_v2 raw={0};bool read=rtc.read&&rtc.read(rtc.context,&raw);
  if(!fw_alive()||!fw_release(&grant))return false;
  if(!read)return false;
  available=fw_acquire(RISC_CIVIL_CLOCK_CAPABILITY,1,0,&grant);if(available<=0)return false;
  const risc_civil_clock_api_v1 *base=grant.api;
  const risc_civil_clock_policy_v1 *policy=risc_civil_clock_get_policy(base,base->struct_size);
  risc_civil_calendar_v1 civil={.struct_size=sizeof(civil),.year=raw.year,.month=raw.month,.day=raw.day,
   .hour=raw.hour,.minute=raw.minute,.second=raw.second,.weekday=raw.weekday};
  int64_t epoch=0;int rc=policy?policy->interpret_rtc(policy->context,&civil,&epoch):-1;
  if(!fw_alive())return false;if(rc==-9){fw_retain();return false;}
  if(!fw_release(&grant))return false;
  if(rc||epoch<1704067200LL||epoch>4102444799LL)return false;
  *seconds=(uint64_t)epoch;return true;
 }
 risc_rtc_utc_client_v1 clock={0};bool valid=utc_suffix==1&&risc_rtc_utc_bind_v1(grant.api,&clock);
 risc_rtc_utc_snapshot_v1 sample={.struct_size=sizeof(sample)},before=sample;
 if(valid){valid=clock.read(clock.context,&sample);if(!fw_alive())return false;
  if(!valid&&memcmp(&sample,&before,sizeof(sample))){fw_retain();return false;}}
 valid=valid&&sample.struct_size==sizeof(sample)&&!sample.reserved&&sample.epoch_seconds>=1704067200LL&&sample.epoch_seconds<=4102444799LL;
 if(!fw_release(&grant))return false;
 if(valid)*seconds=(uint64_t)sample.epoch_seconds;return valid;
}
static bool fw_radio_allowed(void){
 risc_runtime_capability_v1 grant={0};int found=fw_acquire(RISC_KEY_VALUE_CAPABILITY,1,PQA_STORE_INSTANCE,&grant);
 if(found<=0)return found==0;
 const risc_key_value_v1 *kv=grant.api;bool ok=kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get;
 uint8_t record[4]={0},flags=0;uint32_t size=0;int rc=RISC_KEY_VALUE_INVALID;
 if(ok){rc=kv->get(kv->context,PORTABLE_RADIO_KEY,record,sizeof(record),&size);if(!fw_alive())return false;}
 if(rc==RISC_KEY_VALUE_CONTEXT){fw_retain();return false;}
 ok=ok&&(rc==RISC_KEY_VALUE_NOT_FOUND||(rc==RISC_KEY_VALUE_OK&&portable_radio_decode(record,size,&flags)&&(flags&PORTABLE_RADIO_WIFI)));
 if(!fw_release(&grant))return false;
 if(!ok)fw_note("Wi-Fi is off or its setting is unavailable");return ok;
}
static bool fw_connect(bool install){
 if(!fw_alive()||!fw.service||fw.restart_pending)return false;
 if(!ota_update_scene_background_pause())return false;
 if(!fw_cancel())return false;
 const software_update_metadata_api_v1 *metadata=software_update_metadata_get_v1(fw.service);
 if(metadata){
  software_update_readiness_v1 ready={.struct_size=sizeof(ready)};
  bool ok=metadata->readiness(fw.service->context,&ready);if(!fw_alive())return false;
  if(!ok||ready.struct_size!=sizeof(ready)||!fw_string(ready.reason,sizeof(ready.reason))){fw_note("Update readiness unavailable; retry Check");return false;}
  if(!ready.configured){
   if(install){fw_note("Update feed unavailable; choose again");return false;}
   ok=fw.service->refresh(fw.service->context,0);if(!fw_alive())return false;
   if(ok)(void)fw_status();if(fw_alive())fw_note(ready.reason);return ok;
  }
 }
 if(!fw_radio_allowed())return false;
 if(!fw.credentials){
  int found=fw_acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE,&fw.credentials_grant);
  if(found<=0){if(found==0)fw_note("Saved network storage unavailable");return false;}
  const risc_key_value_v1 *kv=fw.credentials_grant.api;
  if(kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get){if(fw_release(&fw.credentials_grant))fw_note("Saved network storage unsupported");return false;}
  fw.credentials=kv;
 }
 int found=fw_acquire("net.wifi",1,0,&fw.wifi_grant);
 if(found<=0){if(found==0)fw_note("Wi-Fi unavailable; retry Check");return false;}
 const wifi_api_v1 *wifi=fw.wifi_grant.api;
 if(!portable_wifi_saved_network_api_valid(wifi)){if(fw_release(&fw.wifi_grant))fw_note("Wi-Fi service unsupported");return false;}
 fw.wifi=wifi;
 wifi_link_t state=fw_wifi_status(NULL);if(!fw_alive())return false;
 if(state!=WIFI_LINK_DOWN){(void)fw_radio_close();if(fw_alive())fw_note("Wi-Fi busy; try again later");return false;}
 const risc_key_value_v1 storage={1,sizeof(storage),NULL,fw_credentials_get,fw_no_write};
 wifi_api_v1 guarded=*wifi;guarded.context=NULL;guarded.status=fw_wifi_status;guarded.connect=fw_wifi_connect;
 fw.radio_owned=true;
 int result=portable_wifi_saved_network_connect(&storage,&guarded);if(!fw_alive())return false;
 if(result!=PORTABLE_WIFI_SAVED_NETWORK_STARTED){if(fw_radio_close())fw_note("Save a network in Wi-Fi Settings first");return false;}
 fw.install_after_connect=install;fw.connecting=true;
 /* Back remains valid throughout this operation. Confirmation Cancel is also
 * cancellation, never approval; later progress revisions keep that meaning. */
 fw.cancel_revision_floor=fw.ui.committed.revision;
 if(!fw_now(&fw.connect_started))return false;
 fw_note("Connecting with saved Wi-Fi");return true;
}
static void fw_tick(void){
 if(!fw_alive()||!fw.service||fw.cleanup_pending||fw.restart_pending)return;
 if(fw.wifi){
  wifi_link_t link=fw_wifi_status(NULL);if(!fw_alive())return;
  wifi_ipv4_v1 ip={0},ap={0};bool address=false;
  if(link==WIFI_LINK_UP){address=fw.wifi->addresses(fw.wifi->context,&ip,&ap);if(!fw_alive())return;address=address&&(ip.address[0]||ip.address[1]||ip.address[2]||ip.address[3]);}
  uint32_t now=0;if(!fw_now(&now))return;
  if(link>WIFI_LINK_UP||link==WIFI_LINK_DOWN||(!fw.connecting&&!address)||(fw.connecting&&(uint32_t)(now-fw.connect_started)>=30000u)){
   if(fw_cancel())fw_note("Connection lost; retry Check");return;
  }
  if(fw.connecting){
   if(!address)return;
   fw.connecting=false;bool install=fw.install_after_connect;
   if(install&&!fw_selected_current()){if(fw_alive()&&fw_cancel())fw_note("Release changed; choose and confirm again");return;}
   uint64_t seconds=0;if(!fw_utc(&seconds)){if(fw_alive()&&fw_cancel())fw_note("Set a valid time before HTTPS");return;}
   bool ok=install?fw.service->begin(fw.service->context,fw.selected,seconds):fw.service->refresh(fw.service->context,seconds);
   if(!fw_alive())return;
   fw.install_after_connect=false;
   if(!ok){if(fw_cancel())fw_note("Check/install unavailable; retry Check");return;}
   (void)fw_status();fw.dirty=true;return;
  }
 }
 uint32_t before=fw.status.state;
 bool stepped=fw.service->step(fw.service->context);if(!fw_alive())return;
 if(!fw_status())return;
 if(fw.status.state==SOFTWARE_UPDATE_ERROR){if(fw_radio_close()){fw.cancel_revision_floor=0;fw_note("Update failed; current install kept");}return;}
 if(!stepped){fw.cleanup_pending=true;fw_note("Update operation failed; retry cleanup");return;}
 if(fw.status.state==SOFTWARE_UPDATE_LIST&&fw.wifi){if(fw_radio_close()){fw.cancel_revision_floor=0;fw_note(fw.status.count?"Select a release; Check refreshes":"No releases in catalog");}return;}
 /* READY is a separate turn: drain fresh semantic Cancel/Back before any
 * activation, including an event arriving during the VERIFYING step. */
 if(fw.status.state==SOFTWARE_UPDATE_READY&&before==SOFTWARE_UPDATE_READY){
  if(!fw_radio_close())return;
  if(!fw_events()||!fw_alive()||fw.exit_requested||fw.handoff||fw.status.state!=SOFTWARE_UPDATE_READY)return;
  bool ok=fw.service->activate(fw.service->context);if(!fw_alive())return;
  if(!fw_status()){
   if(fw_alive()){fw.restart_pending=true;fw.status.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;fw_note("Activation uncertain; restart required");}
   return;
  }
  if(ok&&fw.status.state!=SOFTWARE_UPDATE_ACTIVATED){fw.restart_pending=true;fw.status.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;}
  if(fw.restart_pending)fw_note(fw.status.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Update activated; restart to use it");
  else {fw.cleanup_pending=true;fw_note("Activation failed; retry cleanup before checking again");}
 }
}

static risc_scene_node_v1 *fw_node(uint32_t id,uint32_t kind,uint32_t action,const char *label,const char *text){
 if(fw_document.node_count>=RISC_COMPONENTS_MAX_NODES){fw_retain();return NULL;}
 uint32_t index=fw_document.node_count++;
 risc_scene_node_v1 *node=&fw_document.nodes[index];
 node->id=id;node->route=1;node->kind=kind;node->action=action;
 snprintf(node->label,sizeof(node->label),"%s",label?label:"");snprintf(node->text,sizeof(node->text),"%s",text?text:"");return node;
}
/* The provider's bounded strings must not erase identity or reason text.
 * Full strings are emitted as multiple semantic text nodes; labels are hints. */
static void fw_text(uint32_t id,const char *label,const char *text){
 unsigned length=(unsigned)strlen(text),at=0;bool first=true;
 do {char part[RISC_SCENE_TEXT];unsigned n=length-at;if(n>=sizeof(part))n=sizeof(part)-1;
  memcpy(part,text+at,n);part[n]=0;fw_node(id++,RISC_SCENE_TEXT_NODE,0,first?label:"",part);at+=n;first=false;
 }while(at<length);
}
static bool fw_publish(void){
 if(!fw_alive()||!fw.dirty)return fw_alive();
 if(fw.revision==UINT32_MAX){fw_retain();return false;}
 memset(&fw_document,0,sizeof(fw_document));fw_document.api_version=1;fw_document.struct_size=sizeof(fw_document);
 fw_document.revision=fw.revision+1;fw_document.root=1;fw_document.route_count=1;
 fw_document.routes[0].id=1;fw_document.routes[0].back_action=FW_BACK;
 snprintf(fw_document.routes[0].title,sizeof(fw_document.routes[0].title),"Firmware Update");
 fw.candidate_count=0;memset(fw.candidate_valid,0,sizeof(fw.candidate_valid));
 if(fw.message_pending){
  fw_document.screen_key=6;fw_document.flags=RISC_COMPONENTS_ALERT;fw_document.cancel_action=FW_DISMISS;
  fw_text(100,"Context message",fw.workflow_message);fw_node(FW_DISMISS,RISC_SCENE_ACTION,FW_DISMISS,"Dismiss","");
 }else if(fw.restart_pending){
  fw_document.screen_key=5;fw_text(100,"Restart required",fw.status.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Activation confirmed; restart required");
  fw_node(FW_RESTART,RISC_SCENE_ACTION,FW_RESTART,"Restart device","");
 }else if(fw.cleanup_pending){
  fw_document.screen_key=4;fw_text(100,"Cleanup required","Resources remain owned until cleanup succeeds");
  fw_node(FW_CANCEL,RISC_SCENE_ACTION,FW_CANCEL,"Retry cleanup","");
 }else if(fw.connecting||(fw.status.state>=SOFTWARE_UPDATE_CATALOG&&fw.status.state<=SOFTWARE_UPDATE_READY&&fw.status.state!=SOFTWARE_UPDATE_LIST)){
  fw_document.screen_key=3;
  char bytes[64];snprintf(bytes,sizeof(bytes),"%lu / %lu bytes",(unsigned long)fw.status.done,(unsigned long)fw.status.total);
  risc_scene_node_v1 *progress=fw_node(100,RISC_COMPONENT_PROGRESS_CARD,0,fw.connecting?"Connecting...":fw.status.state==SOFTWARE_UPDATE_CATALOG?"Checking catalog":fw.status.state==SOFTWARE_UPDATE_PREPARING?"Preparing update":fw.status.state==SOFTWARE_UPDATE_DOWNLOAD?"Downloading update":"Verifying update",bytes);
  if(progress){progress->value=(int32_t)(fw.status.done>INT32_MAX?INT32_MAX:fw.status.done);progress->maximum=(int32_t)(fw.status.total>INT32_MAX?INT32_MAX:fw.status.total);}
  fw_node(FW_CANCEL,RISC_SCENE_ACTION,FW_CANCEL,"Cancel operation","Keep current installation");
 }else if(fw.confirming){
  fw_document.screen_key=2;fw_document.flags=RISC_COMPONENTS_CONFIRM;fw_document.cancel_action=FW_CANCEL;
  fw_text(100,"Release",fw.chosen.id);fw_text(110,"Version",fw.chosen.version);fw_text(120,"Installed",fw.chosen.installed);fw_text(130,"Details",fw.chosen.reason);
  fw_node(FW_CANCEL,RISC_SCENE_ACTION,FW_CANCEL,"Cancel","Keep current installation");
  risc_scene_node_v1 *install=fw_node(FW_INSTALL,RISC_SCENE_ACTION,FW_INSTALL,"Install and restart",fw.chosen.version);
  if(install)install->flags=RISC_SCENE_DESTRUCTIVE;
 }else{
  fw_document.screen_key=1+fw.first*8;
  risc_scene_node_v1 *check=fw_node(FW_CHECK,RISC_SCENE_ACTION,FW_CHECK,"Check / retry","Repository release catalog");
  if(check&&!fw.service)check->flags=RISC_SCENE_DISABLED;
  if(fw.first>=fw.status.count)fw.first=0;
  fw.candidate_first=fw.first;fw.candidate_count=fw.status.count-fw.first;if(fw.candidate_count>FW_WINDOW)fw.candidate_count=FW_WINDOW;
  for(unsigned i=0;i<fw.candidate_count;i++){
   software_update_row_v1 *row=&fw.candidate_rows[i];fw.candidate_valid[i]=fw_get(fw.first+i,row);if(!fw_alive())return false;
   if(!fw.candidate_valid[i]){fw_node(FW_RELEASE+fw.first+i,RISC_COMPONENT_ROW,0,"Unavailable","Retry Check");continue;}
   char text[RISC_SCENE_TEXT];snprintf(text,sizeof(text),"%s  %s",row->version,row->availability==SOFTWARE_UPDATE_AVAILABLE?"Update":row->availability==SOFTWARE_UPDATE_CURRENT?"Current":row->availability==SOFTWARE_UPDATE_USB_ONLY?"USB only":"Unsupported");
   fw_node(FW_RELEASE+fw.first+i,RISC_COMPONENT_ROW,FW_RELEASE+fw.first+i,row->id,text);
  }
  if(fw.first)fw_node(FW_PREVIOUS,RISC_SCENE_ACTION,FW_PREVIOUS,"Previous releases","");
  if(fw.first+fw.candidate_count<fw.status.count)fw_node(FW_NEXT,RISC_SCENE_ACTION,FW_NEXT,"More releases","");
  if(!fw.status.count)fw_text(100,"",fw.service?"Check for available releases":"Update service unavailable");
 }
 if(fw.message[0])fw_text(200,"Status",fw.message);
 if(!fw_alive())return false;
 int rc=files_ui_publish(&fw.ui,&fw_document,fw.resident.enabled);
 if(rc!=1)return false;
 if(!fw.cancel_revision_floor&&(fw.connecting||(fw.status.state>=SOFTWARE_UPDATE_CATALOG&&fw.status.state<=SOFTWARE_UPDATE_READY&&fw.status.state!=SOFTWARE_UPDATE_LIST)))
  fw.cancel_revision_floor=fw_document.revision;
 fw.revision=fw_document.revision;fw.row_first=fw.candidate_first;fw.row_count=fw.candidate_count;
 memcpy(fw.rows,fw.candidate_rows,sizeof(fw.rows));memcpy(fw.row_valid,fw.candidate_valid,sizeof(fw.row_valid));fw.dirty=false;return true;
}
static bool fw_action_present(const risc_scene_event_v1 *event){
 if(event->kind!=RISC_SCENE_ACTION_EVENT||event->value)return false;
 if(!event->node)return event->action==FW_BACK;
 for(unsigned i=0;i<fw.ui.committed.node_count;i++){
  const risc_scene_node_v1 *n=&fw.ui.committed.nodes[i];
  if(n->id==event->node)return n->action&&n->action==event->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN));
 }
 return false;
}
static void fw_action(uint32_t action){
 if(!fw_alive())return;
 if(fw.message_pending){if(action==FW_DISMISS||action==FW_BACK){fw.message_pending=false;fw.dirty=true;}return;}
 if(action==FW_BACK){if(fw.confirming){fw.confirming=false;fw.dirty=true;}else fw.exit_requested=true;return;}
 if(action==FW_CANCEL){if(fw.confirming){fw.confirming=false;fw.dirty=true;}else (void)fw_cancel();return;}
 if(action==FW_RESTART&&fw.restart_pending&&fw.service){
  bool ok=fw.service->restart(fw.service->context);if(!fw_alive())return;
  if(!ok)fw_note("Restart not accepted; retry Restart");return;
 }
 if(fw.restart_pending||fw.cleanup_pending||fw.connecting||fw.install_after_connect||fw.radio_owned||
 (fw.status.state>=SOFTWARE_UPDATE_CATALOG&&fw.status.state<=SOFTWARE_UPDATE_READY&&fw.status.state!=SOFTWARE_UPDATE_LIST))return;
 if(action==FW_INSTALL&&fw.confirming){if(fw_selected_current())(void)fw_connect(true);return;}
 if(fw.confirming)return;
 if(action==FW_CHECK){(void)fw_connect(false);return;}
 if(action==FW_PREVIOUS){fw.first=fw.first>=FW_WINDOW?fw.first-FW_WINDOW:0;fw.dirty=true;return;}
 if(action==FW_NEXT){if(fw.first+FW_WINDOW<fw.status.count)fw.first+=FW_WINDOW;fw.dirty=true;return;}
 if(action>=FW_RELEASE&&action<FW_RELEASE+SOFTWARE_UPDATE_ROWS_MAX){
  uint32_t index=action-FW_RELEASE;
  if(index<fw.row_first||index-fw.row_first>=fw.row_count||!fw.row_valid[index-fw.row_first])return;
  software_update_row_v1 current;
  if(!fw_status()||!fw_get(index,&current))return;
  if(!fw_same(&current,&fw.rows[index-fw.row_first])){fw_note("Catalog changed; choose the release again");return;}
  fw.selected=index;fw.chosen=current;fw.cancel_revision_floor=0;fw_note(current.reason);
  /* Unsupported/current/USB rows remain inspectable with their full identity. */
  fw.confirming=current.availability==SOFTWARE_UPDATE_AVAILABLE;
  if(!fw.confirming){snprintf(fw.message,sizeof(fw.message),"%s: %s",current.id,current.reason);fw.dirty=true;}
 }
}
static bool fw_events(void){
 for(unsigned i=0;i<32&&fw_alive();i++){
  risc_scene_event_v1 event={0};int rc=files_ui_next(&fw.ui,&event);
  if(rc<0)return false;if(!rc)return true;
  if(event.document_revision!=fw.ui.committed.revision){
   /* Cosmetic progress replacement must not erase a fresh queued cancellation.
    * A provider can enqueue it in snapshot/update itself. Only stop/handoff
    * intents from this operation's revision interval survive; selection and
    * install always require the exact current committed document. */
   bool same_operation=fw.cancel_revision_floor&&event.document_revision>=fw.cancel_revision_floor&&
    event.document_revision<fw.ui.committed.revision&&fw_working();
   bool stop=(event.kind==RISC_SCENE_ACTION_EVENT&&!event.value&&
      ((event.action==FW_CANCEL&&event.node==FW_CANCEL)||(event.action==FW_BACK&&!event.node)))||
    ((event.kind==RISC_SCENE_SUSPEND_EVENT||event.kind==RISC_SCENE_CONTROLS_EVENT)&&!event.node&&!event.action&&!event.value);
   if(!same_operation||!stop)continue;
  }
  if(!fw_now(&fw.idle_at))return false;
  if(event.kind==RISC_SCENE_SUSPEND_EVENT&&!event.node&&!event.action&&!event.value){fw.exit_requested=true;return true;}
  if(event.kind==RISC_SCENE_CONTROLS_EVENT&&!event.node&&!event.action&&!event.value){if(fw.resident.enabled)fw.handoff=RISC_RESIDENT_CHECKPOINT_CONTROLS;return true;}
  if(fw_action_present(&event))fw_action(event.action);
  if(!fw_alive()||fw.exit_requested||fw.handoff)return fw_alive();
 }
 return false; /* More input may be queued: do not advance the bank. */
}
static bool fw_close(void){
 if(!fw_cancel()||!ota_update_scene_background_pause()||!files_ui_close(&fw.ui))return false;
 if(fw.background_started&&!ota_update_scene_background_close())return false;
 fw.background_started=false;
 if(!fw_release(&fw.credentials_grant)||!fw_release(&fw.service_grant))return false;
 fw.credentials=NULL;fw.service=NULL;
 if(!files_ui_release(&fw.ui))return false;fw.clean=true;return true;
}
static int fw_settle(void *owner){
 (void)owner;if(!fw_alive())return -1;
 if(fw.restart_pending||fw.cleanup_pending)return 0;
 if(!fw_cancel())return fw_alive()?0:-1;
 if(!ota_update_scene_background_pause())return fw_alive()?0:-1;
 return files_ui_close(&fw.ui)?1:fw_alive()?0:-1;
}
static int fw_effect_call(void *owner,uint32_t reason){
 (void)owner;if(!fw.resident.enabled)return RISC_RESIDENT_BUSY;
 uint32_t flags=0;int rc=files_resident_call(&fw.resident,reason,&flags);
 if(flags){fw_retain();return RISC_RESIDENT_RETAINED;}return rc;
}
static bool fw_effect_done(void *owner,unsigned effect,unsigned result){
 (void)owner;return ota_update_scene_background_effect_done(effect,result);
}
static bool fw_initialize(const risc_runtime_api_v1 *runtime){
 if(fw.retained||(fw.invoked&&!fw.clean)){fw_retain();return false;}
 memset(&fw,0,sizeof(fw));fw.runtime=runtime;fw.invoked=true;fw.dirty=true;fw.status.struct_size=sizeof(fw.status);
 files_ui_init(&fw.ui,runtime,fw_ui_alive,fw_ui_retain,NULL);
 int shown=files_ui_bind(&fw.ui);
 if(shown!=1){if(shown==0&&files_ui_release(&fw.ui))fw.clean=true;return false;}
 if(!ota_update_scene_background_init(runtime,fw_ui_alive,fw_ui_retain,NULL)){if(fw_alive())fw_retain();return false;}
 fw.background_started=true;
 if(!files_resident_init(&fw.resident,runtime,fw_ui_alive,fw_ui_retain,fw_settle,NULL))return false;
 files_effects_init(&fw.effects,fw_ui_alive,fw_ui_retain,fw_settle,fw_effect_call,fw_effect_done,NULL);
 if(!fw_now(&fw.idle_at))return false;
 (void)ota_update_scene_background_idle_timeout(&fw.idle_ms);if(!fw_alive())return false;
 int available=fw_acquire(SOFTWARE_UPDATE_FIRMWARE_CAPABILITY,1,0,&fw.service_grant);
 if(available<0)return false;
 if(available){
  const software_update_v1 *s=fw.service_grant.api;
  if(s->api_version==1&&s->struct_size>=sizeof(*s)&&s->refresh&&s->step&&s->status&&s->get&&s->begin&&s->cancel&&s->activate&&s->restart)fw.service=s;
  else if(!fw_release(&fw.service_grant))return false;
 }
 if(fw.service){if(!fw_status())return fw_alive();fw_note("Saved Wi-Fi; Check to connect");}
 else fw_note("Update service unavailable");
 return fw_alive();
}
/* One bounded owner-task turn, also used by the deterministic host fixtures. */
static bool fw_step(void){
 if(!fw_alive())return false;
 uint32_t now=0;if(!fw_now(&now))return false;
 if(!ota_update_scene_background_capture()||!files_resident_capture(&fw.resident,now))return false;
 if(fw.runtime->struct_size>=RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE&&fw.runtime->exit_requested){
  bool exit=fw.runtime->exit_requested();if(!fw_alive())return false;if(exit)fw.exit_requested=true;
 }
 if(!fw_events())return fw_alive();
 if(fw.launch_pending)fw.exit_requested=true;
 if(fw.exit_requested){
  if(fw_close()){
   if(fw.launch_pending&&fw.runtime->request_launch){bool launched=fw.runtime->request_launch(fw.workflow_launch);if(!fw_alive())return false;(void)launched;}
   return false;
  }
  if(!fw_alive())return false;
  if(fw.restart_pending)fw.exit_requested=false;
  else return true; /* Retain exit intent while the display drains. */
 }
 if(fw.handoff){
  (void)files_resident_manual(&fw.resident,fw.handoff);fw.handoff=0;
 }
 if(!files_ui_snapshot(&fw.ui))return false;
 bool settled=!(fw.ui.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY));
 if(fw.ui.flags&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))fw.idle_at=now;
 bool due=fw.idle_ms&&!fw_working()&&!ota_update_scene_background_capture_active()&&(uint32_t)(now-fw.idle_at)>=fw.idle_ms;
 int rc=files_resident_step(&fw.resident,settled,fw.restart_pending||fw.cleanup_pending,due);
 if(rc<0||!fw_alive())return false;
 if(rc==1){fw.dirty=true;fw.idle_at=now;if(fw.resident.exit_requested)fw.exit_requested=true;
  if(fw.resident.reply_flags&RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED)(void)ota_update_scene_background_idle_timeout(&fw.idle_ms);
  if(fw.resident.unsupported)fw_note("Host lifecycle service unavailable");}
 if(!fw_alive())return false;
 if(fw.resident.phase||fw.exit_requested)return true;
 if(ota_update_scene_background_safe()&&!fw.message_pending){
  if(!ota_update_scene_background_step(settled))return false;
  rc=files_effects_step(&fw.effects,ota_update_scene_background_effects(),settled);
  if(rc<0||!fw_alive())return false;
  if(rc==2)fw_note("Host settings effects unavailable");if(rc==1)fw.dirty=true;
 }
 if(fw.effects.phase||fw.launch_pending)return true;
 if(!fw.ui.closing&&!fw.message_pending)fw_tick();
 if(!fw_alive())return false;
 /* A provider callback can queue input. Consume the old committed revision
  * before publishing a progress revision, or its fresh Cancel would be lost. */
 if(!fw_events())return fw_alive();
 (void)fw_publish();return fw_alive();
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1 *runtime=risc_runtime_get_api(1);
 if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->retain_invocation||!runtime->yield_ms)return;
 if(!fw_initialize(runtime))return;
 while(fw_step())runtime->yield_ms(1);
 /* Clean transient background refusal is still an owned invocation. Drain
  * all resources rather than relying on implicit grant revocation at return. */
 while(fw_alive()&&!fw.clean){
  if(fw.restart_pending){fw_retain();break;}
  if(fw_close())break;
  if(fw_alive())runtime->yield_ms(1);
 }
}
