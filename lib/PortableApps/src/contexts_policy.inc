/* Capability-derived applicability. Saved records retain unsupported fields;
 * the editor may remove them, but neither the client nor UI enables them. */
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
/* The editor needs confirmed applicability, not a copy of the host sheet.
 * Refresh this small read-only view only with capture paused and no frame. */
static bool contexts_dnd_valid,contexts_brightness_valid,contexts_volume_valid;
static bool contexts_controls_load(void) {
  if(failed||native_custody_retained)return false;
  if(!portable_contexts_stop())return false;
  risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
  const risc_key_value_v1 *kv=NULL;
  if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PQA_STORE_INSTANCE,&grant))kv=grant.api;
  if(native_custody_retained)return false;
  bool dnd=false;unsigned brightness=0,restore=0,volume=0;
  contexts_dnd_valid=pqa_dnd_load(kv,&dnd);
  if(native_custody_retained)return false;
  contexts_brightness_valid=pqa_preference_load(kv,PQA_BRIGHTNESS_KEY,PQA_BRIGHTNESS_DEFAULT,0,&brightness);
  if(native_custody_retained)return false;
  if(brightness&&brightness<10)contexts_brightness_valid=false;
  if(!brightness)contexts_brightness_valid&=pqa_preference_load(kv,PQA_RESTORE_BRIGHTNESS_KEY,PQA_BRIGHTNESS_DEFAULT,10,&restore);
  if(native_custody_retained)return false;
  contexts_volume_valid=pqa_preference_load(kv,PQA_VOLUME_KEY,PQA_VOLUME_DEFAULT,0,&volume);
  if(native_custody_retained)return false;
  if(grant.api&&!rt->release(&grant)){portable_adapter_retain();return false;}
  return !native_custody_retained;
}
#define contexts_output_modes() alarm_output_modes()
#else
#define contexts_output_modes() quick_alarm_output_modes()
#endif
static bool contexts_alert_allowed(unsigned mode) {
  return !failed&&!native_custody_retained&&mode&&!(mode&~contexts_output_modes());
}
uint16_t portable_contexts_action_mask(void) {
  if(failed||native_custody_retained)return 0;
  uint16_t mask=0;
#ifdef PORTABLE_APP_SLEEP_LOCAL
  mask|=PORTABLE_CONTEXT_IDLE;
#ifndef PORTABLE_X4_IDLE_POLICY
  mask|=PORTABLE_CONTEXT_SLEEP|PORTABLE_CONTEXT_DEEP;
#endif
#endif
#ifdef PORTABLE_RESIDENT_POLICY
  mask|=PORTABLE_CONTEXT_IDLE;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
  if(contexts_dnd_valid)mask|=PORTABLE_CONTEXT_DND;
  if(contexts_brightness_valid&&(info.flags&RISC_DISPLAY_INFO_BRIGHTNESS)&&display->set_brightness)
    mask|=PORTABLE_CONTEXT_BRIGHTNESS;
  uint32_t modes=contexts_output_modes();
  if(contexts_volume_valid&&(modes&ALARM_MODE_SOUND))mask|=PORTABLE_CONTEXT_VOLUME;
#else
  if(quick.ui.dnd_valid)mask|=PORTABLE_CONTEXT_DND;
  if(quick.ui.brightness_valid&&(info.flags&RISC_DISPLAY_INFO_BRIGHTNESS)&&display->set_brightness)
    mask|=PORTABLE_CONTEXT_BRIGHTNESS;
  uint32_t modes=contexts_output_modes();
  if(quick.ui.volume_valid&&(modes&ALARM_MODE_SOUND))mask|=PORTABLE_CONTEXT_VOLUME;
#endif
  if(modes&(ALARM_MODE_SOUND|ALARM_MODE_VIBRATE))mask|=PORTABLE_CONTEXT_ALERT;
  if(portable_contexts_face_count())mask|=PORTABLE_CONTEXT_FACE;
  return mask;
}
static bool contexts_face_save(const risc_key_value_v1 *kv,unsigned id) {
#ifdef PORTABLE_CONTEXT_FACE_COUNT
  if(id>=PORTABLE_CONTEXT_FACE_COUNT||id>255u||!pqa_preferences_valid(kv))return false;
  const uint8_t wanted[]={0x46,1,(uint8_t)id,(uint8_t)(id^0xa5u)};
  int32_t result=kv->put(kv->context,"watch_face",wanted,sizeof(wanted));
  uint8_t actual[4];uint32_t size=0;
  return (result==RISC_KEY_VALUE_OK||result==RISC_KEY_VALUE_IO)&&
    kv->get(kv->context,"watch_face",actual,sizeof(actual),&size)==RISC_KEY_VALUE_OK&&
    size==sizeof(actual)&&!memcmp(actual,wanted,sizeof(actual));
#else
  (void)kv;(void)id;return false;
#endif
}
#include "contexts_rules_adapter.inc"
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
#include "contexts_clock.inc"
#endif
static bool contexts_tick(void) {
  if(failed||native_custody_retained)return false;
#if defined(PORTABLE_CONTEXTS_CLOCK_RF_ONLY) && defined(PORTABLE_RESIDENT_SHELL_HOST)
  /* The controller has not yet returned from the queued owner. Keep its
   * export active and the Home grant closed until the explicit return hook. */
  if(contexts_clock_handoff)return true;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
  if(alarm_modal)return portable_contexts_stop();
  if(!contexts_client.loaded&&!contexts_controls_load())return false;
#else
  if(!quick_storage_safe()||quick_modal||alarm_modal)return portable_contexts_stop();
#endif
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
  /* Finish the user's current Home/Quick/navigation gesture before an owner
   * rendezvous or preset can replace the foreground. */
#ifdef PORTABLE_PAPER_CROSSFADE
  if(portable_paper_transition_active())return portable_contexts_stop();
#endif
  if(!automatic_idle_ready() || home_pending || crown_pending || navigation_pending ||
     (input_pending && (input_sample.down || input_sample.released || input_sample.cancelled)))
    return portable_contexts_stop();
#endif
  if(!contexts_open())return contexts_unavailable&&!native_custody_retained;
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
  if(!contexts_clock_recover())return false;
#endif
  bool audio_allowed=true,radio_allowed=true;
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
  audio_allowed=false;
#endif
#if defined(PORTABLE_AUDIO_SESSION) || defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
  audio_allowed=false;
#endif
  /* Synchronous paper output cannot promise capture servicing during its
   * blocking provider transfer. RF-only monitoring remains available. */
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1&&!(info.flags&RISC_DISPLAY_INFO_ASYNC_PRESENT))audio_allowed=false;
#if defined(PORTABLE_RADIO_SESSION) || defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP) || defined(PORTABLE_BLE_FOREGROUND)
  radio_allowed=false;
