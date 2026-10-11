#pragma once
/* Optional suffix for the ordinary ui.scene@1 table. No Runtime changes or
 * resident policy are required. RiscSceneV1.h and its layouts stay unchanged.
 * All calls remain serialized on the grant-owning task. */
#include "RiscSceneV1.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RISC_SCENE_FEATURE_SHARED_CONTROLS 1u
/* Semantic request only: the presenter never displays shared controls.
 * node, action and value are zero; revision and sequence follow base events.
 * Deliberate top-pull thresholds and arbitration belong to the presenter.
 * Shared-controls gestures are suppressed on a keyboard scene. */
#define RISC_SCENE_CONTROLS_EVENT 4u
/* Additional snapshot flags, emitted only after successful configure().
 * ACTIVITY is latched observed input, consumed by a successful snapshot only.
 * INPUT_BUSY covers held, queued, unacknowledged or unsynchronized input;
 * it is a level and is not consumed by snapshot(). */
#define RISC_SCENE_ACTIVITY 4u
#define RISC_SCENE_INPUT_BUSY 8u
typedef struct {
    risc_scene_api_v1 base;
    /* Configure this session and consume inherited input. Zero enables
     * lifecycle tracking without shared controls. Unknown bits are INVALID;
     * unsupported tables are discovered without reading beyond their size.
     * Reconfigure is an input-context boundary, not an activity event. */
    int32_t (*configure)(void *, uint64_t, uint32_t);
} risc_scene_lifecycle_api_v1;
static inline const risc_scene_lifecycle_api_v1 *
risc_scene_lifecycle_get_v1(const risc_scene_api_v1 *base) {
    if (!base || base->api_version != RISC_SCENE_API_V1 ||
        base->struct_size < offsetof(risc_scene_lifecycle_api_v1, configure) +
                            sizeof(((risc_scene_lifecycle_api_v1 *)0)->configure))
        return NULL;
    const risc_scene_lifecycle_api_v1 *api = (const risc_scene_lifecycle_api_v1 *)base;
    return api->configure ? api : NULL;
}
#ifdef __cplusplus
}
#endif
