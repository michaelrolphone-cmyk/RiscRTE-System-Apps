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
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
static risc_runtime_api_v1 settings_runtime;
static bool settings_started,settings_initialized;
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
  if(result!=RISC_KEY_VALUE_OK && result!=RISC_KEY_VALUE_NOT_FOUND && result!=RISC_KEY_VALUE_BUFFER_SMALL)
    portable_adapter_retain();
  return result;
}
static int32_t custody_kv_put(void *context,const char *key,const void *data,uint32_t size) {
  custody_kv_slot *slot=context;
  if(native_custody_retained || !slot->live)return RISC_KEY_VALUE_CONTEXT;
  int32_t result=slot->source->put(slot->source->context,key,data,size);
  if(result!=RISC_KEY_VALUE_OK)portable_adapter_retain();
  return result;
}
static bool custody_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
  if(native_custody_retained)return false;
  bool ok=native_custody_runtime->acquire(name,version,instance,grant);
  if(!ok) {
    /* Native-only unavailable acquisition has a checked clean-empty exception.
     * External provider rollback cannot be established by a boolean result. */
    if(strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY) || grant->api || grant->slot || grant->generation)
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
  return true;
}
static bool custody_release(risc_runtime_capability_v1 *grant) {
  if(native_custody_retained)return false;
  risc_runtime_capability_v1 released=*grant;
  custody_kv_slot *slot=NULL;
  for(unsigned i=0;i<16;++i)if(released.api==&custody_kv_slots[i].api) {slot=&custody_kv_slots[i];break;}
  if(slot)released.api=slot->source;
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
