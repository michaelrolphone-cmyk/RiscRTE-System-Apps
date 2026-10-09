/* One Runtime fence shared by all opt-in X4 paper controllers. */
void portable_adapter_retain(void) {
  if(!native_custody_retained) {
    if(!native_custody_runtime || !native_custody_runtime->retain_invocation ||
       !native_custody_runtime->retain_invocation())for(;;){}
    native_custody_retained=true;
    native_custody_surface=surface;surface=(risc_display_surface_v1){0};
  }
  failed=true;gauge=NULL;
#ifdef PORTABLE_ALARM_CLIENT
  native_sleep_retained=true;
#endif
}
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
bool portable_adapter_retained(void) {return native_custody_retained;}
#endif
#if defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_NATIVE_TIME_TOOLBAR)
static risc_runtime_api_v1 settings_runtime;
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
static bool settings_started,settings_initialized;
#endif
typedef struct {
  bool live;
  const risc_key_value_v1 *source;
  risc_key_value_v1 api;
} custody_kv_slot;
static custody_kv_slot custody_kv_slots[16];
static int32_t custody_kv_get(void *context,const char *key,void *data,uint32_t size,uint32_t *used) {
  custody_kv_slot *slot=context;
  if(native_custody_retained || !slot->live)return RISC_KEY_VALUE_CONTEXT;
  int32_t result=slot->source->get(slot->source->context,key,data,size,used);
  /* Canonical Runtime/NVS IO reports persistence failure, not lost custody.
   * The checked Set Time helper independently applies its stricter stop rule. */
  if(result!=RISC_KEY_VALUE_OK && result!=RISC_KEY_VALUE_NOT_FOUND && result!=RISC_KEY_VALUE_BUFFER_SMALL &&
     result!=RISC_KEY_VALUE_INVALID && result!=RISC_KEY_VALUE_IO)portable_adapter_retain();
  return result;
}
static int32_t custody_kv_put(void *context,const char *key,const void *data,uint32_t size) {
  custody_kv_slot *slot=context;
  if(native_custody_retained || !slot->live)return RISC_KEY_VALUE_CONTEXT;
  int32_t result=slot->source->put(slot->source->context,key,data,size);
  if(result!=RISC_KEY_VALUE_OK && result!=RISC_KEY_VALUE_IO && result!=RISC_KEY_VALUE_INVALID &&
     result!=RISC_KEY_VALUE_NOT_FOUND && result!=RISC_KEY_VALUE_BUFFER_SMALL)portable_adapter_retain();
  return result;
}
#ifdef PORTABLE_QUICK_RADIOS
#include "PortableQuickRadios.h"
static const wifi_api_v1 *custody_wifi;
static const portable_bluetooth_control_v1 *custody_ble;
static wifi_api_v1 custody_wifi_proxy;
static portable_bluetooth_control_v1 custody_ble_proxy;
static bool custody_ble_wanted;
static bool custody_wifi_disconnect(void *context) {
  if(native_custody_retained)return false;
  if(!custody_wifi->disconnect_checked(context)){portable_adapter_retain();return false;}
  return true;
}
static wifi_link_t custody_wifi_status(void *context) {
  if(native_custody_retained)return WIFI_LINK_JOINING;
  wifi_link_t status=custody_wifi->status(context);
  if(status!=WIFI_LINK_DOWN)portable_adapter_retain();
  return status;
}
static bool custody_ble_set(void *context,bool enabled) {
  if(native_custody_retained)return false;
  if(!custody_ble->set_enabled(context,enabled)){portable_adapter_retain();return false;}
  custody_ble_wanted=enabled;return true;
}
static bool custody_ble_status(void *context,uint8_t *out) {
  if(native_custody_retained)return false;
  if(!custody_ble->status(context,out) || *out!=(custody_ble_wanted?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF)) {
    portable_adapter_retain();return false;
  }
  return true;
}
#endif
static bool custody_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
  if(native_custody_retained)return false;
  bool ok=native_custody_runtime->acquire(name,version,instance,grant);
  if(!ok) {
    /* Native-only unavailable acquisition has a checked clean-empty exception.
     * External provider rollback cannot be established by a boolean result. */
    if((strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
        && strcmp(name,RISC_REALTIME_CAPABILITY)
#endif
       ) || grant->api || grant->slot || grant->generation)
      portable_adapter_retain();
    return false;
  }
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
    const risc_key_value_v1 *kv=grant->api;
    if(!kv || kv->api_version!=1 || kv->struct_size<sizeof(*kv) || !kv->get || !kv->put) {
      portable_adapter_retain();return false;
    }
    custody_kv_slot *slot=NULL;
    for(unsigned i=0;i<16;++i)if(!custody_kv_slots[i].live){slot=&custody_kv_slots[i];break;}
    if(!slot){portable_adapter_retain();return false;}
    slot->live=true;slot->source=kv;slot->api=*kv;slot->api.context=slot;
    slot->api.get=custody_kv_get;slot->api.put=custody_kv_put;grant->api=&slot->api;
  }
#ifdef PORTABLE_QUICK_RADIOS
  if(!strcmp(name,"net.wifi")) {
    const wifi_api_v1 *source=grant->api;
    if(!source || source->api_version!=1 || source->struct_size<sizeof(*source) || !source->status || !source->disconnect_checked) {portable_adapter_retain();return false;}
    custody_wifi=source;custody_wifi_proxy=*source;custody_wifi_proxy.status=custody_wifi_status;
    custody_wifi_proxy.disconnect_checked=custody_wifi_disconnect;grant->api=&custody_wifi_proxy;
  }
  if(!strcmp(name,"bluetooth.hci")) {
    const portable_bluetooth_control_v1 *source=grant->api;
    if(!source || source->api_version!=1 || source->struct_size<sizeof(*source) || !source->set_enabled || !source->status) {portable_adapter_retain();return false;}
    custody_ble=source;custody_ble_proxy=*source;custody_ble_proxy.set_enabled=custody_ble_set;
    custody_ble_proxy.status=custody_ble_status;grant->api=&custody_ble_proxy;
  }
#endif
  return true;
}
static bool custody_release(risc_runtime_capability_v1 *grant) {
  if(native_custody_retained)return false;
  risc_runtime_capability_v1 released=*grant;
  custody_kv_slot *slot=NULL;
  for(unsigned i=0;i<16;++i)if(released.api==&custody_kv_slots[i].api) {slot=&custody_kv_slots[i];break;}
  if(slot)released.api=slot->source;
#ifdef PORTABLE_QUICK_RADIOS
  if(released.api==&custody_wifi_proxy)released.api=custody_wifi;
  if(released.api==&custody_ble_proxy)released.api=custody_ble;
#endif
  if(!native_custody_runtime->release(&released) || released.api || released.slot || released.generation) {
    portable_adapter_retain();return false;
  }
  *grant=released;if(slot)slot->live=false;return true;
}
static bool custody_health(risc_runtime_health_v1 *out) {
  if(native_custody_retained)return false;
  if(!native_custody_runtime->health(out)){portable_adapter_retain();return false;}
  return true;
}
static void custody_yield(uint32_t delay) {if(!native_custody_retained)native_custody_runtime->yield_ms(delay);}
static bool custody_diagnostic(const char *line) {return !native_custody_retained && native_custody_runtime->diagnostic(line);}
static bool custody_launch(const char *path) {return !native_custody_retained && native_custody_runtime->request_launch(path);}
#endif
