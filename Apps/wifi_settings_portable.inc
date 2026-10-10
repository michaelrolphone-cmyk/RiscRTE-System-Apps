#if defined(PORTABLE_QUICK_RADIOS) || defined(PORTABLE_NATIVE_TIME_TOOLBAR)
/* The X4 native app obeys namespace-1 radio policy even when this app does
 * not expose radio toggles. Watch/default builds retain their opt-in. */
#define PORTABLE_WIFI_CHECK_RADIO_POLICY
#include "PortableRadioPolicy.h"
#endif
/* Explicit portable build only. Reader's firmware selector remains in the
 * #else branch of wifi_settings.c. No firmware UI, sockets or network stack. */
#include "T5AppApi.h"
#include "PortableWifiView.h"
#include "PaperFrame.h"
#include "PortableWifiCredentials.h"
#ifdef PORTABLE_TEXT_INPUT_CLIENT
#include "PortableTextInputClient.h"
#endif
#ifdef PORTABLE_WIFI_PROFILES
#include "PortableWifiProfiles.h"
#endif
#ifdef PORTABLE_BLE_BROADCAST
#include "PortableBroadcastClient.h"
#include "PortableNativeCustody.h"
#endif
#include "RiscRuntimeV1.h"
#include "PortableAppCustody.h"
#include "PortableStageLog.h"
#include "WifiApi.h"
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#if defined(PORTABLE_RETURN_APP)
#error "Wi-Fi owns nested Back; use WIFI_RETURN_APP, never PORTABLE_RETURN_APP"
#endif
#ifndef PORTABLE_WIFI_INSTANCE
#define PORTABLE_WIFI_INSTANCE 0u
#endif
#ifndef PORTABLE_WIFI_STORAGE_INSTANCE
#define PORTABLE_WIFI_STORAGE_INSTANCE PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE
#endif
#ifndef PORTABLE_WIFI_VERSION
#define PORTABLE_WIFI_VERSION "dev"
#endif

enum { WP_ROOT, WP_SSID, WP_PASSWORD, WP_SCAN, WP_FORGET, WP_HELP, WP_SAVED };
static const t5_app_api_v1 *wa;
static const risc_runtime_api_v1 *wr;
static risc_runtime_capability_v1 wg, wk;
static const wifi_api_v1 *wifi;
#ifdef WIFI_ASYNC_V1_SIZE
static const wifi_async_v1 *wifi_async;
static uint32_t wifi_operation,wifi_operation_kind;
static bool wifi_stopping,wifi_select_pending;
static risc_radio_request_v1 wifi_next_request;
static garden_radio_scan_entry_v1 wifi_pending_entry;
static bool wifi_async_stop(void);
static void wifi_async_tick(void);
static bool wifi_async_begin(uint32_t kind);
static void wifi_select_entry(const garden_radio_scan_entry_v1 *entry);
#endif
static const risc_key_value_v1 *wifi_store;
#if defined(PORTABLE_BLE_BROADCAST) && !defined(PORTABLE_NATIVE_TIME_TOOLBAR)
static const risc_key_value_v1 *wifi_store_source;
static risc_key_value_v1 wifi_store_checked;
static int32_t wifi_store_result(int32_t result) {
 if(result!=RISC_KEY_VALUE_OK && result!=RISC_KEY_VALUE_NOT_FOUND && result!=RISC_KEY_VALUE_IO &&
    result!=RISC_KEY_VALUE_INVALID && result!=RISC_KEY_VALUE_BUFFER_SMALL && result!=RISC_KEY_VALUE_BUSY)portable_adapter_retain();
 return result;
}
static int32_t wifi_store_get(void*c,const char*k,void*b,uint32_t size,uint32_t*used) {
 (void)c;if(portable_adapter_retained() || !portable_broadcast_stop())return RISC_KEY_VALUE_CONTEXT;
 return wifi_store_result(wifi_store_source->get(wifi_store_source->context,k,b,size,used));
}
static int32_t wifi_store_put(void*c,const char*k,const void*b,uint32_t size) {
 (void)c;if(portable_adapter_retained() || !portable_broadcast_stop())return RISC_KEY_VALUE_CONTEXT;
 return wifi_store_result(wifi_store_source->put(wifi_store_source->context,k,b,size));
}
#endif
static portable_wifi_credentials credentials;
static garden_radio_scan_result_v1 scan_result;
static portable_wifi_view view;
static char editor[64], masked[72], status_text[64], ip_text[48];
static char scan_values[GARDEN_RADIO_SCAN_MAX][40];
static const char *wifi_message;
static unsigned page, choice, key_page;
#if defined(PORTABLE_TOUCH_SCROLL)
static unsigned wifi_view_identity;
#define WIFI_NEW_VIEW() (++wifi_view_identity)
#define WIFI_SCROLL_CANCEL() portable_wifi_scroll_interrupt()
#else
#define WIFI_NEW_VIEW() ((void)0)
#define WIFI_SCROLL_CANCEL() ((void)0)
#endif
static bool wifi_back_return,wifi_scan_back;
static bool opened, dirty, armed, open_network, scanning, scan_owned, joining, cleanup_pending, draft_dirty;
static uint32_t operation_started,last_status;
static uint32_t wifi_service_lease;static unsigned wifi_service_depth;
static wifi_link_t link_state;
static int saved_state;
#ifdef PORTABLE_WIFI_PROFILES
static portable_wifi_profile_search profile_search;
static portable_wifi_credentials profile_draft;
static unsigned profile_work;
enum { WIFI_PROFILE_SAVE=1, WIFI_PROFILE_SCAN, WIFI_PROFILE_FORGET };
static uint32_t saved_base,saved_count,forget_index;
static unsigned saved_loading,saved_rows;
static bool saved_loading_active,forget_index_valid,saved_open_pending,saved_select_pending,forget_pending;
static uint32_t saved_pending_base,saved_pending_slot;
static char saved_names[PORTABLE_WIFI_PROFILE_PAGE][33];
static int saved_results[PORTABLE_WIFI_PROFILE_PAGE];
static void wifi_forget(void);
static void wifi_saved_select(uint32_t slot);
static void wifi_profiles_step(void);
static void wifi_saved_open(uint32_t base);
static void wifi_profile_start(unsigned work);
#endif
#ifdef PORTABLE_TEXT_INPUT_CLIENT
static portable_text_client wifi_text;
static risc_text_entry_state_v1 wifi_text_state;
static bool wifi_text_active,wifi_text_closing,wifi_home_return;
static unsigned wifi_text_field;
static void wifi_text_step(void);
#endif
static void wifi_zero(void *p,size_t n) {volatile unsigned char *b=p;while(n--)*b++=0;}
static size_t wifi_length(const char *s,size_t cap) {
  size_t n=0;while(n<cap && s[n])++n;return n;
}
static bool wifi_api_valid(const wifi_api_v1 *w) {
  return w && w->api_version==1 && w->struct_size>=WIFI_MANAGEMENT_V1_SIZE &&
    w->connect && w->status && w->rssi && w->addresses && w->scan_start &&
    w->scan_poll && w->scan_cancel && w->disconnect_checked;
}
static bool wifi_acquire(void) {
 if(PORTABLE_APP_RETAINED())return false;
  if(wifi)return true;
  if(wg.api)return false; /* Invalid or uncertain grant must be released first. */
  wg=(risc_runtime_capability_v1){.struct_size=sizeof(wg)};
  if(!wr->acquire("net.wifi",1,PORTABLE_WIFI_INSTANCE,&wg)){portable_stage_log(wr,"wifi-provider","result=acquire-failed");return false;}
  if(!wifi_api_valid(wg.api)) {
    PORTABLE_APP_RETAIN();if(PORTABLE_APP_RETAINED())return false;
    portable_stage_log(wr,"wifi-provider","result=invalid-api");
    if(wr->release(&wg))memset(&wg,0,sizeof(wg));
    return false;
  }
  wifi=wg.api;
#ifdef WIFI_ASYNC_V1_SIZE
  wifi_async=NULL;
  if(wifi->struct_size>=WIFI_ASYNC_V1_SIZE){
    const wifi_async_v1 *ext=(const wifi_async_v1*)wifi;
    if(ext->async_tag==RISC_RADIO_ASYNC_TAG&&ext->async_version==RISC_RADIO_ASYNC_VERSION&&ext->begin&&ext->poll&&ext->cancel&&ext->service_begin&&ext->service_end)wifi_async=ext;
  }
#endif
  portable_stage_log(wr,"wifi-provider","result=ready connect=not-requested");return true;
}
static bool wifi_disconnect(void) {
 if(PORTABLE_APP_RETAINED())return false;
#ifdef PORTABLE_BLE_BROADCAST
  if(!portable_broadcast_stop())return false;
#endif
  if(!wifi) {cleanup_pending=wg.api!=NULL;return !cleanup_pending;}
#ifdef WIFI_ASYNC_V1_SIZE
  wifi_zero(&wifi_next_request,sizeof(wifi_next_request));wifi_select_pending=false;wifi_zero(&wifi_pending_entry,sizeof(wifi_pending_entry));
  if(wifi_operation&&!wifi_async_stop())return false;
#endif
  if(!wifi->disconnect_checked(wifi->context)) {
    PORTABLE_APP_RETAIN();
    portable_stage_log(wr,"wifi-disconnect","result=failed cleanup=required");
    cleanup_pending=true;strcpy(status_text,"Cleanup required");ip_text[0]=0;wifi_message="Cleanup failed - retry Disconnect";dirty=true;return false;
  }
  scanning=scan_owned=joining=cleanup_pending=false;link_state=WIFI_LINK_DOWN;
  portable_stage_log(wr,"wifi-disconnect","result=ok state=down");
  strcpy(status_text,"Disconnected");ip_text[0]=0;
  memset(&scan_result,0,sizeof(scan_result));dirty=true;return true;
}
#if defined(PORTABLE_X4_IDLE_POLICY) || defined(PORTABLE_RESIDENT_POLICY) || defined(TEST_IDLE_ELIGIBILITY)
bool portable_wifi_idle_ready(void) {
 if(PORTABLE_APP_RETAINED())return false;
 return !opened || (!scanning && !scan_owned && !joining && !cleanup_pending && link_state==WIFI_LINK_DOWN
#ifdef PORTABLE_TEXT_INPUT_CLIENT
 && !wifi_text_active && !wifi_home_return
#endif
 );
}
#endif
bool portable_wifi_async_owned(void) {
#ifdef WIFI_ASYNC_V1_SIZE
  return opened && wifi_operation!=0;
#else
  return false;
#endif
}
bool portable_wifi_services_begin(void) {
  if(PORTABLE_APP_RETAINED())return false;
  if(wifi_service_depth){if(wifi_service_depth==UINT32_MAX){PORTABLE_APP_RETAIN();return false;}++wifi_service_depth;return true;}
#ifdef WIFI_ASYNC_V1_SIZE
  if(wifi_operation){
    uint32_t token=0;int rc=wifi_async->service_begin(wifi->context,&token);
    if(PORTABLE_APP_RETAINED())return false;
    if((rc==RISC_RADIO_BUSY||rc==RISC_RADIO_AGAIN)&&!token)return false;
    if(rc!=RISC_RADIO_ACCEPTED||!token){PORTABLE_APP_RETAIN();cleanup_pending=true;return false;}
    wifi_service_lease=token;
  }
#endif
  wifi_service_depth=1;return true;
}
bool portable_wifi_services_end(void) {
  if(PORTABLE_APP_RETAINED())return false;
  if(!wifi_service_depth){PORTABLE_APP_RETAIN();return false;}
  if(--wifi_service_depth)return true;
#ifdef WIFI_ASYNC_V1_SIZE
  if(wifi_service_lease){
    int rc=wifi_async->service_end(wifi->context,wifi_service_lease);
    if(PORTABLE_APP_RETAINED())return false;
    if(rc!=RISC_RADIO_ACCEPTED){PORTABLE_APP_RETAIN();cleanup_pending=true;return false;}
    wifi_service_lease=0;
  }
#endif
  return true;
}
bool portable_wifi_stop_pending(void) {
#ifdef WIFI_ASYNC_V1_SIZE
 return wifi_stopping&&wifi_operation&&!cleanup_pending&&!PORTABLE_APP_RETAINED();
#else
 return false;
#endif
}
bool portable_wifi_suspend(void) {
 if(PORTABLE_APP_RETAINED())return false;
  WIFI_SCROLL_CANCEL();
  if(!opened)return true;
  if(!portable_wifi_stop_pending())portable_stage_log(wr,"wifi-suspend","begin disconnect=required");
  if(wifi && !wifi_disconnect())return false;
  if(wg.api && !wr->release(&wg)) {
    portable_stage_log(wr,"wifi-suspend","result=release-failed");
    cleanup_pending=true;wifi_message="Release failed - retry Disconnect";dirty=true;return false;
  }
  memset(&wg,0,sizeof(wg));wifi=NULL;cleanup_pending=false;
#ifdef WIFI_ASYNC_V1_SIZE
  wifi_async=NULL;
#endif
  portable_stage_log(wr,"wifi-suspend","result=ok grant=released");return true;
}
void portable_wifi_resume(void) {
 if(PORTABLE_APP_RETAINED())return;
  if(!opened)return;
  wifi_message=wifi_acquire()?"Woke - connect when ready":"Wi-Fi unavailable - retry Connect";
  link_state=WIFI_LINK_DOWN;last_status=wa->millis();armed=false;dirty=true;
}
bool portable_wifi_close(void) {
 if(PORTABLE_APP_RETAINED())return false;
  if(!opened)return true;
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  if(wifi_text_active)return false;
#endif
  if(!portable_wifi_suspend())return false;
  if(wk.api && !wr->release(&wk))return false;
  memset(&wk,0,sizeof(wk));wifi_store=NULL;
  portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));
