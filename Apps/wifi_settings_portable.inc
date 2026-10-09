#ifdef PORTABLE_QUICK_RADIOS
#include "PortableRadioPolicy.h"
#endif
/* Explicit portable build only. Reader's firmware selector remains in the
 * #else branch of wifi_settings.c. No firmware UI, sockets or network stack. */
#include "T5AppApi.h"
#include "PortableWifiView.h"
#include "PaperFrame.h"
#include "PortableWifiCredentials.h"
#include "RiscRuntimeV1.h"
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

enum { WP_ROOT, WP_SSID, WP_PASSWORD, WP_SCAN, WP_FORGET, WP_HELP };
static const t5_app_api_v1 *wa;
static const risc_runtime_api_v1 *wr;
static risc_runtime_capability_v1 wg, wk;
static const wifi_api_v1 *wifi;
static const risc_key_value_v1 *wifi_store;
static portable_wifi_credentials credentials;
static garden_radio_scan_result_v1 scan_result;
static portable_wifi_view view;
static char editor[64], masked[72], status_text[64], ip_text[48];
static char scan_values[GARDEN_RADIO_SCAN_MAX][40];
static const char *wifi_message;
static unsigned page, choice, key_page;
static bool opened, dirty, armed, open_network, scanning, scan_owned, joining, cleanup_pending, draft_dirty;
static uint32_t operation_started,last_status;
static wifi_link_t link_state;
static int saved_state;
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
  if(wifi)return true;
  if(wg.api)return false; /* Invalid or uncertain grant must be released first. */
  wg=(risc_runtime_capability_v1){.struct_size=sizeof(wg)};
  if(!wr->acquire("net.wifi",1,PORTABLE_WIFI_INSTANCE,&wg))return false;
  if(!wifi_api_valid(wg.api)) {
    if(wr->release(&wg))memset(&wg,0,sizeof(wg));
    return false;
  }
  wifi=wg.api;return true;
}
static bool wifi_disconnect(void) {
  if(!wifi) {cleanup_pending=wg.api!=NULL;return !cleanup_pending;}
  if(!wifi->disconnect_checked(wifi->context)) {
    cleanup_pending=true;strcpy(status_text,"Cleanup required");ip_text[0]=0;wifi_message="Cleanup failed - retry Disconnect";dirty=true;return false;
  }
  scanning=scan_owned=joining=cleanup_pending=false;link_state=WIFI_LINK_DOWN;
  strcpy(status_text,"Disconnected");ip_text[0]=0;
  memset(&scan_result,0,sizeof(scan_result));dirty=true;return true;
}
bool portable_wifi_suspend(void) {
  if(!opened)return true;
  if(wifi && !wifi_disconnect())return false;
  if(wg.api && !wr->release(&wg)) {
    cleanup_pending=true;wifi_message="Release failed - retry Disconnect";dirty=true;return false;
  }
  memset(&wg,0,sizeof(wg));wifi=NULL;cleanup_pending=false;return true;
}
void portable_wifi_resume(void) {
  if(!opened)return;
  wifi_message=wifi_acquire()?"Woke - connect when ready":"Wi-Fi unavailable - retry Connect";
  link_state=WIFI_LINK_DOWN;last_status=wa->millis();armed=false;dirty=true;
}
bool portable_wifi_close(void) {
  if(!opened)return true;
  if(!portable_wifi_suspend())return false;
  if(wk.api && !wr->release(&wk))return false;
  memset(&wk,0,sizeof(wk));wifi_store=NULL;
  portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));
  wifi_zero(&scan_result,sizeof(scan_result));memset(&view,0,sizeof(view));opened=false;return true;
}
static void wifi_page(unsigned next_page) {
  page=next_page;choice=0;armed=false;dirty=true;
}
static void wifi_edit(unsigned field) {
  wifi_zero(editor,sizeof(editor));
  const char *source=field==WP_SSID?credentials.ssid:credentials.password;
  size_t n=wifi_length(source,field==WP_SSID?32:63);memcpy(editor,source,n);
  key_page=PWK_INITIAL_PAGE;
  wifi_page(field); /* Start on lowercase; all printable ASCII is reachable. */
}
static bool wifi_draft_valid(void) {
  if(!credentials.ssid[0]) {wifi_message="Enter a network name";return false;}
  if(!open_network && !credentials.password[0]) {wifi_message="Enter an 8-63 character password";return false;}
  if(!portable_wifi_credentials_validate(&credentials)) {wifi_message="Password must be empty or 8-63 chars";return false;}
  return true;
}
static void wifi_connect(void) {
#ifdef PORTABLE_QUICK_RADIOS
 if(!portable_radio_wifi_allowed(wr)){wifi_message="Wi-Fi is off in Quick Controls";dirty=true;return;}
#endif
  if(!wifi_draft_valid()){dirty=true;return;}
  if(!wifi_acquire()) {wifi_message="Wi-Fi unavailable - retry Connect";dirty=true;return;}
  if(!wifi_disconnect())return;
  if(!wifi->connect(wifi->context,credentials.ssid,credentials.password)) {
    if(wifi_disconnect())wifi_message="Connect failed - retry";
    dirty=true;return;
  }
  joining=true;link_state=WIFI_LINK_JOINING;strcpy(status_text,"Connecting...");operation_started=wa->millis();
  wifi_message="Connecting - select Cancel to stop";dirty=true;
}
static void wifi_scan_start(void) {
#ifdef PORTABLE_QUICK_RADIOS
 if(!portable_radio_wifi_allowed(wr)){wifi_message="Wi-Fi is off in Quick Controls";dirty=true;return;}
#endif
  if(!wifi_acquire()){wifi_message="Wi-Fi unavailable - enter manually";dirty=true;return;}
  if(!wifi_disconnect())return;
  if(!wifi->scan_start(wifi->context)){if(wifi_disconnect())wifi_message="Scan failed - retry or enter manually";dirty=true;return;}
  scan_result=(garden_radio_scan_result_v1){.struct_size=sizeof(scan_result)};
  scanning=scan_owned=true;operation_started=wa->millis();wifi_message="Scanning - Back cancels";wifi_page(WP_SCAN);
}
static bool wifi_scan_valid(const garden_radio_scan_result_v1 *r) {
  if(r->struct_size<sizeof(*r) || r->count>GARDEN_RADIO_SCAN_MAX ||
     r->state>GARDEN_RADIO_SCAN_FAILED || r->reserved)return false;
  for(unsigned i=0;i<r->count;++i)if(r->entries[i].reserved ||
      wifi_length(r->entries[i].ssid,sizeof(r->entries[i].ssid))==sizeof(r->entries[i].ssid))return false;
  return true;
}
static bool wifi_cancel_scan(void) {
  if(!wifi || !wifi->scan_cancel(wifi->context)) {
    cleanup_pending=true;wifi_message="Scan cleanup failed - retry Cancel";dirty=true;return false;
  }
  scanning=scan_owned=false;cleanup_pending=false;return true;
}
static void wifi_tick(void) {
  uint32_t now=wa->millis();
  if((uint32_t)(now-last_status)<250)return;
  last_status=now;
  if(!wifi || cleanup_pending)return;
  if(scan_owned) {
    garden_radio_scan_result_v1 next_scan={.struct_size=sizeof(next_scan)};
    bool ok=wifi->scan_poll(wifi->context,&next_scan) && wifi_scan_valid(&next_scan);
    if(!ok || next_scan.state==GARDEN_RADIO_SCAN_FAILED || (scanning && (uint32_t)(now-operation_started)>=15000u)) {
      if(wifi_cancel_scan())wifi_message=ok?"Scan ended - retry or enter manually":"Scan unavailable - retry";
      dirty=true;
    } else if(next_scan.state==GARDEN_RADIO_SCAN_DONE) {
      bool changed=scanning || memcmp(&scan_result,&next_scan,sizeof(next_scan));
      scan_result=next_scan;scanning=false;wifi_message=scan_result.count?"Choose a network or scan again":"No networks - enter manually";
      dirty|=changed;
    }
    wifi_zero(&next_scan,sizeof(next_scan));
    return;
  }
  wifi_link_t state=wifi->status(wifi->context);
  if(state>WIFI_LINK_UP) {if(wifi_disconnect())wifi_message="Invalid status - retry Connect";dirty=true;return;}
  if(state!=link_state) {
    bool was_joining=joining;
    if(state==WIFI_LINK_DOWN && !wifi_disconnect())return;
    link_state=state;dirty=true;
    if(state==WIFI_LINK_UP){joining=false;wifi_message="Connected; Save stores this network";}
    else if(state==WIFI_LINK_DOWN && was_joining){joining=false;wifi_message="Connection failed - retry Connect";}
  }
  if(joining && (uint32_t)(now-operation_started)>=30000u) {
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
bool portable_wifi_services_safe(void) {
  if(!opened)return true;
  if(cleanup_pending)return false;
  /* Runtime yield may discover a failed native operation immediately before
   * an alarm safe point. Check now, not on the UI's slower status cadence. */
  last_status=wa->millis()-250u;wifi_tick();
  return !cleanup_pending;
}
static void wifi_make_view(void) {
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
    view.title="SESSION AND PRIVACY";view.detail="One saved network; explicit actions";view.count=4;
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
}
static void wifi_key(unsigned key) {
  size_t n=wifi_length(editor,sizeof(editor));
  if(key<PORTABLE_WIFI_KEYS_PER_PAGE) {
    unsigned ch=portable_watch_key_character(key_page,key),limit=page==WP_SSID?32:63;
    if(ch && n<limit){editor[n]=(char)ch;editor[n+1]=0;}
  } else if(key==PWK_PAGE){key_page=(key_page+1)%PWK_PAGES;choice=0;}
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
  if(!wifi_disconnect()){wifi_page(WP_ROOT);return;}
  int result=portable_wifi_credentials_forget(wifi_store);
  saved_state=result;draft_dirty=false;portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));open_network=false;
  wifi_message=result==PORTABLE_WIFI_CREDENTIALS_EMPTY?"Forgotten; storage is not a secure vault":
    result==PORTABLE_WIFI_CREDENTIALS_CLEANUP_REMANENCE?"Forgotten; old storage bytes may remain":"Forget unconfirmed - retry";
  wifi_page(WP_ROOT);
}
static void wifi_activate(unsigned selected) {
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
    if(!wifi_cancel_scan()){wifi_zero(&entry,sizeof(entry));return;}
    portable_wifi_credentials_clear(&credentials);memcpy(credentials.ssid,entry.ssid,sizeof(entry.ssid));draft_dirty=true;
    open_network=entry.auth==GARDEN_RADIO_AUTH_OPEN;wifi_message="Network selected; enter password";
    if(open_network)wifi_page(WP_ROOT);else wifi_edit(WP_PASSWORD);
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
      saved_state=portable_wifi_credentials_save(wifi_store,&credentials);
      if(saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED)draft_dirty=false;
      wifi_message=saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"Saved; storage is not a secure vault":"Save unconfirmed - retry";
    }
    dirty=true;break;
  case 7:wifi_page(WP_FORGET);break;
  case 9:wifi_page(WP_HELP);break;
  default:dirty=true;break;
  }
}
static bool wifi_back(void) {
  if(page==WP_SCAN && (scan_owned || cleanup_pending) && !wifi_cancel_scan())return false;
  if(page!=WP_ROOT) {
    wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));wifi_page(WP_ROOT);return false;
  }
  if(!portable_wifi_close()){wifi_message="Cleanup failed - retry Back";dirty=true;return false;}
