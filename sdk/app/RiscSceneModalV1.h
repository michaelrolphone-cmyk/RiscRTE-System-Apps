#pragma once
#include "RiscSceneConnectivityV1.h"
/* One semantic foreground modal over a fully paused scene, including a paused
 * text-input scene whose token is private to its owner. Open never pauses an
 * owner implicitly. The caller first drains conflicting display/input work and accepted storage
 * receipts, then checkpoints that owner. A checked independent RF lease may
 * remain active; this provider does not perform a Runtime checkpoint. Parent tokens return BUSY while the modal is active.
 *
 * The child uses ordinary scene next/snapshot and components update. Close
 * drains display/input custody before restoring the exact parent still paused.
 * Resume the original owner with REDRAW|FOCUS_CHANGED. A real focus change
 * discards inherited input, never logical editor contents. Nested modals are
 * BUSY. RETAINED is terminal and forbids further dependency I/O.
 */
#define RISC_SCENE_MODAL_TAG UINT32_C(0x4d4f4431)
typedef struct {
    risc_scene_connectivity_api_v1 connectivity;
    uint32_t modal_tag,modal_version;
    int32_t (*open)(void *,const risc_components_document_v1 *,uint64_t *);
    int32_t (*close)(void *,uint64_t);
} risc_scene_modal_api_v1;
static inline const risc_scene_modal_api_v1 *risc_scene_modal_get_v1(const risc_scene_api_v1 *base){
    if(!risc_scene_connectivity_get_v1(base)||base->struct_size<sizeof(risc_scene_modal_api_v1))return NULL;
    const risc_scene_modal_api_v1 *a=(const risc_scene_modal_api_v1 *)base;
    return a->modal_tag==RISC_SCENE_MODAL_TAG&&a->modal_version==1&&a->open&&a->close?a:NULL;
}
