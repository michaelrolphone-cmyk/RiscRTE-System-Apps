#pragma once
#include "InstalledAppsV1.h"
#include "RiscRuntimeV1.h"
#include "T5AppApi.h"
/* One owner invocation. A live scan borrows the SD grant until cancel confirms
 * cleanup; callers keep polling while displaying their current RAM snapshot. */
typedef struct {
 risc_runtime_capability_v1 service_grant,volume_grant;
 const installed_apps_v1 *service;
 t5_app_manifest_t rows[INSTALLED_APPS_MAX];
 unsigned count;uint32_t generation;
 bool requested,closing,retained,wanted,resume_requested;
 bool maintenance_backoff,maintenance_clock,maintenance_enabled;
 uint32_t maintenance_retry_at;unsigned maintenance_retry_polls;
 bool rescan_clock;uint32_t rescan_at;
} portable_installed_catalog;
int portable_installed_refresh(portable_installed_catalog*,const risc_runtime_api_v1*,const t5_app_manifest_t*,unsigned);
int portable_installed_step(portable_installed_catalog*,const risc_runtime_api_v1*,uint64_t,const t5_app_manifest_t*,unsigned);
void portable_installed_resume(portable_installed_catalog*);
int portable_installed_close(portable_installed_catalog*,const risc_runtime_api_v1*);
