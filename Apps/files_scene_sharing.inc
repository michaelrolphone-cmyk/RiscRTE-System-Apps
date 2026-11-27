/* Extracted domain from file_browser_sharing.inc; semantic UI/lifecycle supplied by files_scene.c. */
/* Explicit foreground sharing UI. Native storage remains behind the admitted
 * export map. Network setup reuses the shared saved-profile session helper. */
#include "PortableFileSharing.h"
#include "PortableNativeCustody.h"
#ifndef PORTABLE_ALARM_TERMINAL_RETENTION
#error Sharing requires the silent terminal custody fence
#endif
#include "PortableNetworkSession.h"
#include "PortableRadioPolicy.h"
#include "PortableBackgroundServices.h"
#ifdef PORTABLE_FILE_SETUP
#include "PortableFileSetup.h"
#include "RiscEntropySourceV1.h"
#include "PortableBluetoothControl.h"
static bool fsh_direct=true,fsh_private_started;
static unsigned fsh_direct_stage;
static risc_bluetooth_network_offer_v1 fsh_offer;
static risc_runtime_capability_v1 fsh_entropy_grant,fsh_ble_power_grant;
static bool fsh_ble_restore_off,fsh_ble_power_ready;
static bool fsh_setup_close(void);
static void fsh_setup_fence(void);
static void fsh_setup_wipe(void*,size_t);
#endif
#ifndef PORTABLE_FILE_SHARING_EXPORT_INSTANCE
#error Sharing requires an explicit app-data export grant selection
#endif
#ifndef PORTABLE_FILE_SHARING_TCP_INSTANCE
#error Sharing requires an explicit TCP provider selection
#endif
#ifndef PORTABLE_FILE_SHARING_ENTROPY_INSTANCE
#error Sharing requires an explicit entropy provider selection
#endif
#ifdef PORTABLE_FILE_PEER
#include "PortablePeerFiles.h"
static portable_peer_files *fpeer;
static bool fpeer_close(void);
static void fpeer_tick(uint64_t),fsh_start(void);
#endif
#define FB_SHARE_LIFETIME_MS 300000u
enum { FSH_OFF,FSH_POLICY,FSH_STORE,FSH_WIFI,FSH_CONNECT,FSH_ACTIVE,FSH_STOPPING };
static unsigned fsh_phase;
static uint8_t fsh_radio_flags;
static bool fsh_network_initialized,fsh_back_pending,fsh_service_ending;
static risc_runtime_capability_v1 fsh_store_grant,fsh_wifi_grant;
static portable_network_session fsh_network;
static portable_file_sharing *fsh_client;
static portable_file_sharing_status fsh_status;
static portable_network_snapshot fsh_link;
static uint64_t fsh_now;
static uint32_t fsh_last_clock,fsh_last_second;
static char fsh_url[64],fsh_message[96];
static bool fsh_owner(void *unused){(void)unused;return !fb_terminal&&!portable_adapter_retained();}
static void fsh_trace(const char *event,int result){
 if(!fb_runtime||!fb_runtime->diagnostic||!fsh_owner(NULL))return;
 unsigned direct=0;
#ifdef PORTABLE_FILE_SETUP
 direct=fsh_direct_stage;
#endif
 const char *role="share";
#ifdef PORTABLE_FILE_PEER
 if(fpeer_mode)role="nearby";
#endif
 char line[160];
 snprintf(line,sizeof(line),"FILES t_ms=%llu role=%s event=%s phase=%u direct=%u result=%d",(unsigned long long)fsh_now,role,event,fsh_phase,direct,result);
 (void)fb_runtime->diagnostic(line);
}
static void fsh_error(const char *message){fsh_trace("error",0);snprintf(fsh_message,sizeof(fsh_message),"%s",message);fb_dirty=true;}
static void fsh_retain(void){
 fb_terminal=true;
#ifdef PORTABLE_FILE_PEER
 portable_peer_tick(fpeer,fsh_now); /* Owner is false: pure credential wipe/fence. */
#endif
#ifdef PORTABLE_FILE_SETUP
 fsh_setup_fence();
 fsh_setup_wipe(&fsh_offer,sizeof(fsh_offer));
#endif
 portable_file_sharing_stop(fsh_client); /* Pure owner failure wipes local secrets. */
 memset(&fsh_status,0,sizeof(fsh_status));
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
 portable_adapter_retain_silent();
#else
 PORTABLE_APP_RETAIN();
#endif
}
static uint64_t fsh_clock(void){uint32_t now=files_scene_millis();fsh_now+=(uint32_t)(now-fsh_last_clock);fsh_last_clock=now;return fsh_now;}
bool portable_file_browser_sharing_active(void){return fsh_phase!=FSH_OFF;}
bool portable_file_browser_stop_pending(void){return fsh_phase==FSH_STOPPING&&!fb_terminal&&!PORTABLE_APP_RETAINED();}
bool portable_file_browser_network_owned(void){
#ifdef PORTABLE_FILE_PEER
 if(portable_peer_network_owned(fpeer))return true;
#endif
 return fsh_network_initialized&&!fsh_link.borrowed&&fsh_phase!=FSH_OFF;
}
bool portable_file_browser_idle_ready(void){return fsh_phase==FSH_OFF;}
void portable_file_browser_resume(void){fb_dirty=true;}
bool portable_file_browser_services_safe(void){
 if(!fsh_owner(NULL))return false;
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return false;
#endif
 if(fsh_network_initialized){
  int rc=portable_network_session_step(&fsh_network,(uint32_t)fsh_clock());
  portable_network_session_snapshot(&fsh_network,&fsh_link);
  if(rc==PORTABLE_NETWORK_RETAINED){fsh_retain();return false;}
  if(rc<0&&fsh_phase!=FSH_STOPPING){fsh_error("Wi-Fi connection ended");fsh_phase=FSH_STOPPING;}
 }
 return fsh_owner(NULL);
}
static bool fsh_release(risc_runtime_capability_v1 *grant){
 if(!grant->api)return true;
 if(!fb_runtime->release(grant)||!fsh_owner(NULL)){fsh_retain();return false;}
 memset(grant,0,sizeof(*grant));return true;
}
#ifdef PORTABLE_FILE_PEER
static bool fsh_peer_service_result(bool ok){
 portable_peer_status status;portable_peer_status_get(fpeer,&status);
 if(status.state==PEER_RETAINED){fsh_retain();return false;}
 return ok;
}
#endif
bool portable_file_browser_services_begin(void){
 if(!fsh_owner(NULL))return false;
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return false;
#endif
#ifdef PORTABLE_FILE_PEER
 if(fpeer)return fsh_peer_service_result(portable_peer_services_begin(fpeer));
#endif
 if(!fsh_network_initialized)return true;
 if(fsh_service_ending){
  int rc=portable_network_session_service_end(&fsh_network);
  if(rc==PORTABLE_NETWORK_SERVICE_RETAINED){fsh_retain();return false;}
  if(rc==PORTABLE_NETWORK_SERVICE_OK)fsh_service_ending=false;
  return false; /* The prior work completed; do not replay it in this tick. */
 }
 int rc=portable_network_session_service_begin(&fsh_network);
 if(rc==PORTABLE_NETWORK_SERVICE_RETAINED)fsh_retain();
 return rc==PORTABLE_NETWORK_SERVICE_OK;
}
bool portable_file_browser_services_end(void){
 if(!fsh_owner(NULL))return false;
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return false;
#endif
#ifdef PORTABLE_FILE_PEER
 if(fpeer)return fsh_peer_service_result(portable_peer_services_end(fpeer));
#endif
 if(!fsh_network_initialized)return true;
 int rc=portable_network_session_service_end(&fsh_network);
 if(rc==PORTABLE_NETWORK_SERVICE_RETAINED){fsh_retain();return false;}
 fsh_service_ending=rc==PORTABLE_NETWORK_SERVICE_PENDING;
 return true; /* A pending lease end is drained before any later service work. */
}
#ifdef PORTABLE_FILE_SETUP
static bool fsh_ble_power_close(void){
 if(!fsh_ble_power_grant.api)return true;
 const portable_bluetooth_control_v1*b=fsh_ble_power_grant.api;
 if(fsh_ble_restore_off){
  bool off=b->set_enabled(b->context,false);
  if(!fsh_owner(NULL)){fsh_retain();return false;}
  uint8_t state=PORTABLE_BLUETOOTH_RETAINED;
  if(!off||!b->status(b->context,&state)||!fsh_owner(NULL)||state!=PORTABLE_BLUETOOTH_OFF){fsh_retain();return false;}
 }
 if(!fsh_release(&fsh_ble_power_grant))return false;
 fsh_ble_restore_off=fsh_ble_power_ready=false;
 return true;
}
#endif
static bool fsh_close(void){
 /* Successful handoff marks the browser terminal after all sharing resources
  * closed. Fini may recheck this pure completed state without reacquiring. */
 if(fsh_phase==FSH_OFF)return true;
 if(!fsh_owner(NULL))return false;
 fsh_phase=FSH_STOPPING;
#ifdef PORTABLE_FILE_PEER
 if(!fpeer_close())return false;
#endif
 portable_file_sharing_stop(fsh_client);
 memset(&fsh_status,0,sizeof(fsh_status));
#ifdef PORTABLE_FILE_SETUP
 if(!fsh_setup_close())return false;
#endif
 if(fsh_service_ending){(void)portable_file_browser_services_begin();return false;}
 if(fsh_client){
  if(!portable_file_browser_services_begin())return false;
  bool closed=portable_file_sharing_close(fsh_client,fsh_clock());
  if(!closed){fsh_retain();return false;}
  if(!portable_file_browser_services_end())return false;
  if(fsh_service_ending)return false;
  if(!portable_file_sharing_destroy(&fsh_client)){fsh_retain();return false;}
  memset(&fsh_status,0,sizeof(fsh_status));
 }
 if(fsh_network_initialized){
  int rc=portable_network_session_close(&fsh_network);
  if(rc==PORTABLE_NETWORK_CLOSE_RETAINED){fsh_retain();return false;}
  if(rc==PORTABLE_NETWORK_CLOSE_PENDING)return false;
  fsh_network_initialized=false;memset(&fsh_link,0,sizeof(fsh_link));
 }
 #ifdef PORTABLE_FILE_SETUP
 if(!fsh_release(&fsh_entropy_grant)||!fsh_ble_power_close())return false;
 fsh_setup_wipe(&fsh_offer,sizeof(fsh_offer));fsh_private_started=false;fsh_direct_stage=0;
#endif
 if(!fsh_release(&fsh_wifi_grant)||!fsh_release(&fsh_store_grant))return false;
 fsh_phase=FSH_OFF;fsh_url[0]=0;fb_dirty=true;return true;
}
static bool fsh_volume_share(void){
#ifdef PORTABLE_FILE_SHARING_SELECTED_VOLUME
 return fb_storage_index<FB_LOCAL_ROOT_COUNT&&!strcmp(fb_root_capability(fb_storage_index),"storage.volume");
#else
 return false;
#endif
}
static void fsh_start(void){
 if(fsh_phase!=FSH_OFF)return;
 if(!portable_background_stop()){fsh_retain();return;}
 fsh_now=0;fsh_last_clock=files_scene_millis();fsh_last_second=UINT32_MAX;
 fsh_message[0]=0;fsh_url[0]=0;memset(&fsh_status,0,sizeof(fsh_status));
 #ifdef PORTABLE_FILE_SETUP
 fsh_direct_stage=0;fsh_private_started=false;fsh_setup_wipe(&fsh_offer,sizeof(fsh_offer));
#endif
 fsh_phase=FSH_POLICY;fb_dirty=true;
 fsh_trace("start",0);
}
static unsigned fsh_policy(void){
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!fb_runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&grant)){
  if(grant.api||!fsh_owner(NULL))fsh_retain();
  return PORTABLE_WIFI_POLICY_UNAVAILABLE;
 }
 if(!fsh_owner(NULL)){fsh_retain();return PORTABLE_WIFI_POLICY_UNAVAILABLE;}
 const risc_key_value_v1 *kv=grant.api;uint8_t flags=0,bytes[4];uint32_t size=0;
 if(!pqa_preferences_valid(kv)){(void)fsh_release(&grant);return PORTABLE_WIFI_POLICY_UNAVAILABLE;}
 int32_t rc=kv->get(kv->context,PORTABLE_RADIO_KEY,bytes,sizeof(bytes),&size);
 if(!fsh_owner(NULL)||(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&rc!=RISC_KEY_VALUE_INVALID&&rc!=RISC_KEY_VALUE_IO&&rc!=RISC_KEY_VALUE_BUSY)){fsh_retain();return PORTABLE_WIFI_POLICY_UNAVAILABLE;}
 if(!fsh_release(&grant))return PORTABLE_WIFI_POLICY_UNAVAILABLE;
 if(rc==RISC_KEY_VALUE_BUSY)return PORTABLE_WIFI_POLICY_BUSY;
 if(rc==RISC_KEY_VALUE_NOT_FOUND)flags=PORTABLE_RADIO_WIFI;
 else if(rc!=RISC_KEY_VALUE_OK||!portable_radio_decode(bytes,size,&flags))return PORTABLE_WIFI_POLICY_UNAVAILABLE;
 fsh_radio_flags=flags;
 return flags&PORTABLE_RADIO_WIFI?PORTABLE_WIFI_POLICY_ALLOWED:PORTABLE_WIFI_POLICY_OFF;
}
static bool fsh_acquire(const char *name,uint64_t instance,risc_runtime_capability_v1 *grant){
 *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
 bool ok=fb_runtime->acquire(name,1,instance,grant);
 if(!fsh_owner(NULL)){fsh_retain();return false;}
 if(!ok){if(grant->api)fsh_retain();else {fsh_error("Sharing service unavailable");fsh_phase=FSH_STOPPING;}return false;}
 if(!grant->api){fsh_retain();return false;}return true;
}
#ifdef PORTABLE_FILE_SETUP
#include "files_scene_setup.inc"
#endif
#ifdef PORTABLE_FILE_PEER
#include "files_scene_peer.inc"
#endif
#ifdef PORTABLE_FILE_SETUP
#include "files_scene_direct.inc"
#endif
static void fsh_tick(void){
#ifdef PORTABLE_FILE_PEER
 fpeer_after_close();
#endif
 if(fsh_phase==FSH_OFF||!fsh_owner(NULL))return;
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only()){(void)fsh_setup_close();return;}
 fsh_setup_tick();if(!fsh_owner(NULL)||portable_file_browser_cleanup_only())return;
