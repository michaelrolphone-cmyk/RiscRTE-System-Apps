/* Only the selected Settings UI owns USER preference intents. settings_store
 * is a same-Core read view whose put is fenced; it is never a legacy fallback. */
#include "PortablePreferenceAtomic.h"
static risc_runtime_capability_v1 settings_atomic_grant;
static const risc_key_value_atomic_v1 *settings_atomic_api;
static portable_preference_atomic settings_atomic_edit;
static risc_key_value_v1 settings_atomic_view;
static bool settings_atomic_running,settings_atomic_exit,settings_atomic_submission_accepted;
static int settings_atomic_status;
static bool settings_atomic_guard(void *unused) {
 (void)unused;return !native_custody_retained&&!failed;
}
static bool settings_atomic_owned(void) {
 return settings_atomic_running||settings_atomic_edit.token!=0;
}
static const char *settings_atomic_message(int rc) {
 return rc==RISC_ATOMIC_STALE?"Settings changed; reopen editor":
  rc==RISC_ATOMIC_CANCELLED?"Save cancelled":rc==RISC_ATOMIC_CAPACITY?"Native storage full; not saved":
  rc==RISC_ATOMIC_UNCERTAIN?"Save uncertain; reopen to inspect":
  rc==RISC_ATOMIC_CONFLICT?"Preference conflict; not saved":
  rc==RISC_ATOMIC_RETAINED?"Storage custody unconfirmed":
  "Atomic storage unavailable; reopen editor";
}
static int settings_atomic_result(int rc) {
 settings_atomic_status=rc;
 if(rc==RISC_ATOMIC_RETAINED && !native_custody_retained)portable_adapter_retain();
 return rc;
}
static int32_t settings_atomic_get(void *unused,const char *key,void *out,uint32_t cap,uint32_t *size) {
 (void)unused;if(size)*size=0;
 if(!settings_atomic_api||!settings_atomic_guard(NULL))return RISC_KEY_VALUE_IO;
 uint64_t revision=settings_atomic_edit.valid?settings_atomic_edit.revision:0;
 int rc=RISC_ATOMIC_OK;
 if(!revision) {
  risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
  rc=settings_atomic_api->state(settings_atomic_api->context,&state);
  if(!settings_atomic_guard(NULL))return RISC_KEY_VALUE_CONTEXT;
  if(!rc&&!portable_preference_atomic_state_valid(&state))rc=RISC_ATOMIC_INVALID;
  revision=state.revision;
 }
 if(!rc)rc=settings_atomic_api->get(settings_atomic_api->context,revision,key,out,cap,size);
 if(!settings_atomic_guard(NULL))return RISC_KEY_VALUE_CONTEXT;
 if(rc==RISC_ATOMIC_RETAINED || (rc>RISC_ATOMIC_CANCELLED||rc<RISC_ATOMIC_RETAINED)) {
  settings_atomic_result(RISC_ATOMIC_RETAINED);return RISC_KEY_VALUE_CONTEXT;
 }
 return rc==RISC_ATOMIC_OK?RISC_KEY_VALUE_OK:rc==RISC_ATOMIC_NOT_FOUND?RISC_KEY_VALUE_NOT_FOUND:
  rc==RISC_ATOMIC_BUSY||rc==RISC_ATOMIC_STALE?RISC_KEY_VALUE_BUSY:
  rc==RISC_ATOMIC_BUFFER_SMALL?RISC_KEY_VALUE_BUFFER_SMALL:RISC_KEY_VALUE_IO;
}
static int32_t settings_atomic_deny_put(void *c,const char *key,const void *data,uint32_t size) {
 (void)c;(void)key;(void)data;(void)size;return RISC_KEY_VALUE_CONTEXT;
}
static bool settings_atomic_open(void) {
 settings_atomic_grant=(risc_runtime_capability_v1){.struct_size=sizeof(settings_atomic_grant)};
 settings_atomic_api=NULL;settings_atomic_edit=(portable_preference_atomic){0};
 settings_atomic_running=settings_atomic_exit=false;settings_atomic_status=RISC_ATOMIC_OK;
 if(!rt->acquire(RISC_KEY_VALUE_ATOMIC_CAPABILITY,RISC_KEY_VALUE_ATOMIC_API_V1,
   PORTABLE_PREFERENCE_NAMESPACE,&settings_atomic_grant))return !native_custody_retained;
 if(native_custody_retained)return false;
 if(settings_atomic_grant.struct_size!=sizeof(settings_atomic_grant)||!settings_atomic_grant.slot||
    !settings_atomic_grant.generation||!portable_preference_atomic_api_valid(settings_atomic_grant.api)) {
  settings_atomic_result(RISC_ATOMIC_RETAINED);return false;
 }
 settings_atomic_api=settings_atomic_grant.api;
 settings_atomic_view=(risc_key_value_v1){1,sizeof(settings_atomic_view),NULL,settings_atomic_get,settings_atomic_deny_put};
 settings_store=&settings_atomic_view;return true;
}
static bool settings_atomic_begin(const char *key) {
 settings_atomic_exit=false;settings_atomic_submission_accepted=false;
 int rc=portable_preference_atomic_begin(&settings_atomic_edit,settings_atomic_api,key,
  PORTABLE_PREFERENCE_USER,settings_atomic_guard,NULL);
 settings_atomic_result(rc);
 if(rc!=PORTABLE_PREFERENCE_READY){settings_message=settings_atomic_message(rc);return false;}
 return true;
}
static bool settings_atomic_save(const char *key,const void *value,uint32_t size,const char **message);
/* Called before ANY normal finalizer teardown. A missing terminal receipt or
 * failed release keeps the grant, token, staging and entire invocation live. */
static bool settings_atomic_close(void) {
 if(native_custody_retained)return false;
 if(settings_atomic_edit.token) {
  int rc=portable_preference_atomic_cancel(&settings_atomic_edit);
  if(rc==RISC_ATOMIC_PENDING || rc==RISC_ATOMIC_RETAINED) {
   settings_atomic_result(RISC_ATOMIC_RETAINED);return false;
  }
 }
 if(settings_atomic_running){settings_atomic_result(RISC_ATOMIC_RETAINED);return false;}
 if(settings_atomic_grant.api&&!rt->release(&settings_atomic_grant))return false;
 if(native_custody_retained)return false;
 settings_atomic_api=NULL;settings_store=NULL;settings_atomic_edit=(portable_preference_atomic){0};return true;
}
