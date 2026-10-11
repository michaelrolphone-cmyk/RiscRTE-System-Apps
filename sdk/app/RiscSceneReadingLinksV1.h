#pragma once
#include "RiscSceneReadingV1.h"
/* Optional suffix: immutable link hit regions in reading-bitmap coordinates.
 * The client owns link resolution; the provider owns touch/chrome arbitration.
 * No URI, app pointer or callback is retained by the provider. */
#define RISC_SCENE_READING_LINKS_TAG UINT32_C(0x524c4e31)
#define RISC_SCENE_READING_MAX_LINKS 64u
#define RISC_SCENE_READING_LINK_NODE 9u
typedef struct { uint16_t x,y,width,height; } risc_scene_reading_link_v1;
typedef struct {
 uint32_t struct_size,action,count,reserved;
 risc_scene_reading_link_v1 items[RISC_SCENE_READING_MAX_LINKS];
} risc_scene_reading_links_v1;
typedef struct {
 risc_scene_reading_api_v1 reading;
 uint32_t links_tag,links_version;
 /* One revision atomically copies pixels, chrome and link regions. NULL pixels
  * reuse the retained bitmap; regions are still copied. VALUE events use node
  * 9 and value 0..count-1, bound to that document revision. */
 int32_t (*present_linked)(void*,uint64_t,const risc_scene_reading_document_v1*,
                         const risc_scene_reading_links_v1*,const uint8_t*,size_t,uint32_t);
} risc_scene_reading_links_api_v1;
static inline const risc_scene_reading_links_api_v1* risc_scene_reading_links_get_v1(const risc_scene_api_v1* base){
 if(!risc_scene_reading_get_v1(base)||base->struct_size<sizeof(risc_scene_reading_links_api_v1))return NULL;
 const risc_scene_reading_links_api_v1* p=(const risc_scene_reading_links_api_v1*)base;
 return p->links_tag==RISC_SCENE_READING_LINKS_TAG&&p->links_version==1&&p->present_linked?p:NULL;
}