#endif
 uint64_t now=fsh_clock();
 { /* State transitions only; stable discovery/connect polls stay silent. */
  static unsigned phase=~0u,direct=~0u;
  unsigned next_direct=0;
#ifdef PORTABLE_FILE_SETUP
  next_direct=fsh_direct_stage;
#endif
  if(phase!=fsh_phase||direct!=next_direct){phase=fsh_phase;direct=next_direct;fsh_trace("state",0);}
 }
 if(fsh_phase==FSH_ACTIVE&&fsh_status.expires_ms&&now>=fsh_status.expires_ms){fsh_error("Sharing session expired");fsh_phase=FSH_STOPPING;}
 if(fsh_phase==FSH_STOPPING){
#ifdef PORTABLE_FILE_SETUP
  if(!fsh_setup_close())return;
#endif
  if(fsh_network_initialized)(void)portable_network_session_step(&fsh_network,(uint32_t)now);
  portable_network_session_snapshot(&fsh_network,&fsh_link);
  if(fsh_link.retained){fsh_retain();return;}
  if(fsh_close()&&fsh_back_pending){fsh_back_pending=false;fb_mode=FB_LIST;fb_loaded=false;(void)fb_load(0);}
  return;
 }
 if(fsh_phase==FSH_POLICY){
  unsigned allowed=fsh_policy();
  if(!fsh_owner(NULL)){fsh_retain();return;}
  if(allowed==PORTABLE_WIFI_POLICY_BUSY)return;
  if(allowed!=PORTABLE_WIFI_POLICY_ALLOWED){fsh_error(allowed==PORTABLE_WIFI_POLICY_OFF?"Enable Wi-Fi in Quick Actions first":"Wi-Fi settings unavailable");fsh_phase=FSH_STOPPING;return;}
  fsh_phase=FSH_STORE;return;
 }
 #ifdef PORTABLE_FILE_PEER
 if(fpeer_mode){if(fsh_phase==FSH_STORE)fsh_phase=FSH_CONNECT;fpeer_tick(now);return;}
