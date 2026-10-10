/* Explicit RF-only default Clock rendezvous. The source application owns and
 * exports its model records; Clock never opens an owner storage namespace.
 * Sparse TIMER never polls this foreground adapter. */
static bool contexts_clock_status(contexts_status_v1 *out) {
  *out=(contexts_status_v1){.struct_size=sizeof(*out)};
  if(!contexts_client.api->status(contexts_client.api->context,out) ||
     out->struct_size!=sizeof(*out) || out->cleanup_pending ||
     (out->export_pending&~CONTEXTS_ALL) ||
     (out->export_active!=0 && out->export_active!=CONTEXTS_AUDIO && out->export_active!=CONTEXTS_RADIO)) {
    portable_adapter_retain();return false;
  }
  return true;
}
static bool contexts_clock_recover(void) {
  if(contexts_clock_recovered)return true;
  contexts_status_v1 status;
  if(!contexts_clock_status(&status))return false;
  /* An owner that returned without finishing is a visible model failure.
   * Only an explicit reload may retry it; do not repeatedly launch it. */
  if(status.export_active && (!portable_contexts_stop() ||
     !contexts_client.api->finish_export(contexts_client.api->context,
       status.export_active,CONTEXTS_EXPORT_UNSUPPORTED))) {
    portable_adapter_retain();return false;
  }
  contexts_clock_recovered=true;return true;
}
#ifdef PORTABLE_RESIDENT_SHELL_HOST
/* A resident Home keeps its static state while the model owner runs. Call
 * after that child returns cleanly (including a clean launch failure), before
 * resuming Home polling. The next settled tick verifies export completion. */
static void contexts_clock_owner_returned(void) {
  if(!contexts_clock_handoff)return;
  contexts_clock_handoff=false;contexts_clock_recovered=false;
}
#endif
static bool contexts_clock_rendezvous(void) {
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  if(contexts_clock_handoff)return true;
#endif
  contexts_status_v1 status;
  if(!contexts_clock_status(&status))return false;
  /* Never dispatch an owner outside the selected source mask, even if a
   * mismatched service has queued one. This profile has no Audio owner. */
  uint32_t unsupported=status.export_pending&~PORTABLE_CONTEXTS_SOURCE_MASK;
  if(unsupported) {
    if(!portable_contexts_stop() || !contexts_client.api->finish_export(
       contexts_client.api->context,unsupported,CONTEXTS_EXPORT_UNSUPPORTED) ||
       !contexts_clock_status(&status)){portable_adapter_retain();return false;}
  }
  if(contexts_client.settings_valid && contexts_client.policy.enabled &&
     !status.export_active && !status.export_pending &&
     status.radio.model_state==CONTEXTS_MODEL_EMPTY) {
    if(!portable_background_stop() || !contexts_client.api->request_export(
       contexts_client.api->context,PORTABLE_CONTEXTS_SOURCE_MASK) ||
       !contexts_clock_status(&status)){portable_adapter_retain();return false;}
  }
  if(!(status.export_pending&PORTABLE_CONTEXTS_SOURCE_MASK) || status.export_active)return true;
  if(!portable_background_stop())return false;
  if(status.radio.model_state==CONTEXTS_MODEL_UNAVAILABLE) {
    if(!contexts_client.api->finish_export(contexts_client.api->context,
       CONTEXTS_RADIO,CONTEXTS_EXPORT_UNSUPPORTED)){portable_adapter_retain();return false;}
    return true;
  }
  if(!contexts_client.api->begin_export(contexts_client.api->context,CONTEXTS_RADIO)) {
    portable_adapter_retain();return false;
  }
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  /* The host cannot use destructive Runtime launch. Return the borrowed grant
   * before queuing the model owner for the controller's resident dispatch. */
  if(!contexts_suspend())return false;
  if(resident_queue_launch("waterfall.elf")){contexts_clock_handoff=true;return true;}
#else
  /* The guarded legacy launch closes this borrowed service grant first. */
  if(rt->request_launch("waterfall.elf")){contexts_clock_handoff=true;return true;}
#endif
  if(native_custody_retained)return false;
  if(!contexts_open())return contexts_unavailable&&!native_custody_retained;
  if(!contexts_client.api->finish_export(contexts_client.api->context,
     CONTEXTS_RADIO,CONTEXTS_EXPORT_UNSUPPORTED)){portable_adapter_retain();return false;}
  return true;
}
