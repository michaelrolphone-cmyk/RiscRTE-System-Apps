#pragma once
/* Additive ui.scene held-input contract. No geometry crosses the API. */
#include "RiscSceneCanvasV1.h"
#include "RiscSceneKeypadV1.h"
#define RISC_SCENE_HOLD_TAG UINT32_C(0x484c4431)
#define RISC_COMPONENT_HOLD_KEY (RISC_COMPONENT_KEY + 1u)
enum { RISC_HOLD_NORMAL=0, RISC_HOLD_CONFIRM=1 };
enum { RISC_HOLD_NONE=0, RISC_HOLD_PRESS=1, RISC_HOLD_RELEASE=2, RISC_HOLD_CANCEL=3 };
enum { RISC_HOLD_REASON_NONE=0, RISC_HOLD_REASON_MOTION=1,
 RISC_HOLD_REASON_AMBIGUOUS=2, RISC_HOLD_REASON_TOPOLOGY=3,
 RISC_HOLD_REASON_SERVICE_GAP=4, RISC_HOLD_REASON_TIMEOUT=5,
 RISC_HOLD_REASON_BOUNDARY=6, RISC_HOLD_REASON_LOCAL_FAULT=7 };
#define RISC_HOLD_LOCAL_FAULT_PENDING 1u
/* HOLD_KEY has a nonempty normal scene label, copied text, and action. minimum/maximum/value are zero;
 * step is NORMAL or CONFIRM. NORMAL has no contact-duration limit. CONFIRM
 * requires a physical release within 800ms and at most 8 logical pixels motion.
 * All holds cancel on ambiguity, topology change, or a service gap >100ms.
 * Call next at least every 20ms. Limits are cooperative, not autonomous timers. */
typedef struct {
 uint32_t struct_size,phase,flags,reason;
 risc_scene_event_v1 event;
 uint64_t generation,held_ms;
} risc_scene_hold_event_v1;
typedef struct {
 risc_scene_canvas_api_v1 base;
 uint32_t hold_tag,hold_version;
 int32_t (*open)(void*,const risc_components_document_v1*,const risc_scene_navigation_v1*,uint64_t*);
 int32_t (*update)(void*,uint64_t,const risc_components_document_v1*);
 int32_t (*next)(void*,uint64_t,risc_scene_hold_event_v1*);
} risc_scene_hold_api_v1;
static inline const risc_scene_hold_api_v1 *risc_scene_hold_get_v1(const risc_scene_api_v1 *b){
 if(!risc_scene_components_get_v1(b)||b->struct_size<sizeof(risc_scene_hold_api_v1))return NULL;
 const risc_scene_hold_api_v1 *a=(const void*)b;
 return a->hold_tag==RISC_SCENE_HOLD_TAG&&a->hold_version==1&&a->open&&a->update&&a->next?a:NULL;
}
/* The original scene lifecycle/snapshot/close operate on the same session.
 * A held boundary first queues CANCEL and returns BUSY: drain next, neutralize
 * the domain, then retry the boundary. NONE carries an ordinary scene event.
 * Local-fault CANCEL|LOCAL_FAULT_PENDING is delivered only with still-valid
 * Runtime/dependency custody and freezes further scene dependency I/O. Perform
 * checked domain neutralization before the next scene call, which finalizes
 * RETAINED. Native identity loss or uncertain dependency custody instead returns
 * RETAINED immediately, without a success event or permission for later I/O.
 * No remote neutralization or autonomous HID watchdog is implied. */
