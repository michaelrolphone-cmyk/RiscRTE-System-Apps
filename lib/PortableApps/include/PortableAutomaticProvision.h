#pragma once
#include "InstalledAppsAutomaticTimeV1.h"
#include "PortableNetworkSession.h"
#include "RiscRuntimeV1.h"
/* One foreground invocation. Caller polls after first-frame service promotion
 * and drains close before every launch, sleep or teardown. No radio or storage
 * work occurs from initialization, interrupts or timer-only wake paths. */
typedef struct {
 const risc_runtime_api_v1*runtime;void*context;
 /* utc returns a configured epoch or the private zero token for a strictly
  * validated UNSET snapshot. Zero routes only to the explicit time suffix.
  * false is a read/ownership/validity refusal, never an implicit fallback. */
 bool(*network_allowed)(void*);bool(*safe)(void*);bool(*prepare)(void*);bool(*utc)(void*,uint64_t*);void(*retain)(void*);
 const char*profile;uint64_t wifi_instance,settings_namespace,volume_instance;bool sd_enabled;
} portable_automatic_hooks;
typedef struct {
 portable_automatic_hooks hooks;
 risc_runtime_capability_v1 service_grant,wifi_grant,settings_grant,volume_grant;
 const installed_apps_v1*service;const installed_apps_automatic_v1*automatic;
 portable_network_session network;
 unsigned phase;uint32_t retry_at;bool delayed,retained,closing,network_started,lease,pending_boot;
} portable_automatic_provision;
void portable_automatic_init(portable_automatic_provision*,const portable_automatic_hooks*);
int portable_automatic_step(portable_automatic_provision*,uint32_t now_ms);
int portable_automatic_close(portable_automatic_provision*);