#endif
  contexts_policy_io=true;
  bool ok=portable_contexts_step(&contexts_client,audio_allowed,radio_allowed);
  contexts_policy_io=false;
  if(!ok||native_custody_retained){portable_adapter_retain();return false;}
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
  if(!contexts_clock_rendezvous())return false;
  if(contexts_clock_handoff || !contexts_client.api)return true;
#endif
#if !defined(PORTABLE_AUDIO_SESSION) && !defined(PORTABLE_RADIO_SESSION) && !defined(PORTABLE_WIFI_SETTINGS_APP) && !defined(PORTABLE_UPDATE_APP) && !defined(PORTABLE_BLE_FOREGROUND) && !defined(PORTABLE_CONTEXTS_EDITOR)
  uint16_t applied=0;
  contexts_policy_io=true;
  ok=contexts_client.rules_available?contexts_rules_tick():portable_contexts_apply_room(&contexts_client,&applied,contexts_face_save);
  contexts_policy_io=false;
  if(!ok||native_custody_retained){portable_adapter_retain();return false;}

#ifndef PORTABLE_RESIDENT_SHELL_CLIENT
  if(applied) {
    if(!pqa_session_load(&quick,rt)||!pqa_session_restore(&quick,display)){portable_adapter_retain();return false;}
    quick_paper_capabilities();
    if(applied&(PORTABLE_CONTEXT_VOLUME|PORTABLE_CONTEXT_DND|PORTABLE_CONTEXT_ALERT)) {
#ifdef ALARM_SERVICE_TAGGED_V2
      if(!alarm_refresh()){portable_adapter_retain();return false;}
#else
      const alarm_service_v1 *service=alarm_sleep_api();
      if(service) {int32_t rc=service->refresh(service->context);
        if(rc!=ALARM_OK&&rc!=ALARM_PENDING){portable_adapter_retain();return false;}}
#endif
    }
  }
#endif
#else
  (void)contexts_face_save;(void)contexts_rules_tick;
#endif
  return !native_custody_retained;
}
