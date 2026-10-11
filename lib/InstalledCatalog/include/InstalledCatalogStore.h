#pragma once
#include "InstalledCatalog.h"
#include "RiscAppDataV1.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Existing app-data atomic replacement supplies sync, verified write/readback,
 * revision checks and crash-safe selection. No second journal or raw flash path.
 * Caller owns scratch storage, the live grant, and settled owner scheduling.
 * Load never scans SD. NOT_FOUND/corruption preserve the in-memory catalog.
 * COMMIT_UNKNOWN requires reload; RETAINED forbids further I/O or teardown. */
int32_t installed_catalog_load(installed_catalog*,const char*policy_key,
 const risc_app_data_v1*,void*scratch,uint32_t capacity,uint64_t*revision);
int32_t installed_catalog_save(const installed_catalog*,const char*policy_key,
 const risc_app_data_v1*,void*scratch,uint32_t capacity,uint64_t expected_revision);
#ifdef __cplusplus
}
#endif
