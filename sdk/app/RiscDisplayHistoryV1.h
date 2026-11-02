#pragma once
#include "RiscDisplayOutputV1.h"
/* Existing provider-owned retained-image ABI, identical to Reader SDK
 * c98e4222048244b1e729869ed47aa99130025fd3 RiscDisplayOutputV1.h suffix.
 * The same native frame must be subsequently submitted; no refresh on seed. */
#ifndef RISC_DISPLAY_HISTORY_TAG
#define RISC_DISPLAY_HISTORY_TAG 0x44485331u
typedef struct {
 risc_display_output_api_v1 base;
 uint32_t extension_tag,extension_version;
 bool (*seed_previous)(void*,risc_display_frame_v1);
} risc_display_output_api_v1_history;
static inline const risc_display_output_api_v1_history*risc_display_output_history(const risc_display_output_api_v1*api){
 if(!api||api->api_version!=1||api->struct_size<sizeof(risc_display_output_api_v1_history))return NULL;
 const risc_display_output_api_v1_history*ext=(const risc_display_output_api_v1_history*)api;
 return ext->extension_tag==RISC_DISPLAY_HISTORY_TAG&&ext->extension_version==1&&ext->seed_previous?ext:NULL;
}
#endif