#ifdef PORTABLE_WIFI_PROFILES
  profile_work=0;saved_loading_active=forget_index_valid=saved_open_pending=saved_select_pending=forget_pending=false;portable_wifi_credentials_clear(&profile_draft);wifi_zero(&profile_search,sizeof(profile_search));wifi_zero(saved_names,sizeof(saved_names));
#endif
  wifi_zero(&scan_result,sizeof(scan_result));memset(&view,0,sizeof(view));opened=false;return true;
}
static void wifi_page(unsigned next_page) {
  WIFI_SCROLL_CANCEL();WIFI_NEW_VIEW();
#ifdef PORTABLE_WIFI_PROFILES
  saved_loading_active=false;
#endif
  page=next_page;choice=0;armed=false;dirty=true;
}
static void wifi_edit(unsigned field) {
  wifi_zero(editor,sizeof(editor));
  const char *source=field==WP_SSID?credentials.ssid:credentials.password;
  size_t n=wifi_length(source,field==WP_SSID?32:63);memcpy(editor,source,n);
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  wifi_text_field=field;wifi_text_closing=false;
  wifi_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(wifi_text_state)};
  int rc=portable_text_client_begin_mode(&wifi_text,wr,field==WP_PASSWORD?"Wi-Fi password":"Network name",editor,field==WP_PASSWORD?64u:33u,field==WP_PASSWORD);
  if(rc==RISC_TEXT_ENTRY_RETAINED||wifi_text.retained)return;
  if(rc==RISC_TEXT_ENTRY_OK){wifi_text_active=true;return;}
  wifi_zero(editor,sizeof(editor));wifi_message="Shared keyboard unavailable - retry";dirty=true;
#else
  key_page=PWK_INITIAL_PAGE;
  wifi_page(field); /* Start on lowercase; all printable ASCII is reachable. */
