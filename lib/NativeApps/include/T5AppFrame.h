#pragma once
#include "T5AppApi.h"
/* App-local compatibility helpers. Old backends retain their synchronous
 * presentation contract; new controllers never gate logical state here. */
static inline bool t5_app_frame_ready(const t5_app_api_v1 *api) {
 return api && (api->struct_size<offsetof(t5_app_api_v1,frame_ready)+sizeof(api->frame_ready) ||
                !api->frame_ready || api->frame_ready());
}
static inline bool t5_app_frame_drain(const t5_app_api_v1 *api) {
 return api && (api->struct_size<offsetof(t5_app_api_v1,frame_drain)+sizeof(api->frame_drain) ||
                !api->frame_drain || api->frame_drain());
}
