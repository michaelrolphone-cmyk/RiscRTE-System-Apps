/* Selected native client. The boot-session service owns the advertiser; only
 * synchronous calls borrow app grants. No service slot survives into sleep. */
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#error "Native broadcast integration requires the checked custody fence"
#endif
static portable_broadcast_client broadcast_client;
static bool broadcast_policy_io,broadcast_pause_confirmed;
static bool broadcast_open(void) {
  if(native_custody_retained)return false;
  if(broadcast_client.api)return true;
  /* Keep policy/save-failure state across transient service grants. */
  broadcast_client.runtime=rt;
  broadcast_client.grant=(risc_runtime_capability_v1){.struct_size=sizeof(broadcast_client.grant)};
  if(!rt->acquire(TELEMETRY_BROADCAST_CAPABILITY,1,0,&broadcast_client.grant))return false;
  const telemetry_broadcast_v1 *p=broadcast_client.grant.api;
  if(!p || p->api_version!=1 || p->struct_size<sizeof(*p) || !p->step || !p->pause ||
     !p->status || !p->enumerate || !p->read){portable_adapter_retain();return false;}
  broadcast_client.api=p;return true;
}
static bool broadcast_release(void) {
  if(native_custody_retained)return false;
  if(!portable_broadcast_release_storage(&broadcast_client)){portable_adapter_retain();return false;}
  if(broadcast_client.grant.api && !rt->release(&broadcast_client.grant)) {
    portable_adapter_retain();return false;
  }
  broadcast_client.api=NULL;return true;
}
bool portable_broadcast_stop(void) {
#if defined(PORTABLE_WIFI_SESSION_APP)
  /* Wi-Fi admission pauses advertising before it owns the async operation.
   * HCI start/stop remains deferred through checked Wi-Fi quiescence. */
  if(wifi_session_interaction_active())return !native_custody_retained && broadcast_pause_confirmed;
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  if(desk_phase==DESK_TIMER)return !native_custody_retained;
#endif
  if(!broadcast_open())return false;
  if(!portable_broadcast_pause(&broadcast_client)){portable_adapter_retain();return false;}
  broadcast_pause_confirmed=true;return broadcast_release();
}
bool portable_broadcast_status(telemetry_broadcast_status_v1 *out) {
  if(!broadcast_open())return false;
  bool ok=broadcast_client.api->status(broadcast_client.api->context,out);
  if(!ok){portable_adapter_retain();return false;}
  return broadcast_release();
}
bool portable_broadcast_enable(bool enabled) {
  if(!broadcast_open())return false;
  broadcast_policy_io=true;
  bool ok=portable_broadcast_set_enabled(&broadcast_client,enabled);
  broadcast_policy_io=false;
  if(native_custody_retained)return false;
  /* A persistence error is a retryable UI result. Cleanup uncertainty is not. */
  telemetry_broadcast_status_v1 state={.struct_size=sizeof(state)};
  if(!broadcast_client.api->status(broadcast_client.api->context,&state) || state.cleanup_pending) {
    portable_adapter_retain();return false;
  }
  return broadcast_release() && ok;
}
static bool broadcast_tick(void) {
  if(native_custody_retained)return false;
#ifdef PORTABLE_WIFI_SESSION_APP
  /* Poll completion even while deferring the advertiser, so Cancel progresses. */
  if(!portable_wifi_services_safe())return !native_custody_retained;
  if(wifi_session_interaction_active())return broadcast_pause_confirmed;
#endif
  bool allow=true;
#ifdef PORTABLE_BLE_FOREGROUND
  allow=false;
#endif
#ifdef PORTABLE_FILE_BROWSER_APP
  allow=allow && portable_file_browser_safe();
#endif
#ifdef PORTABLE_RADIO_SESSION
  allow=allow && portable_radio_services_safe();
#endif
#ifdef PORTABLE_AUDIO_SESSION
  allow=allow && portable_audio_services_safe();
#endif
#ifdef PORTABLE_UPDATE_APP
  allow=allow && portable_update_broadcast_safe();
#endif
#if defined(PORTABLE_WIFI_SESSION_APP) || defined(PORTABLE_UPDATE_APP)
  allow=allow && portable_wifi_services_safe();
#endif
  if(!broadcast_open())return false;
  broadcast_policy_io=true;
  broadcast_pause_confirmed=false;
  bool ok=portable_broadcast_step(&broadcast_client,allow);
  broadcast_policy_io=false;
  if(!ok) {
    if(!native_custody_retained)portable_adapter_retain();
    return false;
  }
  return broadcast_release();
}

#if defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_NATIVE_TIME_TOOLBAR) || defined(PORTABLE_X4_IDLE_POLICY)
static bool broadcast_before_storage(void) {
  if(native_custody_retained)return false;
  /* The client pauses its own advertiser before policy reads/writes. Avoid
   * reacquiring/releasing the grant that owns that synchronous operation. */
  return broadcast_policy_io || portable_broadcast_stop();
}
#endif
