#pragma once
/* Optional, copied semantic pointer surface. ui.scene owns geometry, raw input,
 * contact identity, gesture recognition, rendering and physical lifecycle. */
#include "RiscSceneHoldV1.h"
#define RISC_SCENE_POINTER_TAG UINT32_C(0x50545231)
#define RISC_COMPONENT_POINTER (RISC_COMPONENT_HOLD_KEY + 1u)
enum { RISC_POINTER_INTENT=0, RISC_POINTER_REPORT=1, RISC_POINTER_RESET=2 };
typedef struct {
 uint32_t struct_size,kind;
 risc_scene_hold_event_v1 intent;
 int32_t dx,dy,wheel_x,wheel_y;
 uint32_t buttons,click;
} risc_scene_pointer_event_v1;
typedef struct {
 risc_scene_hold_api_v1 base;
 uint32_t pointer_tag,pointer_version;
 int32_t (*open)(void*,const risc_components_document_v1*,const risc_scene_navigation_v1*,uint64_t*);
 int32_t (*update)(void*,uint64_t,const risc_components_document_v1*);
 int32_t (*next)(void*,uint64_t,risc_scene_pointer_event_v1*);
} risc_scene_pointer_api_v1;
static inline const risc_scene_pointer_api_v1 *risc_scene_pointer_get_v1(const risc_scene_api_v1 *base){
 if(!risc_scene_hold_get_v1(base)||base->struct_size<sizeof(risc_scene_pointer_api_v1))return NULL;
 const risc_scene_pointer_api_v1 *p=(const void*)base;
 return p->pointer_tag==RISC_SCENE_POINTER_TAG&&p->pointer_version==1&&p->open&&p->update&&p->next?p:NULL;
}
/* A POINTER node has action, label and explanatory text; other numeric fields
 * are zero. At most one per route. REPORT is an ordered relative delta and
 * current button state; click requests a complete press/release pair. RESET
 * requires domain neutralization before any later scene operation.
 * INTENT carries the unmodified held-input contract, including confirmed
 * releases, ordinary scene actions and local-fault retention semantics.
 * Call next every <=20ms. Updates/pause/close first return BUSY with RESET
 * pending when a pointer gesture is live. Drain next, release domain input,
 * then retry. No contact, frame, raw coordinate or device name crosses this
 * interface. No report may be replayed after a domain connection change. */
