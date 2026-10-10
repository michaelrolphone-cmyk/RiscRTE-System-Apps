#pragma once
#include "RiscSceneComponentsV1.h"
/* Reusable paged-document viewport, independent of the document format.
 * One means white in the caller's MSB-first MONO1 bitmap. Both the description
 * and pixels are copied before return; no caller pointer survives a call.
 * All dimensions come from geometry(). Shell/profile owns screen orientation.
 * Revisions share the same increasing sequence as component documents.
 */
#define RISC_SCENE_PAGE_TAG UINT32_C(0x50414731)
typedef struct {uint32_t struct_size,width,height,stride;} risc_scene_page_geometry_v1;
typedef struct {
 uint32_t struct_size,revision,previous_action,menu_action,next_action,back_action;
 char title[RISC_SCENE_TEXT],footer[RISC_SCENE_TEXT];
} risc_scene_page_document_v1;
typedef struct {
 risc_scene_components_api_v1 components;
 uint32_t page_tag,page_version;
 int32_t (*geometry)(void*,risc_scene_page_geometry_v1*);
 int32_t (*present_page)(void*,uint64_t,const risc_scene_page_document_v1*,const uint8_t*,size_t);
} risc_scene_page_api_v1;
static inline const risc_scene_page_api_v1* risc_scene_page_get_v1(const risc_scene_api_v1* base){
 if(!risc_scene_components_get_v1(base)||base->struct_size<sizeof(risc_scene_page_api_v1))return NULL;
 const risc_scene_page_api_v1* p=(const risc_scene_page_api_v1*)base;
 return p->page_tag==RISC_SCENE_PAGE_TAG&&p->page_version==1&&p->geometry&&p->present_page?p:NULL;
}
