/* Shared portable controller. Product action authority comes only from the
 * selected service grant. Wi-Fi credentials and foreground radio belong here. */
#include "T5AppApi.h"
#include "PortableWifiView.h"
#include "PortableWifiSavedNetwork.h"
#include "PortableRtcClock.h"
#include "PortableUpdate.h"
#include "SoftwareUpdateV1.h"
#include "RiscRuntimeV1.h"
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#ifdef PORTABLE_RETURN_APP
#error "Update app owns nested Back; use UPDATE_RETURN_APP"
#endif
#ifndef PORTABLE_UPDATE_FIRMWARE
#error "Explicit action kind required"
#endif
#ifndef PORTABLE_WIFI_INSTANCE
#define PORTABLE_WIFI_INSTANCE 0u
#endif
#ifndef PORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS
#error "Deployment must explicitly supply configured RTC UTC offset"
#endif
static const t5_app_api_v1 *ua;
static const risc_runtime_api_v1 *ur;
static risc_runtime_capability_v1 usg,uwg,ukg,ucg;
static const software_update_v1 *us;
static const wifi_api_v1 *uw;
static const risc_key_value_v1 *uk;
static const twatch_rtc_api_v1 *uc;
static software_update_status_v1 ust;
static software_update_row_v1 selected_row;
static portable_wifi_view uv;
static char row_names[16][65],row_values[16][80],progress[64];
static const char *message;
static uint32_t selected,confirm_choice,connect_started;
static bool opened,dirty,armed,connecting,cleanup_pending,confirming,install_after_connect,restart_pending,radio_owned;
static bool update_radio_close(void) {
 if(uw&&radio_owned&&!uw->disconnect_checked(uw->context)){cleanup_pending=true;message="Cleanup failed; retry Back";dirty=true;return false;}
 if(uwg.api&&!ur->release(&uwg)){cleanup_pending=true;message="Wi-Fi release failed; retry Back";dirty=true;return false;}
 memset(&uwg,0,sizeof(uwg));uw=NULL;connecting=false;cleanup_pending=false;radio_owned=false;return true;
}
bool portable_update_suspend(void) {
 if(!opened)return true;
 if(restart_pending){message=ust.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Update activated; restart required";dirty=true;return false;}
 bool service_closed=!us||us->cancel(us->context);
 /* Native checked radio leave refuses while HTTP still owns a session. If
  * HTTP is closed but a bank abort is blocked by uncertain RF, radio cleanup
  * can recover first, then one bank-abort retry can complete safely. */
 if(!update_radio_close())return false;
 if(!service_closed&&us&&!us->cancel(us->context)){cleanup_pending=true;message="Update cleanup failed; retry Back";dirty=true;return false;}
 /* Render the completed cancellation, not the previous progress snapshot. */
 if(us&&!us->status(us->context,&ust)){cleanup_pending=true;message="Service status unavailable";dirty=true;return false;}
 cleanup_pending=false;confirming=false;install_after_connect=false;armed=false;message="Cancelled; check again when ready";dirty=true;return true;
}
void portable_update_resume(void) {if(opened){armed=false;dirty=true;message="Woke; check again when ready";}}
bool portable_update_close(void) {
 if(!opened)return true;if(!portable_update_suspend())return false;
 if(usg.api&&!ur->release(&usg))return false;memset(&usg,0,sizeof(usg));us=NULL;
 if(ukg.api&&!ur->release(&ukg))return false;memset(&ukg,0,sizeof(ukg));uk=NULL;
 if(ucg.api&&!ur->release(&ucg))return false;memset(&ucg,0,sizeof(ucg));uc=NULL;opened=false;return true;
}
bool portable_update_services_safe(void) {
 if(!opened)return true;if(cleanup_pending||restart_pending)return false;
 software_update_status_v1 before=ust;
 if(us&&!us->status(us->context,&ust)){cleanup_pending=true;message="Service status unavailable";dirty=true;return false;}
 if(memcmp(&before,&ust,sizeof(ust)))dirty=true;
 if(ust.state==SOFTWARE_UPDATE_RETAINED){cleanup_pending=true;message="Cleanup required; retry Back";dirty=true;return false;}
 if(uw){
  wifi_link_t state=uw->status(uw->context);wifi_ipv4_v1 ip={0},ap={0};
  bool unhealthy=state>WIFI_LINK_UP||state==WIFI_LINK_DOWN||(!connecting&&state!=WIFI_LINK_UP);
  if(state==WIFI_LINK_UP&&(!uw->addresses(uw->context,&ip,&ap)||(!ip.address[0]&&!ip.address[1]&&!ip.address[2]&&!ip.address[3])))unhealthy=true;
  if(connecting&&(uint32_t)(ua->millis()-connect_started)>=30000u)unhealthy=true;
  if(unhealthy){
   if(!portable_update_suspend())return false;message="Connection lost; retry Check";dirty=true;
  }
 }return true;
}
static bool utc_now(uint64_t *out) {
 twatch_rtc_time_v1 t={0};static const uint8_t md[]={31,28,31,30,31,30,31,31,30,31,30,31};
 if(!uc||!uc->read(uc->context,&t)||t.year<2024||t.year>2099||t.month<1||t.month>12||t.day<1||
 t.day>md[t.month-1]+(t.month==2&&t.year%4==0)||t.hour>23||t.minute>59||t.second>59)return false;
 uint32_t days=0;for(unsigned y=1970;y<t.year;++y)days+=365+(y%4==0&&(y%100!=0||y%400==0));
 for(unsigned m=1;m<t.month;++m)days+=md[m-1]+(m==2&&t.year%4==0);
 int64_t seconds=(int64_t)(days+t.day-1)*86400+t.hour*3600+t.minute*60+t.second-PORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS;
 if(seconds<1704067200LL||seconds>4102444799LL)return false;*out=(uint64_t)seconds;return true;
}
static bool connect_saved(bool install) {
 if(!us||!portable_update_suspend())return false;
 uwg=(risc_runtime_capability_v1){.struct_size=sizeof(uwg)};
 if(!ur->acquire("net.wifi",1,PORTABLE_WIFI_INSTANCE,&uwg)){message="Wi-Fi unavailable; retry Check";dirty=true;return false;}
 const wifi_api_v1 *w=uwg.api;
 if(!w||w->api_version!=1||w->struct_size<WIFI_MANAGEMENT_V1_SIZE||!w->connect||!w->status||!w->addresses||!w->disconnect_checked){
  if(!ur->release(&uwg)){cleanup_pending=true;message="Wi-Fi grant cleanup required";}else memset(&uwg,0,sizeof(uwg));dirty=true;return false;}
 uw=w;install_after_connect=install;
 if(uw->status(uw->context)!=WIFI_LINK_DOWN){message="Wi-Fi busy; try again later";(void)update_radio_close();dirty=true;return false;}
 radio_owned=true;
 int result=portable_wifi_saved_network_connect(uk,uw);
 if(result!=PORTABLE_WIFI_SAVED_NETWORK_STARTED){message="Save a network in Wi-Fi Settings first";if(!update_radio_close())return false;dirty=true;return false;}
 connecting=true;connect_started=ua->millis();message="Connecting with saved Wi-Fi";dirty=true;return true;
}
static void update_tick(void) {
 if(cleanup_pending||restart_pending)return;
 if(connecting){wifi_link_t s=uw->status(uw->context);wifi_ipv4_v1 ip={0},ap={0};
  if(s==WIFI_LINK_UP&&uw->addresses(uw->context,&ip,&ap)&&(ip.address[0]||ip.address[1]||ip.address[2]||ip.address[3])){
   connecting=false;bool ok=false;
   if(install_after_connect){uint64_t seconds=0;if(utc_now(&seconds))ok=us->begin(us->context,selected-1,seconds);}
   else {uint64_t seconds=0;if(utc_now(&seconds))ok=us->refresh(us->context,seconds);else message="Set a valid RTC time before HTTPS";}
   if(!ok){if(portable_update_suspend())message="Check/install unavailable; verify time and retry";}
   dirty=true;
  }else if(s>WIFI_LINK_UP||s==WIFI_LINK_DOWN||(uint32_t)(ua->millis()-connect_started)>=30000u){if(update_radio_close())message="Wi-Fi connection failed; retry Check";dirty=true;}
  return;
 }
 if(!us)return;software_update_status_v1 before=ust;(void)us->step(us->context);
 if(!us->status(us->context,&ust)){cleanup_pending=true;message="Service status unavailable";dirty=true;return;}
 if(memcmp(&before,&ust,sizeof(ust)))dirty=true;
 if(ust.state==SOFTWARE_UPDATE_RETAINED){cleanup_pending=true;message="Cleanup required; retry Back";dirty=true;return;}
 if(ust.state==SOFTWARE_UPDATE_ERROR){if(update_radio_close()){message="Update failed; current install kept";dirty=true;}return;}
 if(ust.state==SOFTWARE_UPDATE_LIST&&uw){if(update_radio_close()){message=ust.count?"Select a release; Check refreshes":"No releases in catalog";dirty=true;}return;}
 if(ust.state==SOFTWARE_UPDATE_READY){
  /* Verified bytes alone are not install success. The radio and HTTP session
   * must be gone before the native bank switch. */
  if(!update_radio_close())return;
  if(!us->activate(us->context)){
   if(!us->status(us->context,&ust)||ust.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN){restart_pending=true;message="Activation uncertain; restart required";armed=false;dirty=true;return;}
   message="Activation failed; retry Check";dirty=true;return;
  }
  restart_pending=true;message="Update activated; restart to use it";armed=false;dirty=true;
 }
}
static void render_update(void) {
 memset(&uv,0,sizeof(uv));uv.title=PORTABLE_UPDATE_FIRMWARE?"FIRMWARE UPDATE":"APP STORE";uv.message=message;
 if(restart_pending){uv.count=1;uv.rows[0]="Restart device";uv.values[0]=ust.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Activation confirmed; restart required";}
 else if(cleanup_pending){uv.count=1;uv.rows[0]="Retry cleanup";uv.values[0]="Resources retained until cleanup succeeds";}
 else if(connecting||ust.state==SOFTWARE_UPDATE_CATALOG||(ust.state>=SOFTWARE_UPDATE_PREPARING&&ust.state<=SOFTWARE_UPDATE_READY)){
  uv.count=1;uv.rows[0]="Cancel operation";
  snprintf(progress,sizeof(progress),"%lu / %lu bytes",(unsigned long)ust.done,(unsigned long)ust.total);uv.values[0]=progress;
  uv.detail=connecting?"Connecting...":ust.state==SOFTWARE_UPDATE_CATALOG?"Checking catalog":ust.state==SOFTWARE_UPDATE_PREPARING?"Preparing update":ust.state==SOFTWARE_UPDATE_DOWNLOAD?"Downloading update":"Verifying update";
 }else if(confirming){uv.count=2;uv.selected=confirm_choice;uv.rows[0]="Cancel";uv.values[0]="Keep current installation";uv.rows[1]="Install and restart";uv.values[1]=selected_row.version;uv.detail=selected_row.id;}
 else {
  uint32_t total=ust.count+1;if(selected>=total)selected=0;uint32_t first=selected/16*16;
  uv.count=total-first>16?16:total-first;uv.selected=selected-first;
  for(unsigned i=0;i<uv.count;++i){uint32_t index=first+i;
   if(!index){uv.rows[i]="Check / retry";uv.values[i]="Watch repository release catalog";continue;}
   software_update_row_v1 r={.struct_size=sizeof(r)};
   if(!us||!us->get(us->context,index-1,&r)){uv.rows[i]="Unavailable";uv.values[i]="Retry Check";continue;}
   snprintf(row_names[i],sizeof(row_names[i]),"%s",r.id);
   const char *state=r.availability==SOFTWARE_UPDATE_AVAILABLE?"Update":r.availability==SOFTWARE_UPDATE_CURRENT?"Current":r.availability==SOFTWARE_UPDATE_USB_ONLY?"USB only":"Unsupported";
   snprintf(row_values[i],sizeof(row_values[i]),"%s  %s",r.version,state);uv.rows[i]=row_names[i];uv.values[i]=row_values[i];
  }
 }
 portable_wifi_render(&uv);dirty=false;
}
static bool open_update(void) {
 ua=t5_app_get_api(1);ur=risc_runtime_get_api(1);
 if(!ua||!ua->poll||!ua->millis||!ua->screen_width||!ua->screen_height||ua->screen_width()<240||ua->screen_height()<240||!ur||ur->api_version!=1||ur->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!ur->acquire||!ur->release||!ur->yield_ms||!ur->diagnostic)return false;
 opened=true;dirty=true;armed=false;connecting=cleanup_pending=confirming=install_after_connect=restart_pending=false;selected=confirm_choice=0;
 memset(&ust,0,sizeof(ust));ust.struct_size=sizeof(ust);message="Saved Wi-Fi; Check to connect";
 usg=(risc_runtime_capability_v1){.struct_size=sizeof(usg)};ukg=(risc_runtime_capability_v1){.struct_size=sizeof(ukg)};ucg=(risc_runtime_capability_v1){.struct_size=sizeof(ucg)};memset(&uwg,0,sizeof(uwg));uw=NULL;us=NULL;uk=NULL;uc=NULL;
 if(ur->acquire(PORTABLE_UPDATE_FIRMWARE?SOFTWARE_UPDATE_FIRMWARE_CAPABILITY:SOFTWARE_UPDATE_APPS_CAPABILITY,1,0,&usg)){
  const software_update_v1 *s=usg.api;if(s&&s->api_version==1&&s->struct_size>=sizeof(*s)&&s->refresh&&s->step&&s->status&&s->get&&s->begin&&s->cancel&&s->activate&&s->restart)us=s;}
 if(ur->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE,&ukg)){
  const risc_key_value_v1 *k=ukg.api;if(k&&k->api_version==1&&k->struct_size>=sizeof(*k)&&k->get)uk=k;}
 if(ur->acquire(TWATCH_RTC_CAPABILITY,2,0,&ucg)){
  const twatch_rtc_api_v1 *c=ucg.api;if(c&&c->api_version==2&&c->struct_size>=sizeof(*c)&&c->read)uc=c;}
 if(!us||!uk||!uc)message="Update service, saved network or RTC unavailable";return true;
}
void app_main(void) {
 if(!open_update())return;
 for(;;){if(dirty)render_update();t5_app_input_t input;
  if(!ua->poll(&input,25)){
#ifdef PORTABLE_ALARM_CLIENT
   if(portable_app_sleep_retained())return;
#endif
   if(!portable_update_suspend()){ur->diagnostic("UPDATE cleanup-unconfirmed; invocation retained");for(;;)ur->yield_ms(50);}return;}
  /* Fresh Cancel/Back owns this frame before any irreversible bank step. In
   * particular, a VERIFYING -> READY result must not overtake pending input. */
  if(!input.buttons&&!input.tapped&&!input.exit_requested){armed=true;update_tick();continue;}
  if(!armed&&!input.exit_requested){update_tick();continue;}armed=false;
  if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK)){
   if(confirming){confirming=false;dirty=true;continue;}
   if(!portable_update_close()){dirty=true;continue;}
#ifdef UPDATE_RETURN_APP
   if(!ur->request_launch(UPDATE_RETURN_APP))ur->diagnostic("UPDATE return-request-failed");
#endif
   return;
  }
  bool activate=false;
  if(input.tapped){int x=input.touch_x-(ua->screen_width()-240)/2,y=input.touch_y-(ua->screen_height()-240)/2;
#ifdef PORTABLE_NOVA_UI
   if(y>=188&&y<232){if(x>=16&&x<80)input.buttons=T5_APP_BUTTON_UP;else if(x>=88&&x<152)input.buttons=T5_APP_BUTTON_CONFIRM;else if(x>=160&&x<224)input.buttons=T5_APP_BUTTON_DOWN;}
#else
   if(y>=212&&y<238){if(x>=12&&x<76)input.buttons=T5_APP_BUTTON_UP;else if(x>=88&&x<152)input.buttons=T5_APP_BUTTON_CONFIRM;else if(x>=164&&x<228)input.buttons=T5_APP_BUTTON_DOWN;}
#endif
   else {int hit=portable_wifi_hit(&uv,input.touch_x,input.touch_y);if(hit>=0){if(confirming)confirm_choice=(uint32_t)hit;else if(!connecting&&ust.state==SOFTWARE_UPDATE_LIST)selected=selected/16*16+(uint32_t)hit;dirty=true;}continue;}
  }
  bool busy=connecting||(ust.state>=SOFTWARE_UPDATE_CATALOG&&ust.state!=SOFTWARE_UPDATE_LIST&&ust.state<=SOFTWARE_UPDATE_READY);
  if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT)){if(confirming)confirm_choice^=1;else if(!busy)selected=selected?selected-1:ust.count;dirty=true;}
  else if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)){if(confirming)confirm_choice^=1;else if(!busy)selected=selected==ust.count?0:selected+1;dirty=true;}
  else if(input.buttons&T5_APP_BUTTON_CONFIRM)activate=true;
  if(!activate){update_tick();continue;}
  if(restart_pending){if(!us->restart(us->context)){message="Restart not accepted; retry Restart";dirty=true;}continue;}
  if(cleanup_pending||busy){(void)portable_update_suspend();dirty=true;continue;}
  if(confirming){bool install=confirm_choice==1;confirming=false;dirty=true;if(install)(void)connect_saved(true);continue;}
  if(!selected){(void)connect_saved(false);continue;}
  selected_row=(software_update_row_v1){.struct_size=sizeof(selected_row)};
  if(us&&us->get(us->context,selected-1,&selected_row)){
   message=selected_row.reason;if(selected_row.availability==SOFTWARE_UPDATE_AVAILABLE){confirming=true;confirm_choice=0;}dirty=true;
  }
 }
}