#endif
}
static bool wifi_draft_valid(void) {
  if(!credentials.ssid[0]) {wifi_message="Enter a network name";return false;}
  if(!open_network && !credentials.password[0]) {wifi_message="Enter an 8-63 character password";return false;}
  if(!portable_wifi_credentials_validate(&credentials)) {wifi_message="Password must be empty or 8-63 chars";return false;}
  return true;
}
#ifdef PORTABLE_WIFI_CHECK_RADIO_POLICY
static bool wifi_policy_busy;
static bool wifi_policy_allows(const char *stage) {
  (void)stage;wifi_policy_busy=false;
  unsigned permission=portable_radio_wifi_permission(wr);
  if(PORTABLE_APP_RETAINED())return false;
  if(permission==PORTABLE_WIFI_POLICY_ALLOWED)return true;
  if(permission==PORTABLE_WIFI_POLICY_BUSY){wifi_policy_busy=true;wifi_message="Waiting for radio settings - Back cancels";dirty=true;return false;}
  bool off=permission==PORTABLE_WIFI_POLICY_OFF;
  portable_stage_log(wr,stage,off?"result=blocked radio-policy=off":"result=blocked radio-policy=unreadable");
  wifi_message=off?"Wi-Fi is off in Quick Controls":"Radio settings unavailable; retry";
  dirty=true;return false;
}
#endif
static void wifi_connect(void) {
 if(PORTABLE_APP_RETAINED())return;
#ifdef PORTABLE_BLE_BROADCAST
  if(!portable_broadcast_stop())return ;
#endif
  if(!wifi_draft_valid()){dirty=true;return;}
  if(!wifi_acquire()) {wifi_message="Wi-Fi unavailable - retry Connect";dirty=true;return;}
#ifdef WIFI_ASYNC_V1_SIZE
  if(wifi_async_begin(RISC_RADIO_REQUEST_JOIN))return;
#endif
#ifdef PORTABLE_WIFI_CHECK_RADIO_POLICY
 if(!wifi_policy_allows("wifi-connect"))return;
#endif
  if(!wifi_disconnect())return;
  if(!wifi->connect(wifi->context,credentials.ssid,credentials.password)) {
    portable_stage_log(wr,"wifi-connect","result=failed cleanup=required");
    if(wifi_disconnect())wifi_message="Connect failed - retry";
    dirty=true;return;
  }
  joining=true;link_state=WIFI_LINK_JOINING;strcpy(status_text,"Connecting...");operation_started=wa->millis();
  portable_stage_log(wr,"wifi-connect","result=accepted state=joining");
  wifi_message="Connecting - select Cancel to stop";dirty=true;
}
static void wifi_scan_start(void) {
 if(PORTABLE_APP_RETAINED())return;
#ifdef PORTABLE_BLE_BROADCAST
  if(!portable_broadcast_stop())return ;
#endif
  if(!wifi_acquire()){wifi_message="Wi-Fi unavailable - enter manually";dirty=true;return;}
#ifdef WIFI_ASYNC_V1_SIZE
  if(wifi_async_begin(RISC_RADIO_REQUEST_SCAN))return;
#endif
#ifdef PORTABLE_WIFI_CHECK_RADIO_POLICY
 if(!wifi_policy_allows("wifi-scan"))return;
#endif
  if(!wifi_disconnect())return;
  if(!wifi->scan_start(wifi->context)){portable_stage_log(wr,"wifi-scan","result=start-failed");if(wifi_disconnect())wifi_message="Scan failed - retry or enter manually";dirty=true;return;}
  scan_result=(garden_radio_scan_result_v1){.struct_size=sizeof(scan_result)};
  scanning=scan_owned=true;operation_started=wa->millis();wifi_message="Scanning - Back cancels";wifi_page(WP_SCAN);
  portable_stage_log(wr,"wifi-scan","result=accepted state=scanning");
}
static bool wifi_scan_valid(const garden_radio_scan_result_v1 *r) {
  if(r->struct_size<sizeof(*r) || r->count>GARDEN_RADIO_SCAN_MAX ||
     r->state>GARDEN_RADIO_SCAN_FAILED || r->reserved)return false;
  for(unsigned i=0;i<r->count;++i)if(r->entries[i].reserved ||
      wifi_length(r->entries[i].ssid,sizeof(r->entries[i].ssid))==sizeof(r->entries[i].ssid))return false;
  return true;
}
static bool wifi_cancel_scan(void) {
 if(PORTABLE_APP_RETAINED())return false;
#ifdef WIFI_ASYNC_V1_SIZE
  wifi_zero(&wifi_next_request,sizeof(wifi_next_request));
  if(wifi_operation)return wifi_async_stop();
#endif
  if(!wifi || !wifi->scan_cancel(wifi->context)) {
    PORTABLE_APP_RETAIN();
    portable_stage_log(wr,"wifi-scan-cancel","result=failed cleanup=required");
    cleanup_pending=true;wifi_message="Scan cleanup failed - retry Cancel";dirty=true;return false;
  }
  scanning=scan_owned=false;cleanup_pending=false;portable_stage_log(wr,"wifi-scan-cancel","result=ok");return true;
}
#ifdef WIFI_ASYNC_V1_SIZE
static void wifi_async_failed(void) {
  cleanup_pending=true;wifi_stopping=false;
  wifi_zero(&wifi_next_request,sizeof(wifi_next_request));wifi_select_pending=false;
  PORTABLE_APP_RETAIN();
}
static bool wifi_async_submit(void) {
  uint32_t kind=wifi_next_request.kind,id=0;
#ifdef PORTABLE_WIFI_CHECK_RADIO_POLICY
  if(!wifi_policy_allows(kind==RISC_RADIO_REQUEST_SCAN?"wifi-scan":"wifi-connect")) {
    if(!wifi_policy_busy)wifi_zero(&wifi_next_request,sizeof(wifi_next_request));
    return true;
  }
#endif
  int rc=wifi_async->begin(wifi->context,&wifi_next_request,&id);
  if(PORTABLE_APP_RETAINED())return true;
  if(rc==RISC_RADIO_AGAIN){wifi_message="Starting Wi-Fi - Back cancels";dirty=true;return true;}
  wifi_zero(&wifi_next_request,sizeof(wifi_next_request));
  if(rc==RISC_RADIO_RETAINED||(rc==RISC_RADIO_ACCEPTED&&!id)||(rc!=RISC_RADIO_ACCEPTED&&id)){wifi_async_failed();return true;}
  if(rc!=RISC_RADIO_ACCEPTED){wifi_message="Wi-Fi start unavailable - retry";dirty=true;return true;}
  wifi_operation=id;wifi_operation_kind=kind;wifi_stopping=false;cleanup_pending=false;
  operation_started=wa->millis();last_status=operation_started-250u;
  scanning=scan_owned=kind==RISC_RADIO_REQUEST_SCAN;joining=kind==RISC_RADIO_REQUEST_JOIN;
  link_state=joining?WIFI_LINK_JOINING:WIFI_LINK_DOWN;
  if(scanning){scan_result=(garden_radio_scan_result_v1){.struct_size=sizeof(scan_result)};wifi_page(WP_SCAN);}
  strcpy(status_text,scanning?"Starting scan...":"Connecting...");
  wifi_message=scanning?"Scanning - Back cancels":"Connecting - select Cancel to stop";dirty=true;
  portable_stage_log(wr,"wifi-async-start",scanning?"result=accepted kind=scan":"result=accepted kind=join");return true;
}
static bool wifi_async_begin(uint32_t kind) {
  if(!wifi_async)return false;
  wifi_zero(&wifi_next_request,sizeof(wifi_next_request));
  wifi_next_request=(risc_radio_request_v1){.struct_size=sizeof(wifi_next_request),.kind=kind};
  if(kind==RISC_RADIO_REQUEST_JOIN){memcpy(wifi_next_request.ssid,credentials.ssid,sizeof(credentials.ssid));memcpy(wifi_next_request.password,credentials.password,sizeof(credentials.password));}
  if(wifi_operation){(void)wifi_async_stop();return true;}
  return wifi_async_submit();
}
static bool wifi_async_stop(void) {
  if(!wifi_operation)return true;
  int rc=wifi_async->cancel(wifi->context,wifi_operation);
  if(PORTABLE_APP_RETAINED())return false;
  if(rc!=RISC_RADIO_PENDING&&rc!=RISC_RADIO_AGAIN&&rc!=RISC_RADIO_QUIESCENT){wifi_async_failed();return false;}
  if(!wifi_stopping){wifi_message="Stopping Wi-Fi - cleanup pending";strcpy(status_text,"Stopping...");dirty=true;}
  wifi_stopping=true;joining=scanning=false;
  if(rc==RISC_RADIO_QUIESCENT)wifi_async_tick();
  return !wifi_operation&&!cleanup_pending;
}
static void wifi_async_tick(void) {
  if(!wifi_operation){
    if(wifi_next_request.kind&&!wifi_async_submit()){wifi_message="Wi-Fi async service unavailable - retry";dirty=true;}
    return;
  }
  risc_radio_progress_v1 progress={.struct_size=sizeof(progress)};
  int rc=wifi_async->poll(wifi->context,wifi_operation,&progress);
  if(PORTABLE_APP_RETAINED()||rc==RISC_RADIO_AGAIN)return;
  if(rc==RISC_RADIO_RETAINED){wifi_async_failed();return;}
  if((rc!=RISC_RADIO_PENDING&&rc!=RISC_RADIO_QUIESCENT)||progress.struct_size!=sizeof(progress)||progress.operation_id!=wifi_operation||
      progress.phase<RISC_RADIO_QUEUED||progress.phase>RISC_RADIO_IDLE||progress.failure>RISC_RADIO_FAILURE_CANCELLED||
      progress.owned>1||progress.quiescent>1||progress.station_state>WIFI_LINK_UP||
      (progress.quiescent!=(progress.phase==RISC_RADIO_IDLE))||(progress.owned==progress.quiescent)||
      ((rc==RISC_RADIO_QUIESCENT)!=(progress.quiescent!=0))||progress.reserved[0]||progress.reserved[1]||progress.reserved[2]||progress.reserved[3]||
      (progress.phase==RISC_RADIO_RESULTS&&wifi_operation_kind!=RISC_RADIO_REQUEST_SCAN)||
      (progress.phase==RISC_RADIO_CONNECTED&&wifi_operation_kind!=RISC_RADIO_REQUEST_JOIN)||!wifi_scan_valid(&progress.scan)) {wifi_async_failed();return;}
  if(progress.quiescent){
    bool cancelled=wifi_stopping;wifi_operation=0;wifi_stopping=scanning=scan_owned=joining=cleanup_pending=false;link_state=WIFI_LINK_DOWN;
    strcpy(status_text,"Disconnected");ip_text[0]=0;
    if(wifi_scan_back){wifi_scan_back=false;wifi_page(WP_ROOT);}
    wifi_message=progress.failure&&progress.failure!=RISC_RADIO_FAILURE_CANCELLED?"Wi-Fi operation failed - retry":cancelled?"Wi-Fi stopped":"Disconnected";dirty=true;
    portable_stage_log(wr,"wifi-async-stop","result=quiescent");
    if(wifi_next_request.kind){(void)wifi_async_submit();return;}
    if(wifi_select_pending){garden_radio_scan_entry_v1 selected=wifi_pending_entry;wifi_select_pending=false;wifi_zero(&wifi_pending_entry,sizeof(wifi_pending_entry));wifi_select_entry(&selected);wifi_zero(&selected,sizeof(selected));}
    return;
  }
  if(wifi_stopping)return;
  uint32_t elapsed=(uint32_t)(wa->millis()-operation_started);
  if((wifi_operation_kind==RISC_RADIO_REQUEST_SCAN&&progress.phase!=RISC_RADIO_RESULTS&&elapsed>=15000u)||
     (wifi_operation_kind==RISC_RADIO_REQUEST_JOIN&&progress.phase!=RISC_RADIO_CONNECTED&&elapsed>=30000u)){
    (void)wifi_async_stop();return;
  }
  if(progress.phase==RISC_RADIO_RESULTS){
    bool changed=scanning||memcmp(&scan_result,&progress.scan,sizeof(scan_result));
    if(changed){WIFI_SCROLL_CANCEL();WIFI_NEW_VIEW();scan_result=progress.scan;scanning=false;wifi_message=scan_result.count?"Choose a network or scan again":"No networks - enter manually";dirty=true;}
  }else if(progress.phase==RISC_RADIO_CONNECTED){
    char state[64],ip[48];snprintf(state,sizeof(state),"Connected  %d dBm",(int)progress.rssi);
    snprintf(ip,sizeof(ip),"IPv4 %u.%u.%u.%u",progress.station[0],progress.station[1],progress.station[2],progress.station[3]);
    if(link_state!=WIFI_LINK_UP||strcmp(state,status_text)||strcmp(ip,ip_text))dirty=true;
    link_state=WIFI_LINK_UP;joining=false;strcpy(status_text,state);strcpy(ip_text,ip);wifi_message="Connected; Save stores this network";
  }
}
#endif
static void wifi_tick(void) {
 if(PORTABLE_APP_RETAINED())return;
  uint32_t now=wa->millis();
  if((uint32_t)(now-last_status)<250)return;
  last_status=now;
  if(!wifi || cleanup_pending)return;
#ifdef WIFI_ASYNC_V1_SIZE
  if(wifi_async&&(wifi_operation||wifi_next_request.kind)){wifi_async_tick();return;}
#endif
  if(scan_owned) {
    garden_radio_scan_result_v1 next_scan={.struct_size=sizeof(next_scan)};
    bool ok=wifi->scan_poll(wifi->context,&next_scan) && wifi_scan_valid(&next_scan);
    if(!ok || next_scan.state==GARDEN_RADIO_SCAN_FAILED || (scanning && (uint32_t)(now-operation_started)>=15000u)) {
      portable_stage_log(wr,"wifi-scan",!ok?"result=invalid-status":next_scan.state==GARDEN_RADIO_SCAN_FAILED?"result=failed":"result=timeout");
      if(wifi_cancel_scan())wifi_message=ok?"Scan ended - retry or enter manually":"Scan unavailable - retry";
      dirty=true;
    } else if(next_scan.state==GARDEN_RADIO_SCAN_DONE) {
      bool changed=scanning || memcmp(&scan_result,&next_scan,sizeof(next_scan));
      if(changed)portable_stage_log(wr,"wifi-scan",next_scan.count?"result=complete networks=found":"result=complete networks=none");
      if(changed){WIFI_SCROLL_CANCEL();WIFI_NEW_VIEW();}
      scan_result=next_scan;scanning=false;wifi_message=scan_result.count?"Choose a network or scan again":"No networks - enter manually";
      dirty|=changed;
    }
    wifi_zero(&next_scan,sizeof(next_scan));
    return;
  }
  wifi_link_t state=wifi->status(wifi->context);
  if(state>WIFI_LINK_UP) {if(wifi_disconnect())wifi_message="Invalid status - retry Connect";dirty=true;return;}
  if(state!=link_state) {
    portable_stage_log(wr,"wifi-link",state==WIFI_LINK_UP?"state=up":state==WIFI_LINK_JOINING?"state=joining":"state=down");
    bool was_joining=joining;
    if(state==WIFI_LINK_DOWN && !wifi_disconnect())return;
    link_state=state;dirty=true;
    if(state==WIFI_LINK_UP){joining=false;wifi_message="Connected; Save stores this network";}
    else if(state==WIFI_LINK_DOWN && was_joining){joining=false;wifi_message="Connection failed - retry Connect";}
  }
  if(joining && (uint32_t)(now-operation_started)>=30000u) {
    portable_stage_log(wr,"wifi-connect","result=timeout");
    if(wifi_disconnect())wifi_message="Connection timed out - retry";
    dirty=true;
  }
  char status[64],address[48];
  if(link_state==WIFI_LINK_UP) {
    snprintf(status,sizeof(status),"Connected  %d dBm",(int)wifi->rssi(wifi->context));
    wifi_ipv4_v1 ip={0},access_point={0};
    if(wifi->addresses(wifi->context,&ip,&access_point))snprintf(address,sizeof(address),"IPv4 %u.%u.%u.%u",ip.address[0],ip.address[1],ip.address[2],ip.address[3]);
    else {if(wifi_disconnect())wifi_message="Address read failed - retry Connect";dirty=true;return;}
  } else {strcpy(status,joining?"Connecting...":"Disconnected");address[0]=0;}
  if(strcmp(status,status_text) || strcmp(address,ip_text))dirty=true;
  strcpy(status_text,status);strcpy(ip_text,address);
}
#ifdef PORTABLE_TEXT_INPUT_CLIENT
static void wifi_text_step(void) {
  int rc;
  if(!wifi_text_closing){
    rc=portable_text_client_poll(&wifi_text,&wifi_text_state);
    if(rc==RISC_TEXT_ENTRY_AGAIN)return;
    if(rc==RISC_TEXT_ENTRY_RETAINED||wifi_text.retained)return;
    if(rc!=RISC_TEXT_ENTRY_OK){wifi_text_state.state=RISC_TEXT_ENTRY_CANCELLED;wifi_message="Keyboard unavailable";}
    else if(wifi_text_state.state==RISC_TEXT_ENTRY_EDITING)return;
    wifi_text_closing=true;
  }
  rc=portable_text_client_close(&wifi_text);
  if(rc==RISC_TEXT_ENTRY_AGAIN)return;
  if(rc!=RISC_TEXT_ENTRY_OK||wifi_text.retained)return;
  wifi_text_active=wifi_text_closing=false;
  wifi_home_return=(wifi_text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)!=0;
  if(wifi_text_state.state==RISC_TEXT_ENTRY_ACCEPTED){
    char *destination=wifi_text_field==WP_PASSWORD?credentials.password:credentials.ssid;
    size_t capacity=wifi_text_field==WP_PASSWORD?sizeof(credentials.password):sizeof(credentials.ssid);
    size_t n=wifi_length(wifi_text_state.text,capacity);
    if(n<capacity){wifi_zero(destination,capacity);memcpy(destination,wifi_text_state.text,n);draft_dirty=true;if(wifi_text_field==WP_PASSWORD&&n)open_network=false;wifi_message="Draft changed; Connect or Save";}
    else wifi_message="Invalid keyboard result";
  }else wifi_message="Entry cancelled";
  wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));wifi_zero(&wifi_text_state,sizeof(wifi_text_state));
  wifi_page(WP_ROOT);
}
#endif
#ifdef PORTABLE_WIFI_PROFILES
static void wifi_profile_start(unsigned work) {
  if(profile_work)return;
  portable_wifi_credentials_clear(&profile_draft);profile_draft=credentials;
  int rc=portable_wifi_profile_search_begin(wifi_store,credentials.ssid,&profile_search);
  if(PORTABLE_APP_RETAINED())return;
  if(rc!=PORTABLE_WIFI_PROFILE_AGAIN){wifi_message="Saved networks unavailable - retry";dirty=true;portable_wifi_credentials_clear(&profile_draft);return;}
  profile_work=work;wifi_message="Checking saved networks - Back cancels";dirty=true;
}
static void wifi_saved_open(uint32_t base) {
  uint32_t count=0;int rc=portable_wifi_profile_count(wifi_store,&count);
  if(PORTABLE_APP_RETAINED())return;
  if(rc==PORTABLE_WIFI_CREDENTIALS_AGAIN){saved_open_pending=true;saved_pending_base=base;wifi_message="Loading saved networks - Back cancels";dirty=true;return;}
  saved_open_pending=false;
  if(rc||base>=count){wifi_message="Saved networks unavailable - retry";dirty=true;return;}
  wifi_page(WP_SAVED);saved_count=count;saved_base=base;saved_rows=(unsigned)(saved_count-base);
  if(saved_rows>PORTABLE_WIFI_PROFILE_PAGE)saved_rows=PORTABLE_WIFI_PROFILE_PAGE;
  memset(saved_names,0,sizeof(saved_names));memset(saved_results,0,sizeof(saved_results));
  saved_loading=0;saved_loading_active=true;wifi_message="Loading saved networks - Back cancels";
}
static void wifi_profiles_step(void) {
  if(forget_pending){wifi_forget();return;}
  if(saved_open_pending){wifi_saved_open(saved_pending_base);return;}
  if(saved_select_pending){wifi_saved_select(saved_pending_slot);return;}
  if(profile_work){
    int rc=portable_wifi_profile_search_step(wifi_store,&profile_search);
    if(PORTABLE_APP_RETAINED()||rc==PORTABLE_WIFI_PROFILE_AGAIN)return;
    unsigned work=profile_work;uint32_t slot=profile_search.index;
    if(work==WIFI_PROFILE_SAVE){
      if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED||rc==PORTABLE_WIFI_CREDENTIALS_EMPTY)rc=portable_wifi_profile_save(wifi_store,slot,&profile_draft);
      if(PORTABLE_APP_RETAINED())return;
      if(rc==PORTABLE_WIFI_CREDENTIALS_AGAIN)return;
      saved_state=rc;if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED)draft_dirty=false;
      wifi_message=rc==PORTABLE_WIFI_CREDENTIALS_LOADED?"Network saved; storage is not a secure vault":rc==PORTABLE_WIFI_PROFILE_FULL?"Storage index exhausted; forget an unused network":"Save unconfirmed - retry";
    }else if(work==WIFI_PROFILE_FORGET){
      if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED){forget_index=slot;forget_index_valid=true;wifi_page(WP_FORGET);}
      else wifi_message=rc==PORTABLE_WIFI_CREDENTIALS_EMPTY?"This network is not saved":"Cannot confirm saved network - retry";
    }else{
      portable_wifi_credentials stored={{0},{0}};
      int loaded_result=rc==PORTABLE_WIFI_CREDENTIALS_LOADED?portable_wifi_profile_load(wifi_store,slot,&stored):rc;
      if(loaded_result==PORTABLE_WIFI_CREDENTIALS_AGAIN){portable_wifi_credentials_clear(&stored);return;}
      bool loaded=loaded_result==PORTABLE_WIFI_CREDENTIALS_LOADED;
      if(PORTABLE_APP_RETAINED()){portable_wifi_credentials_clear(&stored);return;}
      if(loaded&&!open_network&&stored.password[0]){credentials=stored;draft_dirty=false;wifi_message="Saved network selected; Connect when ready";wifi_page(WP_ROOT);}
      else if(open_network){wifi_message="Open network selected; Connect when ready";wifi_page(WP_ROOT);}
      else wifi_edit(WP_PASSWORD);
      portable_wifi_credentials_clear(&stored);
    }
    profile_work=0;portable_wifi_credentials_clear(&profile_draft);wifi_zero(&profile_search,sizeof(profile_search));dirty=true;return;
  }
  if(saved_loading_active){
    portable_wifi_credentials stored={{0},{0}};unsigned row=saved_loading;
    int rc=portable_wifi_profile_load(wifi_store,saved_base+row,&stored);
    if(PORTABLE_APP_RETAINED()){portable_wifi_credentials_clear(&stored);return;}
    if(rc==PORTABLE_WIFI_CREDENTIALS_AGAIN){portable_wifi_credentials_clear(&stored);return;}
    saved_results[row]=rc;if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED)memcpy(saved_names[row],stored.ssid,sizeof(stored.ssid));
    portable_wifi_credentials_clear(&stored);++saved_loading;
    if(saved_loading==saved_rows){saved_loading_active=false;wifi_message="Choose a saved network; Back returns";}
    dirty=true;
  }
}
#endif
bool portable_wifi_services_safe(void) {
 if(PORTABLE_APP_RETAINED())return false;
  if(!opened)return true;
  if(cleanup_pending)return false;
  /* Runtime yield may discover a failed native operation immediately before
   * an alarm safe point. Check now, not on the UI's slower status cadence. */
  last_status=wa->millis()-250u;wifi_tick();
  return !cleanup_pending;
}
#ifdef PORTABLE_TOUCH_SCROLL
static void wifi_make_view_data(void) {
#else
static void wifi_make_view(void) {
#endif
  memset(&view,0,sizeof(view));view.selected=choice;view.message=wifi_message;view.detail=status_text;
  if(page==WP_SSID || page==WP_PASSWORD) {
    view.title=page==WP_SSID?"NETWORK NAME":"PASSWORD";view.keyboard=true;view.key_page=key_page;
    size_t n=wifi_length(editor,sizeof(editor));
    if(page==WP_PASSWORD){memset(masked,'*',n);masked[n]=0;view.entry=masked;}
    else view.entry=editor;
    /* Show the tail so length-limited fields can still be edited on a watch. */
    if(n>25)view.entry+=n-25;
    view.message=page==WP_SSID?"32 bytes max; < cancels":"63 chars max; < cancels";
    return;
  }
  if(page==WP_HELP) {
    view.title="SESSION AND PRIVACY";
#ifdef PORTABLE_WIFI_PROFILES
    view.detail="Saved networks; explicit actions";
#else
    view.detail="One saved network; explicit actions";
#endif
    view.count=4;
    view.rows[0]="Foreground session";view.values[0]="Back or idle sleep disconnects";
    view.rows[1]="Reconnect manually";view.values[1]="No automatic background connection";
    view.rows[2]="Plaintext storage";view.values[2]="No encryption or secret-vault guarantee";
    view.rows[3]="Forget is logical";view.values[3]="Old physical flash copies may remain";return;
  }
  if(page==WP_FORGET) {
    view.title="FORGET NETWORK?";view.detail="Remove this app's saved credentials";
    view.rows[0]="Cancel";view.values[0]="Keep the saved network";
    view.rows[1]="Forget";view.values[1]="Stops connection and clears draft";view.count=2;return;
  }
#ifdef PORTABLE_WIFI_PROFILES
  if(page==WP_SAVED){
    view.title="SAVED NETWORKS";view.count=saved_rows+2;
    for(unsigned i=0;i<saved_rows;i++){
      view.rows[i]=i>=saved_loading?"Loading...":saved_results[i]==PORTABLE_WIFI_CREDENTIALS_LOADED?saved_names[i]:saved_results[i]==PORTABLE_WIFI_CREDENTIALS_EMPTY?"Empty slot":"Unreadable saved network";
      view.values[i]=i>=saved_loading?"":saved_results[i]==PORTABLE_WIFI_CREDENTIALS_LOADED?(saved_base+i==0?"Default for network tools":"Select to connect or edit"):saved_results[i]==PORTABLE_WIFI_CREDENTIALS_EMPTY?"Save a new network here":"Select to confirm Forget";
    }
    view.rows[saved_rows]="Previous page";view.values[saved_rows]=saved_base?"Earlier saved networks":"First page";
    view.rows[saved_rows+1]="Next page";view.values[saved_rows+1]=saved_count-saved_base>PORTABLE_WIFI_PROFILE_PAGE?"More saved networks":"Last page";return;
  }
#endif
  if(page==WP_SCAN) {
    view.title="AVAILABLE NETWORKS";
    for(unsigned i=0;i<scan_result.count;++i) {
      const garden_radio_scan_entry_v1 *e=&scan_result.entries[i];
      view.rows[i]=e->ssid[0]?e->ssid:"Hidden network";
      const char *auth=e->auth==GARDEN_RADIO_AUTH_OPEN?"Open":e->auth>=1 && e->auth<=5?"Password":"Unsupported";
      snprintf(scan_values[i],sizeof(scan_values[i]),"%s  %d dBm  Ch %u",auth,e->rssi,e->channel);
      view.values[i]=scan_values[i];
    }
    view.count=scan_result.count+1;view.rows[scan_result.count]=scanning || cleanup_pending?"Cancel scan":"Scan again";
    view.values[scan_result.count]="Or Back to enter manually";return;
  }
  view.title="WI-FI SETTINGS";view.count=10;
  view.rows[0]="Network name";view.values[0]=credentials.ssid[0]?credentials.ssid:"Enter manually or scan";
  view.rows[1]="Password";view.values[1]=credentials.password[0]?"********":"Not entered";
  view.rows[2]="Security";view.values[2]=open_network?"Open network (no password)":"Password protected";
  view.rows[3]="Available networks";view.values[3]="Scan nearby access points";
  view.rows[4]=joining?"Cancel connection":"Connect / retry";view.values[4]=status_text;
  view.rows[5]="Disconnect";view.values[5]=cleanup_pending?"Cleanup unconfirmed - retry":"Stops Wi-Fi until you connect";
  view.rows[6]="Save credentials";view.values[6]=draft_dirty?"Draft not saved":saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"Saved; save again after editing":saved_state==PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED?"Save unconfirmed":"Explicit Save only";
  view.rows[7]="Forget saved network";view.values[7]="Confirm before removing";
  view.rows[8]="Connection status";view.values[8]=ip_text[0]?ip_text:PORTABLE_WIFI_VERSION;
  view.rows[9]="Session and privacy";view.values[9]="Back/sleep disconnect; plaintext Save";
#ifdef PORTABLE_WIFI_PROFILES
  view.count=11;view.rows[7]="Forget selected network";view.rows[10]="Saved networks";view.values[10]="Choose or manage remembered networks";
#endif
}
#ifdef PORTABLE_TOUCH_SCROLL
static void wifi_make_view(void) {
  wifi_make_view_data();
  view.identity=wifi_view_identity;portable_wifi_scroll_prepare(&view);
}
#endif
static void wifi_key(unsigned key) {
  size_t n=wifi_length(editor,sizeof(editor));
  if(key<PORTABLE_WIFI_KEYS_PER_PAGE) {
    unsigned ch=portable_watch_key_character(key_page,key),limit=page==WP_SSID?32:63;
    if(ch && n<limit){editor[n]=(char)ch;editor[n+1]=0;}
  } else if(key==PWK_PAGE){WIFI_SCROLL_CANCEL();WIFI_NEW_VIEW();key_page=(key_page+1)%PWK_PAGES;choice=0;}
  else if(key==PWK_DELETE){if(n)editor[n-1]=0;}
  else if(key==PWK_DONE) {
    char *dst=page==WP_SSID?credentials.ssid:credentials.password;
    size_t capacity=page==WP_SSID?sizeof(credentials.ssid):sizeof(credentials.password);
    wifi_zero(dst,capacity);memcpy(dst,editor,n);draft_dirty=true;
    if(page==WP_PASSWORD && n)open_network=false;
    wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));wifi_message="Draft changed; Connect or Save";wifi_page(WP_ROOT);
  }
  dirty=true;
}
static void wifi_forget(void) {
  /* Failed native ownership forbids even our own KV access. Drain first; a
   * refused cleanup leaves the saved profile and editor available for retry. */
  if(!wifi_disconnect()){
#ifdef PORTABLE_WIFI_PROFILES
    forget_pending=portable_wifi_stop_pending();
#endif
    return;
  }
#ifdef PORTABLE_WIFI_PROFILES
  if(!forget_index_valid){forget_pending=false;wifi_message="Choose a saved network first";wifi_page(WP_ROOT);return;}
  int result=portable_wifi_profile_forget(wifi_store,forget_index);
  if(result==PORTABLE_WIFI_CREDENTIALS_AGAIN){forget_pending=true;wifi_message="Storage busy - Back cancels Forget";dirty=true;return;}
  forget_index_valid=forget_pending=false;
#else
  int result=portable_wifi_credentials_forget(wifi_store);
#endif
#ifdef PORTABLE_BLE_BROADCAST
  if(portable_adapter_retained())return;
#endif
  saved_state=result;draft_dirty=false;portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));open_network=false;
  wifi_message=result==PORTABLE_WIFI_CREDENTIALS_EMPTY?"Forgotten; storage is not a secure vault":
    result==PORTABLE_WIFI_CREDENTIALS_CLEANUP_REMANENCE?"Forgotten; old storage bytes may remain":"Forget unconfirmed - retry";
  wifi_page(WP_ROOT);
}
static void wifi_select_entry(const garden_radio_scan_entry_v1 *entry) {
  portable_wifi_credentials_clear(&credentials);memcpy(credentials.ssid,entry->ssid,sizeof(entry->ssid));draft_dirty=true;
  open_network=entry->auth==GARDEN_RADIO_AUTH_OPEN;wifi_message="Network selected; enter password";
#ifdef PORTABLE_WIFI_PROFILES
  wifi_page(WP_ROOT);wifi_profile_start(WIFI_PROFILE_SCAN);
#else
  if(open_network)wifi_page(WP_ROOT);else wifi_edit(WP_PASSWORD);
#endif
}
#ifdef PORTABLE_WIFI_PROFILES
static void wifi_saved_select(uint32_t slot) {
  portable_wifi_credentials stored={{0},{0}};
  saved_select_pending=false;
  int rc=portable_wifi_profile_load(wifi_store,slot,&stored);if(PORTABLE_APP_RETAINED()){portable_wifi_credentials_clear(&stored);return;}
  if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED){credentials=stored;saved_state=rc;draft_dirty=false;open_network=!credentials.password[0];wifi_message="Saved network selected; Connect when ready";wifi_page(WP_ROOT);}
  else if(rc==PORTABLE_WIFI_CREDENTIALS_EMPTY){portable_wifi_credentials_clear(&credentials);draft_dirty=false;open_network=false;wifi_edit(WP_SSID);}
  else if(rc==PORTABLE_WIFI_CREDENTIALS_AGAIN){saved_select_pending=true;saved_pending_slot=slot;wifi_message="Loading selection - Back cancels";dirty=true;}
  else {forget_index=slot;forget_index_valid=true;wifi_page(WP_FORGET);wifi_message="Unreadable slot; confirm before forgetting";}
  portable_wifi_credentials_clear(&stored);return;
}
#endif
static void wifi_activate(unsigned selected) {
#ifdef PORTABLE_WIFI_PROFILES
  if(profile_work||saved_open_pending||saved_select_pending||forget_pending){wifi_message="Checking saved networks - Back cancels";dirty=true;return;}
  if(page==WP_SAVED){
    if(selected==saved_rows){if(saved_base)wifi_saved_open(saved_base>=PORTABLE_WIFI_PROFILE_PAGE?saved_base-PORTABLE_WIFI_PROFILE_PAGE:0);return;}
    if(selected==saved_rows+1){if(saved_count-saved_base>PORTABLE_WIFI_PROFILE_PAGE)wifi_saved_open(saved_base+PORTABLE_WIFI_PROFILE_PAGE);return;}
    if(selected>=saved_loading)return;
    wifi_saved_select(saved_base+selected);return;
  }
#endif
  if(page==WP_SSID || page==WP_PASSWORD){wifi_key(selected);return;}
  if(page==WP_HELP)return;
  if(page==WP_FORGET){if(selected==1)wifi_forget();else wifi_page(WP_ROOT);return;}
  if(page==WP_SCAN) {
    if(selected>=scan_result.count) {
      if(scanning || cleanup_pending){if(wifi_cancel_scan()){wifi_message="Scan cancelled";wifi_page(WP_ROOT);}}
      else wifi_scan_start();
      return;
    }
    garden_radio_scan_entry_v1 entry=scan_result.entries[selected];
    if(!entry.ssid[0]){wifi_message="Hidden network: enter name manually";dirty=true;wifi_zero(&entry,sizeof(entry));return;}
    if(entry.auth>GARDEN_RADIO_AUTH_WPA2_WPA3_PSK){wifi_message="Unsupported network security";dirty=true;wifi_zero(&entry,sizeof(entry));return;}
    if(!wifi_cancel_scan()){
#ifdef WIFI_ASYNC_V1_SIZE
      if(portable_wifi_stop_pending()){wifi_pending_entry=entry;wifi_select_pending=true;}
#endif
      wifi_zero(&entry,sizeof(entry));return;
    }
    wifi_select_entry(&entry);
    wifi_zero(&entry,sizeof(entry));return;
  }
  switch(selected) {
  case 0:wifi_edit(WP_SSID);break;
  case 1:wifi_edit(WP_PASSWORD);break;
  case 2:draft_dirty=true;open_network=!open_network;if(open_network)wifi_zero(credentials.password,sizeof(credentials.password));wifi_message="Security changed; Connect or Save";dirty=true;break;
  case 3:wifi_scan_start();break;
  case 4:if(joining){if(wifi_disconnect())wifi_message="Connection cancelled";}else wifi_connect();break;
  case 5:if(wifi_disconnect())wifi_message="Disconnected";break;
  case 6:
    if(!portable_wifi_services_safe()){wifi_message="Disconnect before retrying Save";dirty=true;break;}
    if(wifi_draft_valid()) {
#ifdef PORTABLE_WIFI_PROFILES
      wifi_profile_start(WIFI_PROFILE_SAVE);break;
#else
      saved_state=portable_wifi_credentials_save(wifi_store,&credentials);
      if(PORTABLE_APP_RETAINED())return;
#ifdef PORTABLE_BLE_BROADCAST
      if(portable_adapter_retained())return;
#endif
      if(saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED)draft_dirty=false;
      wifi_message=saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"Saved; storage is not a secure vault":"Save unconfirmed - retry";
#endif
    }
    dirty=true;break;
  case 7:
#ifdef PORTABLE_WIFI_PROFILES
    wifi_profile_start(WIFI_PROFILE_FORGET);
#else
    wifi_page(WP_FORGET);
#endif
    break;
#ifdef PORTABLE_WIFI_PROFILES
  case 10:wifi_saved_open(0);break;
#endif
  case 9:wifi_page(WP_HELP);break;
  default:dirty=true;break;
  }
}
static bool wifi_back(void) {
  WIFI_SCROLL_CANCEL();
#ifdef WIFI_ASYNC_V1_SIZE
  wifi_select_pending=false;wifi_zero(&wifi_next_request,sizeof(wifi_next_request));wifi_zero(&wifi_pending_entry,sizeof(wifi_pending_entry));
#endif
#ifdef PORTABLE_WIFI_PROFILES
  bool storage_pending=saved_open_pending||saved_select_pending||forget_pending;
  saved_loading_active=saved_open_pending=saved_select_pending=forget_pending=false;
  if(storage_pending){wifi_message="Saved-network action cancelled";dirty=true;return false;}
  if(profile_work){profile_work=0;portable_wifi_credentials_clear(&profile_draft);wifi_zero(&profile_search,sizeof(profile_search));wifi_message="Saved-network action cancelled";dirty=true;return false;}
#endif
  if(page==WP_SCAN && (scan_owned || cleanup_pending) && !wifi_cancel_scan()){wifi_scan_back=portable_wifi_stop_pending();return false;}
  if(page!=WP_ROOT) {
    wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));wifi_page(WP_ROOT);return false;
  }
  if(!portable_wifi_close()){wifi_back_return=portable_wifi_stop_pending();wifi_message=portable_wifi_stop_pending()?"Stopping Wi-Fi - Back will return after cleanup":"Cleanup failed - retry Back";dirty=true;return false;}
