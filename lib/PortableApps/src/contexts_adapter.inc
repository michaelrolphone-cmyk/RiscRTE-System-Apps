/* Opt-in native Contexts client. Ambiguous ownership uses the same X4 custody
 * fence as display, input, storage, alarms and the foreground controller. */
static portable_contexts_client contexts_client;
static bool contexts_policy_io,contexts_unavailable;
static bool contexts_open(void) {
  if(native_custody_retained)return false;
  if(contexts_client.api)return true;
  contexts_unavailable=false;
  bool save_failed=contexts_client.save_failed;
  contexts_client=(portable_contexts_client){.runtime=rt,.save_failed=save_failed,
    .grant={.struct_size=sizeof(contexts_client.grant)}};
  bool acquired=rt->acquire(CONTEXTS_SERVICE_CAPABILITY,1,0,&contexts_client.grant);
  if(native_custody_retained)return false;
  const risc_runtime_capability_v1 *grant=&contexts_client.grant;
  if(!acquired) {
    if(grant->struct_size==sizeof(*grant)&&!grant->api&&!grant->slot&&!grant->generation) {
      contexts_unavailable=true;return false;
    }
    portable_adapter_retain();return false;
  }
  if(grant->struct_size!=sizeof(*grant)||!grant->api||!grant->slot||!grant->generation) {
    portable_adapter_retain();return false;
  }
  const contexts_service_v1 *api=grant->api;
  if(api->api_version!=1||api->struct_size<CONTEXTS_SERVICE_V1_SIZE||!api->step||!api->pause||!api->status||
     !api->request_export||!api->begin_export||!api->export_record||!api->finish_export||!api->label||
     !api->claim_preset||!api->preset_result||!api->capture_audio) {
    portable_adapter_retain();return false;
  }
  contexts_client.api=api;contexts_client.policy.struct_size=sizeof(contexts_client.policy);return true;
}
const contexts_service_v1 *portable_contexts_service(void) {
  return contexts_open()?contexts_client.api:NULL;
}
bool portable_contexts_stop(void) {
  if(native_custody_retained)return false;
  if(!portable_contexts_pause(&contexts_client)){portable_adapter_retain();return false;}
  return true;
}
/* Release the borrowed service grant before native Light entry. The boot
 * session model and room-entry claim belong to the service, never the app. */
static bool contexts_suspend(void) {
  if(!portable_contexts_stop())return false;
  bool save_failed=contexts_client.save_failed;
  if(!portable_contexts_close(&contexts_client)){portable_adapter_retain();return false;}
  contexts_client.save_failed=save_failed;return true;
}
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
bool portable_contexts_close_for_handoff(void) {return contexts_suspend();}
#endif
bool portable_contexts_enable(bool enabled) {
  if(!contexts_open())return false;
  contexts_policy_io=true;
  bool ok=portable_contexts_set_enabled(&contexts_client,enabled);
  contexts_policy_io=false;
  if(native_custody_retained)return false;
  contexts_status_v1 status={.struct_size=sizeof(status)};
  if(!contexts_client.api->status(contexts_client.api->context,&status)||status.cleanup_pending) {
    portable_adapter_retain();return false;
  }
  return ok;
}
unsigned portable_contexts_face_count(void) {
#ifdef PORTABLE_CONTEXT_FACE_COUNT
  return PORTABLE_CONTEXT_FACE_COUNT;
#else
  return 0;
#endif
}
const char *portable_contexts_face_name(unsigned id) {
#ifdef PORTABLE_CONTEXT_FACE_NAMES
  static const char *const names[]=PORTABLE_CONTEXT_FACE_NAMES;
  _Static_assert(sizeof(names)/sizeof(*names)==PORTABLE_CONTEXT_FACE_COUNT,"Context face catalog size");
  return id<sizeof(names)/sizeof(*names)?names[id]:NULL;
#else
  (void)id;return NULL;
#endif
}
static bool contexts_capture_checkpoint(void) {
  if(failed||native_custody_retained)return false;
  if(portable_contexts_capture(&contexts_client))return true;
  portable_adapter_retain();return false;
}
static bool contexts_before_storage(void) {
  return !native_custody_retained&&(contexts_policy_io||portable_contexts_stop());
}
