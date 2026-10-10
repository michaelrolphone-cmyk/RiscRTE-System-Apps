/* One host owns persisted power policy. A settled child grants a separate
 * POLICY boundary only after dropping focus and its touch subscription. */
#ifdef PORTABLE_RESIDENT_POLICY
#if !defined(PORTABLE_X4_IDLE_POLICY) || !defined(PORTABLE_LOW_BATTERY)
#error Resident host policy requires the typed X4 idle and low-battery profile
#endif
#include "resident_sleep.inc"
static bool resident_policy_requested;
static bool resident_policy_due(uint32_t now) {
 return !low_battery_sampled || (uint32_t)(now-low_battery_sampled_at)>=5000u ||
   (portable_idle_ms()&&(uint32_t)(now-last_activity)>=portable_idle_ms());
}
static int resident_policy_dispatch(const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply) {
 if(failed || surface.frame || paper_token || !display_settled || alarm_modal || quick_modal ||
    resident_launch_pending || resident_sleep_pending)return RISC_RESIDENT_BUSY;
 uint32_t now=millis_now();
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
 if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL) {
  if(request->flags&(RISC_RESIDENT_POLL_ACTIVITY|RISC_RESIDENT_POLL_INHIBIT_IDLE))last_activity=now;
  if(request->flags&RISC_RESIDENT_POLL_INHIBIT_POLICY)resident_policy_requested=false;
  else if(resident_policy_due(now)) {
   resident_policy_requested=true;reply->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;
  }
  return RISC_RESIDENT_OK;
 }
 if(!resident_policy_requested)return RISC_RESIDENT_BUSY;
 resident_dispatching=true;
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_suspend())return RISC_RESIDENT_RETAINED;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 if(!portable_broadcast_stop())return RISC_RESIDENT_RETAINED;
#endif
 /* Settings may have changed in the child. Read the confirmed timer/brightness
  * before applying a crossing or preparing native Light. */
 if(!pqa_session_load(&quick,rt) || native_custody_retained || !low_battery_poll()) {
  if(!native_custody_retained)portable_adapter_retain();
  return RISC_RESIDENT_RETAINED;
 }
 now=millis_now();
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
 if(portable_idle_ms()&&(uint32_t)(now-last_activity)>=portable_idle_ms()) {
  /* This path is reversible Light only. Explicit deep sleep is handled by
   * CHECKPOINT_SLEEP, clean child return, then the host's ordinary sleep path. */
  int status=portable_app_idle_sleep_with_ui(rt,display,gauge,alarm_sleep_api(),&resident_sleep_ui);
  if(status==-2){portable_adapter_retain_silent();return RISC_RESIDENT_RETAINED;}
  if(native_custody_retained)return RISC_RESIDENT_RETAINED;
  if(status<0) {
   /* The helper confirmed cleanup but foreground alarm resume failed. End
    * the child cleanly, then let the host return through ordinary fini. */
   if(resident.request_foreground_exit(resident.invocation)!=RISC_RESIDENT_OK) {
    portable_adapter_retain();return RISC_RESIDENT_RETAINED;
   }
   failed=true;resident_policy_requested=false;resident_dispatching=false;
   return RISC_RESIDENT_EXIT;
  }
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(quick.ui.paper && !pqa_session_sync_tone(&quick,display)) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
   /* A refused provider operation may already own terminal custody. */
   portable_adapter_retain_silent();
#else
   portable_adapter_retain();
#endif
   return RISC_RESIDENT_RETAINED;
  }
#endif
  if(!pqa_radios_resume(&quick_radios,&quick.ui,rt)) {
   if(!native_custody_retained)portable_adapter_retain();
   return RISC_RESIDENT_RETAINED;
  }
  last_activity=millis_now();
  if(native_custody_retained)return RISC_RESIDENT_RETAINED;
  paper_previous_valid=previous_valid=false;
  reply->flags|=RISC_RESIDENT_REPLY_REDRAW;
 }
 reply->flags|=RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
 resident_policy_requested=false;resident_dispatching=false;
 return RISC_RESIDENT_OK;
}
#endif
