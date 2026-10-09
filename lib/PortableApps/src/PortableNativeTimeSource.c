/* Optional application-local owner for foreground apps without their own
 * native reader. Link explicitly; adapter.c never includes this source. */
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#error "Native toolbar source requires the canonical native custody fence"
#endif
#if defined(PORTABLE_SETTINGS_APP) || defined(PORTABLE_SETTINGS_NATIVE_TIME) || defined(PORTABLE_DESK_CLOCK) || defined(PORTABLE_DESK_CLOCK_SPARSE_START) || defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
#error "Native toolbar requires one unambiguous app-owned time source"
#endif
#include "PortableNativeTimeToolbar.h"
#include "PortableRealtimeClient.h"
#include "PortableTimeZonePreference.h"
#include <string.h>
#ifndef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
#error "Native toolbar source requires the complete canonical Runtime retention SDK"
#endif

/* Static invocation storage preserves unresolved grants after fencing. Clean
 * samples retain neither a provider pointer nor a snapshot/timezone cache. */
static portable_realtime_client reader;
static risc_runtime_capability_v1 zone_grant;
static const risc_key_value_v1 *zone_source;

static int foreground(void *context) {
  (void)context;
  return portable_adapter_retained()?PORTABLE_REALTIME_GUARD_RETAINED:PORTABLE_REALTIME_GUARD_SAFE;
}
static bool halted(int status) {
  if(status==PORTABLE_REALTIME_CONTEXT || status==PORTABLE_REALTIME_UNCERTAIN ||
     status==PORTABLE_REALTIME_RETAINED)portable_adapter_retain();
  return portable_adapter_retained();
}
static bool empty(const risc_runtime_capability_v1 *grant) {
  return grant->struct_size==sizeof(*grant) && !grant->api && !grant->slot && !grant->generation;
}
static bool release_zone(const risc_runtime_api_v1 *runtime) {
  if(portable_adapter_retained())return false;
  risc_runtime_capability_v1 released=zone_grant;
  bool ok=runtime->release(&released);
  if(portable_adapter_retained())return false;
  if(!ok || !empty(&released)){portable_adapter_retain();return false;}
  zone_grant=released;zone_source=NULL;return true;
}
static int32_t zone_get(void *context,const char *key,void *out,uint32_t capacity,uint32_t *used) {
  (void)context;
  if(portable_adapter_retained())return RISC_KEY_VALUE_CONTEXT;
  int32_t result=zone_source->get(zone_source->context,key,out,capacity,used);
  if(portable_adapter_retained())return RISC_KEY_VALUE_CONTEXT;
  switch(result) {
    case RISC_KEY_VALUE_OK:case RISC_KEY_VALUE_NOT_FOUND:case RISC_KEY_VALUE_BUFFER_SMALL:
    case RISC_KEY_VALUE_INVALID:case RISC_KEY_VALUE_IO:return result;
    default:portable_adapter_retain();return RISC_KEY_VALUE_CONTEXT;
  }
}
static bool load_zone(const risc_runtime_api_v1 *runtime,portable_timezone_rule *rule) {
  zone_grant=(risc_runtime_capability_v1){.struct_size=sizeof(zone_grant)};
  bool ok=runtime->acquire(RISC_KEY_VALUE_CAPABILITY,RISC_KEY_VALUE_API_V1,
    PORTABLE_TIMEZONE_STORE_INSTANCE,&zone_grant);
  if(portable_adapter_retained())return false;
  /* A boolean external-provider failure cannot prove clean rollback, even
   * with an empty grant. Typed NOT_FOUND/IO from get remain ordinary failures. */
  if(!ok || zone_grant.struct_size!=sizeof(zone_grant) || !zone_grant.api ||
     !zone_grant.slot || !zone_grant.generation){portable_adapter_retain();return false;}
  uint32_t header[2];memcpy(header,zone_grant.api,sizeof(header));
  if(header[0]!=RISC_KEY_VALUE_API_V1 || header[1]<sizeof(risc_key_value_v1)) {
    (void)release_zone(runtime);return false;
  }
  zone_source=zone_grant.api;
  if(!zone_source->get){(void)release_zone(runtime);return false;}
  /* Read-only facade: the preference loader never receives a put callback. */
  const risc_key_value_v1 checked={.api_version=RISC_KEY_VALUE_API_V1,
    .struct_size=sizeof(checked),.get=zone_get};
  char id[PORTABLE_TIMEZONE_ID_BYTES];
  int status=portable_timezone_preference_load(&checked,id);
  if(portable_adapter_retained() || !release_zone(runtime))return false;
  /* Reader's absent preference is the checked virtual UTC default. Corrupt
   * or unreadable records remain unavailable and are never overwritten. */
  return (status==PORTABLE_TIMEZONE_LOADED || status==PORTABLE_TIMEZONE_MISSING) &&
    portable_timezone_resolve(id,sizeof(id),rule)==PORTABLE_TIMEZONE_OK;
}
bool portable_app_native_local_time(twatch_rtc_time_v1 *out) {
  if(!out || portable_adapter_retained())return false;
  const risc_runtime_api_v1 *runtime=risc_runtime_get_api(RISC_RUNTIME_API_V1);
  if(portable_adapter_retained())return false;
  if(!runtime || runtime->api_version!=RISC_RUNTIME_API_V1 ||
     runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE ||
     !runtime->acquire || !runtime->release || !runtime->retain_invocation)return false;
  portable_timezone_rule rule;
  if(!load_zone(runtime,&rule))return false;
  int status=portable_realtime_open(&reader,runtime,PORTABLE_REALTIME_READER,
    PORTABLE_REALTIME_TIMER_ONLY,foreground,NULL);
  if(halted(status) || status!=PORTABLE_REALTIME_OK)return false;
  risc_realtime_snapshot_v1 snapshot;
  status=portable_realtime_read(&reader,&snapshot);
  if(halted(status))return false;
  int closed=portable_realtime_close(&reader);
  if(halted(closed) || closed!=PORTABLE_REALTIME_OK || status!=PORTABLE_REALTIME_OK)return false;
  portable_timezone_civil civil;
  if(portable_timezone_utc_to_local(&rule,snapshot.epoch_seconds,&civil,NULL)!=PORTABLE_TIMEZONE_OK)return false;
  *out=(twatch_rtc_time_v1){(uint16_t)civil.year,civil.month,civil.day,civil.weekday,
    civil.hour,civil.minute,civil.second};
  return true;
}
#endif
