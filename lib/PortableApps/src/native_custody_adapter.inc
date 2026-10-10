#ifdef PORTABLE_STAGE_LOGS
static const char *touch_issue_operation;
static int touch_issue_status;
void portable_adapter_touch_issue(const char *operation,int status,int terminal) {
  if(!operation){touch_issue_operation=NULL;touch_issue_status=0;return;}
  if(native_custody_retained)return;
#if defined(PORTABLE_DISPLAY_SETTLED) && defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
  if(terminal && resident_sleep_present){portable_adapter_retain_silent();return;}
#endif
  if(!terminal && touch_issue_operation && !strcmp(operation,touch_issue_operation) && status==touch_issue_status)return;
  touch_issue_operation=operation;touch_issue_status=status;
  char detail[128];
  snprintf(detail,sizeof(detail),"operation=%s result=%d action=%s",operation,status,
           terminal?"retain-invocation":"cancel-gesture-wait-neutral");
  portable_stage_log(native_custody_runtime,"touch-input",detail);
}
#endif
/* One Runtime fence shared by all opt-in X4 paper controllers. */
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
static void portable_adapter_retain_inner(bool log) {
#else
void portable_adapter_retain(void) {
#endif
  if(!native_custody_retained) {
#ifdef PORTABLE_FILE_SETUP
    /* Refused setup cleanup excludes diagnostic timestamps as well as normal
     * providers. The retention barrier itself remains the terminal action. */
    if(!file_setup_io_allowed())log=false;
#endif
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(log)
#endif
    portable_stage_log(native_custody_runtime,"app-retain","reason=uncertain-capability-custody");
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
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
void portable_adapter_retain(void) {
#if defined(PORTABLE_DISPLAY_SETTLED) && defined(PORTABLE_RESIDENT_SHELL_HOST) && defined(PORTABLE_RESIDENT_POLICY)
 portable_adapter_retain_inner(!resident_sleep_present);
#else
 portable_adapter_retain_inner(true);
#endif
}
void portable_adapter_retain_silent(void) {portable_adapter_retain_inner(false);}
#endif
#if defined(PORTABLE_NATIVE_TIME_TOOLBAR) || defined(PORTABLE_ALARM_TERMINAL_RETENTION)
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
#ifdef PORTABLE_BLE_BROADCAST
static bool broadcast_before_storage(void);
#endif
static int32_t custody_kv_get(void *context,const char *key,void *data,uint32_t size,uint32_t *used) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return RISC_KEY_VALUE_BUSY;
#endif
  custody_kv_slot *slot=context;
  if(native_custody_retained || !slot->live)return RISC_KEY_VALUE_CONTEXT;
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
#ifdef PORTABLE_BLE_BROADCAST
  if(!broadcast_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
  portable_perf_count(PORTABLE_PERF_KV_READS);
  int32_t result=slot->source->get(slot->source->context,key,data,size,used);
  /* Canonical Runtime/NVS IO reports persistence failure, not lost custody.
   * The checked Set Time helper independently applies its stricter stop rule. */
  if(result!=RISC_KEY_VALUE_OK && result!=RISC_KEY_VALUE_NOT_FOUND && result!=RISC_KEY_VALUE_BUFFER_SMALL &&
     result!=RISC_KEY_VALUE_INVALID && result!=RISC_KEY_VALUE_IO && result!=RISC_KEY_VALUE_BUSY)portable_adapter_retain();
  return result;
}
static int32_t custody_kv_put(void *context,const char *key,const void *data,uint32_t size) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return RISC_KEY_VALUE_BUSY;
#endif
  custody_kv_slot *slot=context;
  if(native_custody_retained || !slot->live)return RISC_KEY_VALUE_CONTEXT;
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
#ifdef PORTABLE_BLE_BROADCAST
  if(!broadcast_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
  int32_t result=slot->source->put(slot->source->context,key,data,size);
  if(result!=RISC_KEY_VALUE_OK && result!=RISC_KEY_VALUE_IO && result!=RISC_KEY_VALUE_INVALID &&
     result!=RISC_KEY_VALUE_NOT_FOUND && result!=RISC_KEY_VALUE_BUFFER_SMALL && result!=RISC_KEY_VALUE_BUSY)portable_adapter_retain();
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
#ifdef PORTABLE_FILE_SETUP
static bool custody_setup_unavailable(const char *name,uint32_t version,uint64_t instance,
    const risc_runtime_capability_v1 *grant) {
  return !strcmp(name,"bluetooth.session-setup") && version==1 &&
    instance==PORTABLE_FILE_SHARING_SETUP_INSTANCE && grant &&
    grant->struct_size==sizeof(*grant) && !grant->api && !grant->slot && !grant->generation;
}
#endif
static bool custody_acquire_for(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant,bool shutdown_radio) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return false;
#endif
  (void)shutdown_radio;
  if(native_custody_retained)return false;
  bool ok=native_custody_runtime->acquire(name,version,instance,grant);
  if(native_custody_retained)return false;
  if(!ok) {
#ifdef PORTABLE_FILE_SETUP
    if(custody_setup_unavailable(name,version,instance,grant)) {
      /* Empty refusal is optional unavailability only while this invocation
       * still owns the same native Runtime table. Never retry a dirty grant. */
      if(risc_runtime_get_api(1)!=native_custody_runtime)portable_adapter_retain_silent();
      return false;
    }
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
    /* This optional app client can show a clean missing service. No token,
     * altered record or other provider refusal is covered by this exception. */
    if(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)&&version==1&&instance==0&&
       grant->struct_size==sizeof(*grant)&&!grant->api&&!grant->slot&&!grant->generation)return false;
#endif
#ifdef PORTABLE_TEXT_INPUT_CLIENT
    /* A missing optional text host is clean only for this exact request and
     * empty, unmodified grant while native Runtime still reports live custody. */
    if(!strcmp(name,"ui.text-input")) {
      if(!risc_runtime_get_api(1)){portable_adapter_retain();return false;}
      if(version==1&&instance==0&&grant->struct_size==sizeof(*grant)&&
         !grant->api&&!grant->slot&&!grant->generation)return false;
    }
#endif
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
  if(shutdown_radio && !strcmp(name,"net.wifi")) {
    const wifi_api_v1 *source=grant->api;
    if(!source || source->api_version!=1 || source->struct_size<sizeof(*source) || !source->status || !source->disconnect_checked) {portable_adapter_retain();return false;}
    custody_wifi=source;custody_wifi_proxy=*source;custody_wifi_proxy.status=custody_wifi_status;
    custody_wifi_proxy.disconnect_checked=custody_wifi_disconnect;grant->api=&custody_wifi_proxy;
  }
  if(shutdown_radio && !strcmp(name,"bluetooth.hci")) {
    const portable_bluetooth_control_v1 *source=grant->api;
    if(!source || source->api_version!=1 || source->struct_size<sizeof(*source) || !source->set_enabled || !source->status) {portable_adapter_retain();return false;}
    custody_ble=source;custody_ble_proxy=*source;custody_ble_proxy.set_enabled=custody_ble_set;
    custody_ble_proxy.status=custody_ble_status;grant->api=&custody_ble_proxy;
  }
#endif
  return true;
}
static bool custody_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
  return custody_acquire_for(name,version,instance,grant,true);
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
  if(!native_custody_runtime->release(&released) || native_custody_retained || released.api || released.slot || released.generation || released.struct_size!=grant->struct_size) {
    portable_adapter_retain();return false;
  }
  *grant=released;if(slot)slot->live=false;return true;
}
static bool custody_health(risc_runtime_health_v1 *out) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return false;
#endif
  if(native_custody_retained)return false;
  if(!native_custody_runtime->health(out)){portable_adapter_retain();return false;}
  return true;
}
static void custody_yield(uint32_t delay) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return;
#endif
  if(!native_custody_retained)native_custody_runtime->yield_ms(delay);
}
static bool custody_diagnostic(const char *line) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return false;
#endif
  return !native_custody_retained && native_custody_runtime->diagnostic(line);
}
static bool custody_launch(const char *path) {
  if(native_custody_retained || !portable_paper_frame_drain())return false;
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_suspend())return false;
#endif
  return native_custody_runtime->request_launch(path);
}
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
/* Controllers own their grants separately from Quick Controls. In particular,
 * JOINING and UP are valid app states, not shutdown-barrier failures. Keep the
 * exact exposed token until checked release confirms it was cleared. */