#ifdef WIFI_RETURN_APP
  if(!wr->request_launch(WIFI_RETURN_APP)) {
    /* No credentials or live private grants remain. Do not pretend handoff worked. */
    wr->diagnostic("WIFI return-request-failed");
  }
#endif
  return true;
}
static bool wifi_open(void) {
 if(PORTABLE_APP_RETAINED())return false;
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 if(opened)return true;
#endif
  wifi_back_return=wifi_scan_back=false;wifi_service_lease=wifi_service_depth=0;
  wa=t5_app_get_api(1);wr=PORTABLE_APP_RUNTIME();
  if(!wa || !wa->poll || !wa->millis || !wa->screen_width || !wa->screen_height ||
     wa->screen_width()<240 || wa->screen_height()<240 || !wr || wr->api_version!=1 ||
     wr->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !wr->acquire || !wr->release || !wr->request_launch || !wr->diagnostic || !wr->yield_ms)return false;
  memset(&wg,0,sizeof(wg));memset(&wk,0,sizeof(wk));wifi=NULL;wifi_store=NULL;
#ifdef WIFI_ASYNC_V1_SIZE
  wifi_async=NULL;wifi_operation=wifi_operation_kind=0;wifi_stopping=wifi_select_pending=false;wifi_zero(&wifi_next_request,sizeof(wifi_next_request));wifi_zero(&wifi_pending_entry,sizeof(wifi_pending_entry));
#endif
  portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));