#ifdef WIFI_RETURN_APP
  if(!wr->request_launch(WIFI_RETURN_APP)) {
    /* No credentials or live private grants remain. Do not pretend handoff worked. */
    wr->diagnostic("WIFI return-request-failed");
  }
#endif
  return true;
}
static bool wifi_open(void) {
  wa=t5_app_get_api(1);wr=risc_runtime_get_api(1);
  if(!wa || !wa->poll || !wa->millis || !wa->screen_width || !wa->screen_height ||
     wa->screen_width()<240 || wa->screen_height()<240 || !wr || wr->api_version!=1 ||
     wr->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !wr->acquire || !wr->release || !wr->request_launch || !wr->diagnostic || !wr->yield_ms)return false;
  memset(&wg,0,sizeof(wg));memset(&wk,0,sizeof(wk));wifi=NULL;wifi_store=NULL;
  portable_wifi_credentials_clear(&credentials);wifi_zero(editor,sizeof(editor));wifi_zero(masked,sizeof(masked));
  memset(&scan_result,0,sizeof(scan_result));opened=true;scanning=scan_owned=joining=cleanup_pending=draft_dirty=false;
  dirty=true;armed=false;page=WP_ROOT;choice=key_page=0;link_state=WIFI_LINK_DOWN;
  strcpy(status_text,"Disconnected");ip_text[0]=0;last_status=wa->millis();
  wk.struct_size=sizeof(wk);
  if(wr->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_WIFI_STORAGE_INSTANCE,&wk)) {
    const risc_key_value_v1 *kv=wk.api;
    if(kv && kv->api_version==1 && kv->struct_size>=sizeof(*kv) && kv->get && kv->put)wifi_store=kv;
  }
  saved_state=portable_wifi_credentials_load(wifi_store,&credentials);open_network=saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED && !credentials.password[0];
  wifi_message=saved_state==PORTABLE_WIFI_CREDENTIALS_LOADED?"Saved draft loaded; Connect when ready":
      saved_state==PORTABLE_WIFI_CREDENTIALS_EMPTY?"Enter a network or scan":
      saved_state==PORTABLE_WIFI_CREDENTIALS_INVALID?"Saved data invalid; enter a network":"Storage unavailable; temporary connection";
  if(!wifi_acquire())wifi_message="Wi-Fi unavailable; retry Connect";
  return true;
}
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
static void wifi_controller_main(void) {
#else
void app_main(void) {
#endif
  if(!wifi_open())return;
  for(;;) {
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
      if(!portable_wifi_suspend()) {
        wr->diagnostic("WIFI poll-failure cleanup-unconfirmed; invocation retained");
        for(;;)wr->yield_ms(50);
      }
      return;
    }
    /* Physical Home already closed this session and queued its root target.
     * Never turn that terminal handoff into nested Back or redraw the old app. */
    if(input.exit_requested && !opened)return;
    wifi_tick();
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
    else if(input.buttons&T5_APP_BUTTON_CONFIRM)wifi_activate(choice);
    dirty=true;
  }
}
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
void app_main(void) {
  wifi_controller_main();
  (void)paper_frame_drain();
}
#endif
