/* Device-independent returning suspension. Legacy/X4 paths remain unchanged. */
#include "PortableForegroundIdle.h"
#include "RiscRuntimeCheckpointV1.h"
#ifdef PORTABLE_FOREGROUND_ALARM_SLEEP
#include "foreground_alarm.inc"
#endif
static int foreground_idle_result;
static bool foreground_idle_configured,foreground_idle_retry_pending;
static uint32_t foreground_idle_retry_at,foreground_idle_retry_delay;
/* Transport recovery only: do not rewrite the selected timeout or activity. */
#define FOREGROUND_IDLE_RETRY_MIN_MS 250u
#define FOREGROUND_IDLE_RETRY_MAX_MS 4000u
static bool foreground_idle_live(void) {
 if(native_custody_retained || failed)return false;
 if(!risc_runtime_get_api(1)){portable_adapter_retain_silent();return false;}
 return true;
}
static bool foreground_idle_ready(bool modal) {
 if(!foreground_idle_live() || !portable_paper_frame_idle() || handoff_requested ||
    home_pending || crown_pending || navigation_pending || idle_capture_active())return false;
 if(risc_runtime_exit_requested_v1(rt) || !foreground_idle_live())return false;
#ifdef PORTABLE_FOREGROUND_IDLE_WORK_GUARD
 if(portable_app_foreground_idle_inhibited())return false;
#endif
 if(!modal) {
  if(touch.down || touch.home_down || (touch.subscription &&
     (!touch.neutral || !touch.home_neutral || touch.resync || touch.cancel_pending)) ||
     (input_pending && (input_sample.down || input_sample.began || input_sample.released || input_sample.cancelled)))return false;
#ifdef PORTABLE_INPUT_NAVIGATION
  if(navigation_ready && !navigation_neutral)return false;
#endif
 }
#ifdef PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL
 if(settings_atomic_owned())return false;
#endif
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
 if(quick_atomic_owned() || home_automatic_input_busy())return false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
 if(!display_settled || alarm_modal || portable_alarm_owned(&alarms))return false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_modal || pqa_visible(&quick.ui) || pqa_capture(&quick.ui) ||
    quick_launch_pending || quick_restore_pending)return false;
#endif
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 if(handoff_active || handoff_pending)return false;
#endif
#ifdef PORTABLE_INSTALLED_CATALOG
 if(installed_catalog_client.volume_grant.api || installed_catalog_client.closing)return false;
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
 if(!portable_file_browser_safe())return false;
#endif
#if defined(PORTABLE_WIFI_SESSION_APP) || defined(PORTABLE_UPDATE_APP)
 bool wifi_safe=portable_wifi_services_safe();
 if(!foreground_idle_live() || !wifi_safe || !portable_wifi_idle_ready())return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
 bool radio_safe=portable_radio_services_safe();
 if(!foreground_idle_live() || !radio_safe)return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
 bool audio_safe=portable_audio_services_safe();
 if(!foreground_idle_live() || !audio_safe)return false;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 /* Do not silently stop a configured advertiser to make the app eligible. */
 if(!broadcast_client.loaded || (broadcast_client.policy.enabled &&
    broadcast_client.policy.radios_allowed))return false;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(contexts_client.loaded && contexts_client.policy.enabled &&
    (contexts_client.policy.audio_allowed || contexts_client.policy.radio_allowed))return false;