#ifdef PORTABLE_WIFI_PROFILES
  profile_work=0;saved_loading_active=forget_index_valid=saved_open_pending=saved_select_pending=forget_pending=false;portable_wifi_credentials_clear(&profile_draft);memset(saved_names,0,sizeof(saved_names));
#endif
  memset(&scan_result,0,sizeof(scan_result));opened=true;scanning=scan_owned=joining=cleanup_pending=draft_dirty=false;
#ifdef PORTABLE_TOUCH_SCROLL
  wifi_view_identity=1;portable_wifi_scroll_reset();
#endif
#ifdef PORTABLE_TEXT_INPUT_CLIENT
  wifi_text=(portable_text_client){0};wifi_text_state=(risc_text_entry_state_v1){0};wifi_text_active=wifi_text_closing=wifi_home_return=false;
#endif
  dirty=true;armed=false;page=WP_ROOT;choice=key_page=0;link_state=WIFI_LINK_DOWN;
  strcpy(status_text,"Disconnected");ip_text[0]=0;last_status=wa->millis();
#ifdef PORTABLE_BLE_BROADCAST
  if(!portable_broadcast_stop())return false;
#endif
  wk.struct_size=sizeof(wk);
  if(wr->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_WIFI_STORAGE_INSTANCE,&wk)) {
    const risc_key_value_v1 *kv=wk.api;
    if(kv && kv->api_version==1 && kv->struct_size>=sizeof(*kv) && kv->get && kv->put)wifi_store=kv;
  }
