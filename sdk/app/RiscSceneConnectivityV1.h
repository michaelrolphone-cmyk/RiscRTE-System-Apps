#pragma once
#include "RiscSceneTelemetryV1.h"
/* Optional suffix: existing scene, reader and telemetry prefixes are unchanged.
 * NETWORK_ROW: label=SSID, text=security, badge=status, value=0..4 signal bars.
 * CONNECTION_CARD: label=network, text=status; choices=4 pipe-separated values
 * for SIGNAL/LINK SPEED/IP ADDRESS/ONLINE FOR. No network callbacks or secrets.
 * SIGNAL_HERO: label=quality, text=RSSI, value=0..4 signal bars.
 * CONNECTION_STEP: label=step, text=detail, value=0 pending, 1 active, 2 done.
 */
#define RISC_SCENE_CONNECTIVITY_TAG UINT32_C(0x434e5631)
typedef struct {
    risc_scene_telemetry_api_v1 telemetry;
    uint32_t connectivity_tag, connectivity_version;
} risc_scene_connectivity_api_v1;
static inline const risc_scene_connectivity_api_v1 *risc_scene_connectivity_get_v1(const risc_scene_api_v1 *base) {
    if (!risc_scene_telemetry_get_v1(base) || base->struct_size < sizeof(risc_scene_connectivity_api_v1)) return NULL;
    const risc_scene_connectivity_api_v1 *a=(const risc_scene_connectivity_api_v1 *)base;
    return a->connectivity_tag==RISC_SCENE_CONNECTIVITY_TAG && a->connectivity_version==1 ? a : NULL;
}
