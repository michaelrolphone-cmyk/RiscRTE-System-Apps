/* Home-only automatic controller binding. No ambient/generic KV write route. */
#include "PortablePreferenceAutomatic.h"
static risc_runtime_capability_v1 home_automatic_grant;
static portable_preference_automatic home_automatic;
static bool home_automatic_guard(void *unused) {
 (void)unused;return !native_custody_retained&&!failed&&!quick.retained;
}
static bool home_automatic_before(void *unused,bool write) {
 (void)unused;(void)write;
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!(write?contexts_before_storage():contexts_before_storage_read()))return false;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 if(!(write?broadcast_before_storage():broadcast_before_storage_read()))return false;
#endif
 return home_automatic_guard(NULL);
}
static bool home_automatic_wait(void *unused) {
 (void)unused;rt->yield_ms(1);
 if(!home_automatic_guard(NULL))return true;
 input_service();input_dispatch();
 return home_pending||handoff_requested||(navigation_pending&T5_APP_BUTTON_BACK);
}
static bool home_automatic_owned(void) {
 return home_automatic.running||home_automatic.operation.token||home_automatic_grant.api;
}
static bool home_automatic_close(void) {
 if(!home_automatic_owned()&&!home_automatic.retained)return true;
 if(!home_automatic_guard(NULL)||home_automatic.retained||home_automatic.running)return false;
 if(home_automatic.operation.token) {
  int rc=portable_preference_atomic_cancel(&home_automatic.operation);
  if(rc==RISC_ATOMIC_PENDING||rc==RISC_ATOMIC_RETAINED)return false;
 }
 if(home_automatic_grant.api&&!rt->release(&home_automatic_grant))return false;
 if(!home_automatic_guard(NULL))return false;
 home_automatic_grant=(risc_runtime_capability_v1){0};home_automatic=(portable_preference_automatic){0};return true;
}
static bool home_automatic_begin(unsigned boundary) {
 if(home_automatic_owned()||!home_automatic_guard(NULL))return false;
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!portable_home_settings_poll()||!portable_home_settings_available())return false;
#endif
 home_automatic_grant=(risc_runtime_capability_v1){.struct_size=sizeof(home_automatic_grant)};
 bool opened=rt->acquire(RISC_KEY_VALUE_ATOMIC_CAPABILITY,1,1,&home_automatic_grant);
 if(!home_automatic_guard(NULL))return false;
 if(!opened) {
  if(home_automatic_grant.struct_size!=sizeof(home_automatic_grant)||home_automatic_grant.api||home_automatic_grant.slot||home_automatic_grant.generation)portable_adapter_retain();
  return false;
 }
 if(home_automatic_grant.struct_size!=sizeof(home_automatic_grant)||!home_automatic_grant.slot||!home_automatic_grant.generation||!portable_preference_atomic_api_valid(home_automatic_grant.api)) {
  portable_adapter_retain();return false;
 }
 int rc=portable_preference_automatic_begin(&home_automatic,home_automatic_grant.api,boundary,
  home_automatic_guard,home_automatic_before,home_automatic_wait,NULL);
 if(rc==RISC_ATOMIC_RETAINED)portable_adapter_retain();
 return rc==PORTABLE_PREFERENCE_READY;
}
#ifdef PORTABLE_LOW_BATTERY
static unsigned home_automatic_low_battery(const risc_battery_sample_v1 *sample) {
 if(!portable_low_battery_sample_valid(sample))return PORTABLE_LOW_BATTERY_NONE;
 bool low=sample->percent<PORTABLE_LOW_BATTERY_THRESHOLD;
 if(low_battery.observed&&low_battery.low==low)return PORTABLE_LOW_BATTERY_NONE;
 unsigned result;
 if(home_automatic_begin(PORTABLE_AUTOMATIC_LOW_BATTERY))
  result=portable_low_battery_observe(&low_battery,&home_automatic.view,sample);
 else {low_battery.observed=true;low_battery.low=low;result=PORTABLE_LOW_BATTERY_ERROR;}
 if(!home_automatic_close()){portable_adapter_retain();result|=PORTABLE_LOW_BATTERY_RETAINED;}
 return result;
}
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
static inline bool home_automatic_preset(portable_contexts_client *c,const portable_context_preset *p,
 bool low,uint16_t *applied,bool (*face)(const risc_key_value_v1 *,unsigned)) {
 (void)c;bool ok=false;
 if(home_automatic_begin(PORTABLE_AUTOMATIC_HOME_PRESET))
  ok=portable_contexts_apply_preferences(&home_automatic.view,p,low,applied,face);
 if(!home_automatic_close()){portable_adapter_retain();return false;}
 return ok;
}
#endif