#if defined(PORTABLE_BLE_BROADCAST) && !defined(PORTABLE_NATIVE_TIME_TOOLBAR)
  if(wifi_store){wifi_store_source=wifi_store;wifi_store_checked=*wifi_store;wifi_store_checked.get=wifi_store_get;wifi_store_checked.put=wifi_store_put;wifi_store=&wifi_store_checked;}
#endif
  if(!wifi_store){PORTABLE_APP_RETAIN();if(PORTABLE_APP_RETAINED())return false;}
  saved_state=portable_wifi_credentials_load(wifi_store,&credentials);
  if(PORTABLE_APP_RETAINED())return false;
#ifdef PORTABLE_BLE_BROADCAST
  if(portable_adapter_retained())return false;
#endif
  open_network=saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED && !credentials.password[0];
  portable_stage_log(wr,"wifi-saved-profile",saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"result=loaded connect=explicit":
      saved_state==PORTABLE_WIFI_CREDENTIALS_EMPTY?"result=empty connect=explicit":saved_state==PORTABLE_WIFI_CREDENTIALS_INVALID?"result=invalid":"result=unavailable");
  wifi_message=saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"Saved draft loaded; Connect when ready":
      saved_state==PORTABLE_WIFI_CREDENTIALS_EMPTY?"Enter a network or scan":
      saved_state==PORTABLE_WIFI_CREDENTIALS_INVALID?"Saved data invalid; enter a network":"Storage unavailable; temporary connection";
  if(!wifi_acquire())wifi_message="Wi-Fi unavailable; retry Connect";
  return !PORTABLE_APP_RETAINED();
}
/* A failed presenter cannot resume normal UI, but healthy worker cleanup is
 * still cooperative progress. Poll it until quiescent before the exit barrier. */
