#ifndef PORTABLE_SETTINGS_AVAILABILITY_H
#define PORTABLE_SETTINGS_AVAILABILITY_H
/* Explicit read-only status. Data grants and all draft decisions stay with the
 * caller. A generation change never licenses a merge, reset or grant release. */
#include <stdbool.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscKeyValueAvailabilityV1.h"
typedef struct PortableSettingsAvailability {
 risc_runtime_capability_v1 grant;
 const risc_key_value_availability_v1 *api;
 risc_key_value_availability_state_v1 state;
} PortableSettingsAvailability;
static inline bool portable_settings_availability_refresh(PortableSettingsAvailability *s) {
 if(!s||!s->api)return false;
 risc_key_value_availability_state_v1 next={.struct_size=sizeof(next)};
 int rc=s->api->snapshot(s->api->context,&next);
 if(rc!=RISC_KEY_VALUE_AVAILABILITY_OK||next.struct_size<sizeof(next)||
    next.namespace_id!=s->state.namespace_id||next.reserved||!next.generation||
    next.generation<s->state.generation||
    (next.generation==s->state.generation&&s->state.availability&&next.availability!=s->state.availability)||next.availability<RISC_KEY_VALUE_UNAVAILABLE||
    next.availability>RISC_KEY_VALUE_CONFLICT)return false;
 s->state=next;return true;
}
static inline bool portable_settings_availability_open(PortableSettingsAvailability *s,
 const risc_runtime_api_v1 *rt,uint32_t namespace_id) {
 if(!s||s->grant.api||!rt||!rt->acquire||!rt->release||(namespace_id!=1&&namespace_id!=6))return false;
 *s=(PortableSettingsAvailability){.grant={.struct_size=sizeof(s->grant)},
  .state={.struct_size=sizeof(s->state),.namespace_id=namespace_id}};
 if(!rt->acquire(RISC_KEY_VALUE_AVAILABILITY_CAPABILITY,1,namespace_id,&s->grant))return false;
 const risc_key_value_availability_v1 *api=s->grant.api;
 if(s->grant.struct_size!=sizeof(s->grant)||!s->grant.slot||!s->grant.generation||
    !api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->snapshot)return false;
 s->api=api;return portable_settings_availability_refresh(s);
}
static inline bool portable_settings_availability_close(PortableSettingsAvailability *s,
 const risc_runtime_api_v1 *rt) {
 if(!s||!rt||!rt->release)return false;
 if(s->grant.api&&(!rt->release(&s->grant)||s->grant.struct_size!=sizeof(s->grant)||
    s->grant.api||s->grant.slot||s->grant.generation))return false;
 *s=(PortableSettingsAvailability){0};return true;
}
static inline bool portable_settings_availability_available(const PortableSettingsAvailability *s) {
 return s&&s->api&&s->state.availability==RISC_KEY_VALUE_AVAILABLE;
}
static inline const char *portable_settings_availability_message(const PortableSettingsAvailability *s) {
 if(!s||!s->api)return "Settings status unavailable";
 return s->state.availability==RISC_KEY_VALUE_AVAILABLE?"Settings available locally":
  s->state.availability==RISC_KEY_VALUE_RECONCILIATION_PENDING?"Recovered settings waiting for clean reload":
  s->state.availability==RISC_KEY_VALUE_CONFLICT?"Settings source conflict; nothing overwritten":
  "Settings unavailable; temporary defaults";
}
#endif