#endif
#ifdef PORTABLE_FILE_SETUP
 if(fsh_direct){fsh_direct_tick(now);return;}
#endif
 if(fsh_phase==FSH_STORE){if(fsh_acquire(RISC_KEY_VALUE_CAPABILITY,PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE,&fsh_store_grant))fsh_phase=FSH_WIFI;return;}
 if(fsh_phase==FSH_WIFI){
  if(!fsh_acquire("net.wifi",15,&fsh_wifi_grant))return;
  portable_network_session_init(&fsh_network,fsh_wifi_grant.api,fsh_store_grant.api,fsh_owner,NULL);fsh_network_initialized=true;
  int rc=portable_network_session_start(&fsh_network,(uint32_t)now);
  if(rc==PORTABLE_NETWORK_RETAINED){fsh_retain();return;}
  if(rc<0){fsh_error("Asynchronous Wi-Fi unavailable");fsh_phase=FSH_STOPPING;return;}
  fsh_phase=FSH_CONNECT;return;
 }
 int connected=portable_network_session_step(&fsh_network,(uint32_t)now);
 portable_network_session_snapshot(&fsh_network,&fsh_link);
 if(connected==PORTABLE_NETWORK_RETAINED){fsh_retain();return;}
 if(connected<0){fsh_error(connected==PORTABLE_NETWORK_EMPTY?"Save a default network in Wi-Fi Settings":"Wi-Fi connection ended");fsh_phase=FSH_STOPPING;return;}
 if(connected!=PORTABLE_NETWORK_READY)return;
 #ifdef PORTABLE_FILE_PEER
 if(fpeer_mode){fpeer_tick(now);return;}