static bool wifi_failure_drain(void) {
  bool done=portable_wifi_suspend();
  while(!done&&portable_wifi_stop_pending()) {
    wr->yield_ms(1);if(PORTABLE_APP_RETAINED())return false;
    (void)portable_wifi_services_safe();if(PORTABLE_APP_RETAINED())return false;
    done=portable_wifi_suspend();
  }
  return done;
}
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
static void wifi_controller_main(void) {
#else
void app_main(void) {
#endif
  if(!wifi_open())return;
  for(;;) {
 if(PORTABLE_APP_RETAINED())return;
#ifdef PORTABLE_TEXT_INPUT_CLIENT
    if(wifi_text.retained||portable_text_adapter_retained())return;
    if(wifi_text_active){
      wifi_tick();if(PORTABLE_APP_RETAINED())return;
      wifi_text_step();if(wifi_text.retained||PORTABLE_APP_RETAINED())return;
      wr->yield_ms(1);continue;
    }
    if(wifi_home_return&&portable_wifi_close()){
#ifdef PORTABLE_HOME_APP
      (void)wr->request_launch(PORTABLE_HOME_APP);
#endif
      return;
    }
    if(PORTABLE_APP_RETAINED())return;
#endif
    if(wifi_back_return){
      wifi_tick();if(PORTABLE_APP_RETAINED())return;
      if(wifi_back())return;
    }
    if(dirty){
      wifi_make_view();
      if(!portable_wifi_paper() || paper_frame_ready()){portable_wifi_render(&view);dirty=false;}
    }
    t5_app_input_t input;
    if(!wa->poll(&input,25)) {
#ifdef PORTABLE_ALARM_CLIENT
      if(portable_app_sleep_retained())return; /* No I/O across native retention. */
#endif
      /* The Runtime exit barrier precedes fini. Standalone/non-alarm builds
       * must also drain healthy RF before returning after any ordinary error. */
      if(!wifi_failure_drain()) {
        if(PORTABLE_APP_RETAINED())return;
        wr->diagnostic("WIFI poll-failure cleanup-unconfirmed; invocation retained");
        for(;;)wr->yield_ms(50);
      }
      return;
    }
    /* Physical Home already closed this session and queued its root target.
     * Never turn that terminal handoff into nested Back or redraw the old app. */
    if(input.exit_requested && !opened)return;
    wifi_tick();
    if(PORTABLE_APP_RETAINED())return;
#ifdef PORTABLE_WIFI_PROFILES
    wifi_profiles_step();if(PORTABLE_APP_RETAINED())return;
#ifdef PORTABLE_TEXT_INPUT_CLIENT
    if(wifi_text_active)continue;
#endif
#endif
#ifdef PORTABLE_TOUCH_SCROLL
    dirty|=portable_wifi_scroll_changed();
#endif
    if(!input.buttons && !input.tapped && !input.exit_requested){armed=true;continue;}
    if(!armed)continue;
    armed=false;
    if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK)){if(wifi_back())return;continue;}
    wifi_make_view();
    if(input.tapped && portable_wifi_paper()) {
      int hit=portable_wifi_hit(&view,input.touch_x,input.touch_y);
      input.tapped=false;
      if(hit==WIFI_HIT_BACK){if(wifi_back())return;continue;}
      if(hit==WIFI_HIT_PREVIOUS)input.buttons=T5_APP_BUTTON_UP;
      else if(hit==WIFI_HIT_NEXT)input.buttons=T5_APP_BUTTON_DOWN;
      else {if(hit>=0){choice=(unsigned)hit;wifi_activate(choice);}continue;}
    }
    if(input.tapped) {
      int x=input.touch_x-(wa->screen_width()-240)/2,y=input.touch_y-(wa->screen_height()-240)/2;
#ifdef PORTABLE_NOVA_UI
      if(!view.keyboard && y>=188 && y<232) {
        if(x>=16 && x<80)input.buttons=T5_APP_BUTTON_UP;
        else if(x>=88 && x<152)input.buttons=T5_APP_BUTTON_CONFIRM;
        else if(x>=160 && x<224)input.buttons=T5_APP_BUTTON_DOWN;
#else
      if(!view.keyboard && y>=212 && y<238) {
        if(x>=12 && x<76)input.buttons=T5_APP_BUTTON_UP;
        else if(x>=88 && x<152)input.buttons=T5_APP_BUTTON_CONFIRM;
        else if(x>=164 && x<228)input.buttons=T5_APP_BUTTON_DOWN;
#endif
      } else {
        int hit=portable_wifi_hit(&view,input.touch_x,input.touch_y);
        if(hit>=0){choice=(unsigned)hit;wifi_activate(choice);}continue;
      }
    }
    unsigned count=view.keyboard?PORTABLE_WIFI_KEY_COUNT:view.count;
    if(!count)continue;
    if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))choice=choice?choice-1:count-1;
    else if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))choice=(choice+1)%count;
    else if(input.buttons&T5_APP_BUTTON_CONFIRM){
#ifdef PORTABLE_TOUCH_SCROLL
      if(!portable_wifi_scroll_confirm(&view)){dirty=true;continue;}
#endif
      wifi_activate(choice);
    }
    dirty=true;
  }
}
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
void app_main(void) {
  wifi_controller_main();
  (void)paper_frame_drain();
}
#endif
