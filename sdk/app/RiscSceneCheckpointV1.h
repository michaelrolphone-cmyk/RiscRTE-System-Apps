#pragma once
#include "RiscScenePageV1.h"
/* Optional scene lifecycle suffix. By default pause drains submitted work, returns any
 * mutable surface, and releases touch/navigation custody while retaining the
 * document, navigation and copied page pixels. Retry AGAIN; after OK no scene
 * servicing or mutations are permitted until resume (close is always allowed).
 * resume reacquires and neutralizes input. Zero flags do not invalidate the
 * settled visual. REDRAW requests repaint after another owner changed it;
 * CLEAN additionally requests a full clean refresh for a paged document.
 * POLICY_INPUT_CONTINUITY retains a bounded passive touch subscription and
 * copied contact/queue state plus navigation claims across same-focus policy
 * handoff. No polling occurs while paused. The host must not take focus except
 * through a real transition reported by FOCUS_CHANGED. FOCUS_CHANGED
 * explicitly neutralizes inherited input after real sleep or another UI.
 */
#define RISC_SCENE_CHECKPOINT_TAG UINT32_C(0x53435031)
#define RISC_SCENE_RESUME_REDRAW 1u
#define RISC_SCENE_RESUME_CLEAN 2u
#define RISC_SCENE_RESUME_FOCUS_CHANGED 4u
typedef struct {
 risc_scene_page_refresh_api_v1 presentation;
 uint32_t checkpoint_tag,checkpoint_version;
 int32_t (*pause)(void*,uint64_t);
 int32_t (*resume)(void*,uint64_t,uint32_t);
} risc_scene_checkpoint_api_v1;
static inline const risc_scene_checkpoint_api_v1* risc_scene_checkpoint_get_v1(const risc_scene_api_v1* base){
 if(!risc_scene_page_refresh_get_v1(base)||base->struct_size<sizeof(risc_scene_checkpoint_api_v1))return NULL;
 const risc_scene_checkpoint_api_v1* p=(const risc_scene_checkpoint_api_v1*)base;
 return p->checkpoint_tag==RISC_SCENE_CHECKPOINT_TAG&&p->checkpoint_version==1&&p->pause&&p->resume?p:NULL;
}