#endif
 return foreground_idle_live();
}
static bool foreground_idle_configure_once(uint32_t now) {
 if(!foreground_idle_live())return false;
 int status=portable_foreground_idle_configure(rt);
 if(status==-2){portable_adapter_retain_silent();return false;}
 if(!foreground_idle_live())return false;
 foreground_idle_configured=status==0;
 foreground_idle_retry_pending=status==-3;
 if(foreground_idle_retry_pending) {
  /* Start recovery delay after the read, even if the failed I/O was slow. */
  now=millis_now();if(!foreground_idle_live())return false;
  uint32_t delay=foreground_idle_retry_delay?foreground_idle_retry_delay*2u:FOREGROUND_IDLE_RETRY_MIN_MS;
  foreground_idle_retry_delay=delay>FOREGROUND_IDLE_RETRY_MAX_MS?FOREGROUND_IDLE_RETRY_MAX_MS:delay;
  foreground_idle_retry_at=now+foreground_idle_retry_delay;
 }else foreground_idle_retry_delay=0;
 return true;
}
static bool foreground_idle_retry_due(uint32_t now) {
 return foreground_idle_retry_pending && (int32_t)(now-foreground_idle_retry_at)>=0;
}
static bool foreground_idle_retry(uint32_t now,bool modal) {
 if(!foreground_idle_retry_due(now))return true;
 if(!foreground_idle_ready(modal))return foreground_idle_live();
 int32_t serviced=risc_runtime_service_checkpoint_v1(rt);
 if(!foreground_idle_live())return false;
 if(serviced==RISC_RUNTIME_CHECKPOINT_BUSY)return true;
 if(serviced!=RISC_RUNTIME_CHECKPOINT_OK){portable_adapter_retain_silent();return false;}
 if(!foreground_idle_ready(modal))return foreground_idle_live();
 return foreground_idle_configure_once(now);
}
static inline void foreground_idle_confirmed(void) {
 foreground_idle_configured=true;foreground_idle_retry_pending=false;
 foreground_idle_retry_delay=0;
}
static bool foreground_idle_due(uint32_t now,bool modal) {
 const uint32_t interval=portable_foreground_idle_timeout_ms();
 return foreground_idle_configured && interval && interval<=INT32_MAX &&
     (uint32_t)(now-last_activity)>=interval && foreground_idle_ready(modal);
}
static bool foreground_idle_navigation(bool active) {
#ifdef PORTABLE_INPUT_NAVIGATION
 if(navigation_ready) {
  const risc_input_foreground_v1 claims[]={{"input.touch.raw",1}};
  bool ok=navigation->foreground(navigation->context,active?claims:NULL,active?1:0);
  if(!foreground_idle_live())return false;
  if(!ok){portable_adapter_retain_silent();return false;}
  ok=navigation->reset(navigation->context);
  if(!foreground_idle_live())return false;
  if(!ok){portable_adapter_retain_silent();return false;}
  navigation_neutral=false;
 }
#else
 (void)active;
#endif
 return true;
}
static bool foreground_idle_touch_close(void) {
 if(touch.subscription) {
  bool ok=touch.api->unsubscribe(touch.api->context,touch.subscription);
  if(!foreground_idle_live())return false;
  if(!ok){portable_adapter_retain_silent();return false;}
  touch.subscription=0;
 }
 if(touch.grant.api) {
  risc_runtime_capability_v1 released=touch.grant;
  bool ok=rt->release(&released);
  if(!foreground_idle_live())return false;
  if(!ok || released.api || released.slot || released.generation) {
   portable_adapter_retain_silent();return false;
  }
 }
 touch=(portable_touch){0};return true;
}
static bool foreground_idle_touch_open(void) {
 touch=(portable_touch){.grant={.struct_size=sizeof(touch.grant)}};
 bool ok=rt->acquire("input.touch.raw",1,0,&touch.grant);
 if(!foreground_idle_live())return false;
 if(!ok || !touch.grant.api || !touch.grant.slot || !touch.grant.generation)goto retained;
 touch.api=touch.grant.api;
 if(touch.api->api_version!=1 || touch.api->struct_size<sizeof(*touch.api) ||
    !touch.api->subscribe || !touch.api->unsubscribe || !touch.api->poll ||
    !touch.api->next || !touch.api->snapshot)goto retained;
 touch.subscription=touch.api->subscribe(touch.api->context);
 if(!foreground_idle_live())return false;
 if(!touch.subscription)goto retained;
 risc_touch_snapshot_v1 snapshot={0};
 ok=touch.api->snapshot(touch.api->context,&snapshot);
 if(!foreground_idle_live())return false;
 if(!ok || !portable_touch_snapshot_valid(&snapshot))goto retained;
 /* Same boundary semantics as portable_touch_sync(false), with a liveness
  * check between every provider call. No inherited DOWN becomes an action. */
 touch.initialized=touch.prefix=touch.cancel_pending=true;
 touch.width=snapshot.width;touch.height=snapshot.height;
 touch.sequence=snapshot.sequence;touch.timestamp_ms=snapshot.timestamp_ms;
 touch.contact_count=snapshot.contact_count;
 memcpy(touch.contacts,snapshot.contacts,sizeof(touch.contacts));
 touch.buttons=snapshot.buttons;touch.home_down=!!(snapshot.buttons&RISC_TOUCH_BUTTON_PRIMARY);
 touch.home_neutral=!touch.home_down;touch.neutral=!snapshot.contact_count;
 return true;
retained:
 portable_adapter_retain_silent();return false;
}
static bool foreground_idle_enter(void) {
 const bool modal=
#ifdef PORTABLE_TEXT_INPUT_POLICY
     text_input_suspended;
#else
     false;
#endif
 if(!foreground_idle_ready(modal))return !native_custody_retained && !failed;
 int32_t serviced=risc_runtime_service_checkpoint_v1(rt);
 if(!foreground_idle_live())return false;
 if(serviced==RISC_RUNTIME_CHECKPOINT_BUSY)return true;
 if(serviced!=RISC_RUNTIME_CHECKPOINT_OK){portable_adapter_retain_silent();return false;}
 /* Services may have changed eligibility. Do not borrow input until settled. */
 if(!foreground_idle_ready(modal))return !native_custody_retained && !failed;
#ifdef PORTABLE_NATIVE_SCENE
 native_scene_invalidate();if(failed)return false;
#endif
 if(!modal) {
  if(!foreground_idle_navigation(false))return false;
  if(!foreground_idle_touch_close())return false;
 }
 input_sample=(portable_touch_sample){0};input_pending=false;navigation_pending=0;
 clear_contact_snapshots();
#ifdef PORTABLE_FOREGROUND_ALARM_SLEEP
 const portable_foreground_alarm_hooks hooks={NULL,foreground_alarm_prepare,foreground_alarm_resume};
 foreground_alarm_ticket_valid=false;
 foreground_idle_result=portable_app_foreground_idle_sleep_alarm(rt,display,gauge,&hooks);
 foreground_alarm_ticket_valid=false;
#else
 foreground_idle_result=portable_app_foreground_idle_sleep(rt,display,gauge);
#endif
 /* Preserve both the -1 failure and -2 retention result locally. Neither
  * licenses native teardown of an application with an unsaved model. */
 if(foreground_idle_result!=0 && foreground_idle_result!=1) {
  portable_adapter_retain_silent();return false;
 }
 if(!foreground_idle_live())return false;
 if(!modal) {
  if(!foreground_idle_touch_open() || !foreground_idle_navigation(true))return false;
 }
 input_sample=(portable_touch_sample){0};input_pending=false;navigation_pending=0;
 home_pending=crown_pending=false;clear_contact_snapshots();
 previous_valid=paper_previous_valid=false;
 last_activity=last_poll_at=input_sampled_at=millis_now();
 return foreground_idle_live();
}
