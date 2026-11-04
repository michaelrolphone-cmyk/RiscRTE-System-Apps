#ifndef PORTABLE_SETTINGS_RUNTIME_H
#define PORTABLE_SETTINGS_RUNTIME_H
#include "PortableSettingsModel.h"
#include "PortableSettingsAvailability.h"
#include "PortableClockCommit.h"
/* Optional deployment-selected semantic policy. Not a hardware classifier.
 * A deployment can instead pass this exact descriptor directly to psr_open.
 * No provider exists by default: missing policy is an explicit open failure. */
#define PORTABLE_SETTINGS_POLICY_CAPABILITY "settings.policy"
enum { PSR_CLOCK_NONE, PSR_CLOCK_NATIVE, PSR_CLOCK_RTC };
enum { PSR_STORE_LEGACY, PSR_STORE_ATOMIC, PSR_STORE_PREFERENCES };
typedef struct {
 uint32_t api_version,struct_size;
 psm_policy model;
 uint32_t clock_mode,storage_mode,apply_features;
 uint64_t rtc_instance;
 void *context;
 int (*applied)(void *,psm_feature,const psm_value *);
} portable_settings_policy_v1;
typedef struct {
 const risc_runtime_api_v1 *runtime;
 portable_realtime_phase_guard guard;void (*retain)(void *);void *context;
 bool opened,retained,editing;
 unsigned phase;
 risc_runtime_capability_v1 policy_grant,kv_grant,atomic_grant;
 PortableSettingsAvailability availability;
 const risc_key_value_v1 *store,*bound_store;
 risc_key_value_v1 guarded_store;
 const risc_key_value_atomic_v1 *atomic_store;
 portable_settings_policy_v1 policy;
 psm_services services;
 portable_alarm_client alarms;
 portable_clock_commit commit;
 portable_set_time_client clock;
 portable_realtime_client reader;
 portable_rtc_basis captured_basis;
 char captured_zone[PORTABLE_TIMEZONE_ID_BYTES];
 portable_preference_atomic metadata;
 portable_set_time_request request;
 portable_set_time_result result;
} PortableSettingsRuntime;
/* Zero-initialize. guard and retain are mandatory invocation-owned callbacks.
 * false may leave grants requiring psr_close; retained means no further I/O.
 * Compile backend and model consistently with PORTABLE_PREFERENCE_STORE when
 * selecting storage.preferences, or use PORTABLE_PREFERENCE_ANY_STORE for one
 * ELF supporting all modes. ALARM_SERVICE_TAGGED_V2 and
 * ALARM_SERVICE_CLOCK_EDIT_V2 are mandatory. */
bool psr_open(PortableSettingsRuntime *,const risc_runtime_api_v1 *,
 const portable_settings_policy_v1 *,portable_realtime_phase_guard,
 void (*retain)(void *),void *);
bool psr_init_model(PortableSettingsRuntime *,PortableSettingsModel *);
bool psr_close(PortableSettingsRuntime *,const PortableSettingsModel *);
/* Explicit clean root Reload only: release namespace drafts/grants, reacquire,
 * then initialize fresh model. Never called automatically after availability. */
bool psr_reload(PortableSettingsRuntime *,PortableSettingsModel *);
bool psr_policy_valid(const portable_settings_policy_v1 *);
#endif