static risc_runtime_api_v1 app_custody_runtime;
static risc_runtime_capability_v1 app_custody_grants[8];
static bool app_custody_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
#ifdef PORTABLE_FILE_SETUP
  if(!file_setup_io_allowed())return false;
#endif
  if(native_custody_retained)return false;
  if(!grant || grant->struct_size<sizeof(*grant) || grant->api || grant->slot || grant->generation) {
    portable_adapter_retain();return false;
  }
  risc_runtime_capability_v1 *owned=NULL;
  for(unsigned i=0;i<8;++i)if(!app_custody_grants[i].api){owned=&app_custody_grants[i];break;}
  if(!owned){portable_adapter_retain();return false;}
  if(!custody_acquire_for(name,version,instance,grant,false)) {
#ifdef PORTABLE_FILE_SETUP
    if(!native_custody_retained && custody_setup_unavailable(name,version,instance,grant))return false;
#endif
    if(!native_custody_retained)portable_adapter_retain();
    return false;
  }
  if(grant->struct_size<sizeof(*grant) || !grant->api || !grant->slot || !grant->generation) {
    portable_adapter_retain();return false;
  }
  *owned=*grant;return true;
}
static bool app_custody_release(risc_runtime_capability_v1 *grant) {
  if(native_custody_retained)return false;
  risc_runtime_capability_v1 *owned=NULL;
  if(grant)for(unsigned i=0;i<8;++i) {
    risc_runtime_capability_v1 *g=&app_custody_grants[i];
    if(g->api && g->api==grant->api && g->slot==grant->slot && g->generation==grant->generation &&
       g->struct_size==grant->struct_size){owned=g;break;}
  }
  if(!owned){portable_adapter_retain();return false;}
  if(!custody_release(grant))return false;
  memset(owned,0,sizeof(*owned));return true;
}
const risc_runtime_api_v1 *portable_app_custody_runtime(void) {
  if(native_custody_retained || !native_custody_runtime)return NULL;
  app_custody_runtime=settings_runtime;
  app_custody_runtime.acquire=app_custody_acquire;
  app_custody_runtime.release=app_custody_release;
  return &app_custody_runtime;
}
#endif
#endif