#endif
 if(fsh_phase==FSH_CONNECT){
  const portable_file_sharing_hooks hooks={NULL,fsh_owner,malloc,free};
  fsh_client=portable_file_sharing_create(fb_runtime,&hooks);
  if(!fsh_client){fsh_error("Not enough memory to share files");fsh_phase=FSH_STOPPING;return;}
  portable_file_sharing_config config={PORTABLE_FILE_SHARING_EXPORT_INSTANCE,PORTABLE_FILE_SHARING_TCP_INSTANCE,PORTABLE_FILE_SHARING_ENTROPY_INSTANCE,{0},8080,FB_SHARE_LIFETIME_MS,true,false};
#ifdef PORTABLE_FILE_SHARING_SELECTED_VOLUME
  config.storage_volume=fsh_volume_share();
  config.files_instance=config.storage_volume?PORTABLE_FILE_BROWSER_INSTANCE:PORTABLE_FILE_SHARING_EXPORT_INSTANCE;
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
  if(config.storage_volume)config.files_instance=fb_root_instance(fb_storage_index);
#endif
#endif
  memcpy(config.address,fsh_link.ipv4.address,sizeof(config.address));
  snprintf(fsh_url,sizeof(fsh_url),"http://%u.%u.%u.%u:%u/",config.address[0],config.address[1],config.address[2],config.address[3],config.port);
  if(!portable_file_sharing_start(fsh_client,&config,now)){fsh_error("Sharing could not start");fsh_phase=FSH_STOPPING;return;}
  fsh_phase=FSH_ACTIVE;fb_dirty=true;return;
 }
 if(!portable_file_browser_services_begin())return;
 portable_file_sharing_tick(fsh_client,now);
 portable_file_sharing_status next;portable_file_sharing_get_status(fsh_client,&next);
 if(next.state==PORTABLE_FILE_SHARING_RETAINED){fsh_retain();return;}
 if(!portable_file_browser_services_end())return;
 if(next.state!=fsh_status.state||next.completed_requests!=fsh_status.completed_requests){fb_dirty=true;fsh_status=next;}
 if(next.state==PORTABLE_FILE_SHARING_OFF||next.state==PORTABLE_FILE_SHARING_ERROR){fsh_error(next.error?"Sharing stopped after an error":"Sharing session expired");fsh_phase=FSH_STOPPING;}
 uint32_t second=(uint32_t)now/1000u;if(second!=fsh_last_second){fsh_last_second=second;files_cosmetic_pending=true;}
}
static bool fsh_back(void){fsh_back_pending=true;if(fsh_close()){fsh_back_pending=false;fb_mode=FB_LIST;fb_loaded=false;(void)fb_load(0);}return false;}
static void fsh_action(void){if(fsh_phase==FSH_OFF)fsh_start();else {fsh_error("Stopping sharing...");(void)fsh_close();}fb_dirty=true;}
static void fsh_open(void){if(!portable_file_browser_close())return;
#ifdef PORTABLE_FILE_PEER
 fpeer_mode=false;
#endif
fb_mode=FB_SHARING;fb_choice=0;fsh_back_pending=false;fsh_message[0]=0;fb_dirty=true;}


