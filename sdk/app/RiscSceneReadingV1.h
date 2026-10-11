#pragma once
#include "RiscSceneCheckpointV1.h"
/* Optional immersive document presentation. The checkpoint and all earlier
 * API prefixes remain byte-for-byte compatible. The provider owns geometry,
 * chrome, icons, gestures, hit testing, and immutable raster snapshots.
 * A null bitmap with length zero updates controls around the retained pixels;
 * this never asks the client to reopen storage or render its document again.
 */
#define RISC_SCENE_READING_TAG UINT32_C(0x52444731)
enum {
 RISC_SCENE_READING_CHROME=1u, RISC_SCENE_READING_INVERT=2u,
 RISC_SCENE_READING_BOOKMARKED=4u, RISC_SCENE_READING_SCROLL=8u
};
typedef struct {
 uint32_t struct_size,flags;
 risc_scene_page_document_v1 base;
 uint32_t bookmark_action,layout_action,contents_action,scrub_action,scroll_action;
 uint32_t progress; /* 0..1000. Scrubber emits the midpoint of a tenth. */
 char progress_text[RISC_SCENE_TEXT];
} risc_scene_reading_document_v1;
typedef struct {
 risc_scene_checkpoint_api_v1 checkpoint;
 uint32_t reading_tag,reading_version;
 int32_t (*reading_geometry)(void*,risc_scene_page_geometry_v1*);
 int32_t (*present_reading)(void*,uint64_t,const risc_scene_reading_document_v1*,const uint8_t*,size_t,uint32_t);
 /* A copied raster text sample inside a semantic TEXT_PREVIEW card. Geometry
  * is host-owned; font selection/shaping stays with the document client. */
 int32_t (*preview_geometry)(void*,risc_scene_page_geometry_v1*);
 int32_t (*present_preview)(void*,uint64_t,const risc_components_document_v1*,uint32_t,const uint8_t*,size_t);
} risc_scene_reading_api_v1;
/* Scroll VALUE events encode a signed logical-display-pixel displacement as
 * value-1024. They are emitted once at gesture release, never at panel settling.
 * The component API remains available for library, settings and other screens.
 */
static inline const risc_scene_reading_api_v1* risc_scene_reading_get_v1(const risc_scene_api_v1* base){
 if(!risc_scene_checkpoint_get_v1(base)||base->struct_size<sizeof(risc_scene_reading_api_v1))return NULL;
 const risc_scene_reading_api_v1* p=(const risc_scene_reading_api_v1*)base;
 return p->reading_tag==RISC_SCENE_READING_TAG&&p->reading_version==1&&p->reading_geometry&&p->present_reading&&p->preview_geometry&&p->present_preview?p:NULL;
}
