#pragma once
/* Compatibility fixtures now exercise the same read-only runtime boundary as
 * deployed apps. These source arrays belong solely to the fake native owner. */
/* The mock native owner uses the current capability ABI even when the app and
 * its SDK are compiled from an isolated, pre-capability baseline archive. */
#include "../../../lib/PortableApps/include/RiscAppCatalogV1.h"
#include "T5AppApi.h"
#include <string.h>
extern const t5_app_manifest_t portable_catalog[];
extern const unsigned portable_catalog_count;
static int32_t fixture_catalog_snapshot(void *context,risc_app_catalog_snapshot_v1 *out) {
    (void)context;
    *out=(risc_app_catalog_snapshot_v1){.struct_size=sizeof(*out),.count=portable_catalog_count,.generation=1};
    return RISC_APP_CATALOG_OK;
}
static int32_t fixture_catalog_get(void *context,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1 *out) {
    (void)context;
    if(generation!=1)return RISC_APP_CATALOG_STALE;
    if(index>=portable_catalog_count)return RISC_APP_CATALOG_NOT_FOUND;
    const t5_app_manifest_t *entry=&portable_catalog[index];
    *out=(risc_app_catalog_entry_v1){.struct_size=sizeof(*out),.flags=entry->compatible?RISC_APP_CATALOG_AVAILABLE:0};
    memcpy(out->display_name,entry->display_name,sizeof(out->display_name));
    strcpy(out->file_name,entry->file_name);
    /* Source fixture keys are admitted identities in this fake owner. */
    strcpy(out->id,"fixture-app");
    memcpy(out->min_firmware_version,entry->min_firmware_version,sizeof(out->min_firmware_version));
    memcpy(out->icon,entry->icon,sizeof(out->icon));
    return RISC_APP_CATALOG_OK;
}
static const risc_app_catalog_v1 fixture_catalog_api={1,sizeof(fixture_catalog_api),NULL,fixture_catalog_snapshot,fixture_catalog_get};
