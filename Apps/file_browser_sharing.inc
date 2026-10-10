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
static bool fsh_setup_close(void);
static void fsh_setup_fence(void);
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
static void fsh_error(const char *message){snprintf(fsh_message,sizeof(fsh_message),"%s",message);fb_dirty=true;}
static void fsh_retain(void){
 fb_terminal=true;
#ifdef PORTABLE_FILE_SETUP
 fsh_setup_fence();
#endif
 portable_file_sharing_stop(fsh_client); /* Pure owner failure wipes local secrets. */
 memset(&fsh_status,0,sizeof(fsh_status));
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
 portable_adapter_retain_silent();
#else
 PORTABLE_APP_RETAIN();
#endif
}
static uint64_t fsh_clock(void){uint32_t now=fb_app->millis();fsh_now+=(uint32_t)(now-fsh_last_clock);fsh_last_clock=now;return fsh_now;}
bool portable_file_browser_sharing_active(void){return fsh_phase!=FSH_OFF;}
bool portable_file_browser_stop_pending(void){return fsh_phase==FSH_STOPPING&&!fb_terminal&&!PORTABLE_APP_RETAINED();}
bool portable_file_browser_network_owned(void){return fsh_network_initialized&&!fsh_link.borrowed&&fsh_phase!=FSH_OFF;}
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
bool portable_file_browser_services_begin(void){
 if(!fsh_owner(NULL))return false;
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return false;
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
 if(!fsh_network_initialized)return true;
 int rc=portable_network_session_service_end(&fsh_network);
 if(rc==PORTABLE_NETWORK_SERVICE_RETAINED){fsh_retain();return false;}
 fsh_service_ending=rc==PORTABLE_NETWORK_SERVICE_PENDING;
 return true; /* A pending lease end is drained before any later service work. */
}
static bool fsh_close(void){
 if(!fsh_owner(NULL))return false;
 if(fsh_phase==FSH_OFF)return true;
 fsh_phase=FSH_STOPPING;
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
 if(!fsh_release(&fsh_wifi_grant)||!fsh_release(&fsh_store_grant))return false;
 fsh_phase=FSH_OFF;fsh_url[0]=0;fb_dirty=true;return true;
}
static void fsh_start(void){
 if(fsh_phase!=FSH_OFF)return;
 if(!portable_background_stop()){fsh_retain();return;}
 fsh_now=0;fsh_last_clock=fb_app->millis();fsh_last_second=UINT32_MAX;
 fsh_message[0]=0;fsh_url[0]=0;memset(&fsh_status,0,sizeof(fsh_status));
 fsh_phase=FSH_POLICY;fb_dirty=true;
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
#include "file_browser_setup.inc"
#endif
static void fsh_tick(void){
 if(fsh_phase==FSH_OFF||!fsh_owner(NULL))return;
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only()){(void)fsh_setup_close();return;}
 fsh_setup_tick();if(!fsh_owner(NULL)||portable_file_browser_cleanup_only())return;
#endif
 uint64_t now=fsh_clock();
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
 if(fsh_phase==FSH_CONNECT){
  const portable_file_sharing_hooks hooks={NULL,fsh_owner,malloc,free};
  fsh_client=portable_file_sharing_create(fb_runtime,&hooks);
  if(!fsh_client){fsh_error("Not enough memory to share files");fsh_phase=FSH_STOPPING;return;}
  portable_file_sharing_config config={PORTABLE_FILE_SHARING_EXPORT_INSTANCE,PORTABLE_FILE_SHARING_TCP_INSTANCE,PORTABLE_FILE_SHARING_ENTROPY_INSTANCE,{0},8080,FB_SHARE_LIFETIME_MS,true};
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
 uint32_t second=(uint32_t)now/1000u;if(second!=fsh_last_second){fsh_last_second=second;fb_dirty=true;}
}
static bool fsh_back(void){fsh_back_pending=true;if(fsh_close()){fsh_back_pending=false;fb_mode=FB_LIST;fb_loaded=false;(void)fb_load(0);}return false;}
static void fsh_action(void){if(fsh_phase==FSH_OFF)fsh_start();else {fsh_error("Stopping sharing...");(void)fsh_close();}fb_dirty=true;}
static void fsh_open(void){if(!portable_file_browser_close())return;fb_mode=FB_SHARING;fb_choice=0;fsh_back_pending=false;fsh_message[0]=0;fb_dirty=true;}
static const char *fsh_title(void){return fsh_phase==FSH_OFF?"SHARE FILES":fsh_phase==FSH_STOPPING?"STOPPING":fsh_status.state==PORTABLE_FILE_SHARING_LISTENING||fsh_status.state==PORTABLE_FILE_SHARING_SERVING?"SHARING FILES":"CONNECTING";}
static void fsh_draw(void){
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return;
 if(fsh_setup_screen){fsh_setup_draw();return;}
#endif
 if(fb_paper&&!paper_frame_ready())return;
 const bool ready=fsh_phase==FSH_ACTIVE&&(fsh_status.state==PORTABLE_FILE_SHARING_LISTENING||fsh_status.state==PORTABLE_FILE_SHARING_SERVING);
 char detail[64];unsigned remaining=fsh_status.expires_ms>fsh_now?(unsigned)(fsh_status.expires_ms-fsh_now)/1000u:0;
 snprintf(detail,sizeof(detail),"%u:%02u left  /  %u requests",remaining/60,remaining%60,fsh_status.completed_requests);
 if(fb_paper){
  fb_paper->begin();fbp_text(32,28,fbp_w()-64,fsh_title(),true,true,false);fb_app->fill_rect(32,100,fbp_w()-64,3,true);
  if(ready){fbp_wrap(32,128,fbp_w()-64,2,fsh_url,true);fbp_text(32,220,fbp_w()-64,"USERNAME",false,true,false);fbp_text(32,252,fbp_w()-64,fsh_status.username,false,true,true);fbp_text(32,308,fbp_w()-64,"TEMPORARY PASSWORD",false,true,false);fbp_text(32,344,fbp_w()-64,fsh_status.password,true,true,true);fbp_text(32,408,fbp_w()-64,detail,false,true,false);}
  else fbp_wrap(32,132,fbp_w()-64,5,fsh_phase==FSH_OFF?"Share configured saved files for 5 minutes on your local Wi-Fi network. A fresh password is required for every session.":"Preparing the saved network and private session. Back cancels.",true);
#ifdef PORTABLE_FILE_SETUP
  if(ready)fbp_button(32,448,fbp_w()-64,48,"BLUETOOTH SETUP",NULL,false);
#endif
  fbp_text(32,fbp_h()-296,fbp_w()-64,"Authenticated HTTP.",false,true,true);
  fbp_text(32,fbp_h()-268,fbp_w()-64,"File contents are unencrypted.",false,true,true);
  fbp_text(32,fbp_h()-240,fbp_w()-64,"Use a trusted local network.",false,true,true);
  fbp_wrap(32,fbp_h()-190,fbp_w()-64,2,fsh_message,true);
  int half=(fbp_w()-80)/2;fbp_button(32,fbp_h()-112,half,88,"BACK",NULL,false);fbp_button(fbp_w()/2+8,fbp_h()-112,half,88,fsh_phase==FSH_OFF?"START HTTP":"STOP",NULL,true);
 }else{
  portable_nova_begin();portable_nova_header(fsh_title());
  if(ready){portable_nova_wrap(3,16,49,208,13,2,fsh_url,NOVA_CYAN);portable_nova_text(3,16,81,208,"USER: files",NOVA_CAP);portable_nova_text(2,16,101,208,fsh_status.password,NOVA_WHITE);portable_nova_text(3,16,128,208,fsh_message[0]?fsh_message:detail,NOVA_CAP);}
  else portable_nova_wrap(2,16,52,208,17,4,fsh_phase==FSH_OFF?"Share saved files for 5 minutes using your saved Wi-Fi network.":"Connecting... Back cancels.",NOVA_TEXT);
#ifdef PORTABLE_FILE_SETUP
  if(ready){portable_nova_button(16,146,208,24,"Bluetooth setup",false);portable_nova_wrap(3,16,174,208,10,2,"Authenticated HTTP. Contents unencrypted.",NOVA_CAP);}
  else
#endif
  portable_nova_wrap(3,16,149,208,12,3,"HTTP is authenticated. File contents are unencrypted.",NOVA_CAP);
  if(fsh_message[0]
#ifdef PORTABLE_FILE_SETUP
   &&!ready
#endif
  )portable_nova_wrap(3,16,178,208,12,1,fsh_message,NOVA_CYAN);
  portable_nova_button(16,196,84,36,"Back",false);portable_nova_button(108,196,116,36,fsh_phase==FSH_OFF?"Start HTTP":"Stop",true);
 }
 fb_app->present(false);fb_dirty=false;
}
static bool fsh_touch(int x,int y){
#ifdef PORTABLE_FILE_SETUP
 if(fsh_setup_screen)return fsh_setup_touch(x,y);
 if(fsh_phase==FSH_ACTIVE){
  if((fb_paper&&x>=32&&x<fbp_w()-32&&y>=448&&y<496)||(!fb_paper&&x>=16&&x<224&&y>=146&&y<170)){fsh_setup_start();return false;}
 }
#endif
 if(fb_paper){if(y>=fbp_h()-112&&y<fbp_h()-24&&x>=32&&x<fbp_w()-32){if(x<fbp_w()/2-8)return fsh_back();if(x>=fbp_w()/2+8)fsh_action();}}
 else if(x<52&&y<44)return fsh_back();else if(y>=196&&y<232){if(x>=16&&x<100)return fsh_back();if(x>=108&&x<224)fsh_action();}
 return false;
}
