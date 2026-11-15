#pragma once
#include "RiscSceneTelemetryV1.h"
#include "RiscCanvasV1.h"
#define RISC_SCENE_CANVAS_TAG UINT32_C(0x53435631)
/* Fixed append-only offset. Compact presenters leave unsupported telemetry
 * tag/version/function zero; every pre-existing prefix keeps its layout. */
typedef struct {
 risc_scene_telemetry_api_v1 scene;
 uint32_t canvas_tag,canvas_version;
 const risc_canvas_api_v1 *canvas;
} risc_scene_canvas_api_v1;
static inline const risc_canvas_api_v1*risc_scene_canvas_get_v1(const risc_scene_api_v1*b){
 if(!risc_scene_reading_links_get_v1(b)||b->struct_size<sizeof(risc_scene_canvas_api_v1))return NULL;
 const risc_scene_canvas_api_v1*x=(const void*)b;
 if(x->canvas_tag!=RISC_SCENE_CANVAS_TAG||x->canvas_version!=1||!x->canvas)return NULL;
 return risc_canvas_get_v1(x->canvas,x->canvas->struct_size);
}
/* The ui.scene grant owns this borrowed API for its entire lifetime. Scene and
 * canvas sessions are mutually exclusive, including paused/quiesced sessions.
 * Close successfully before switching; RETAINED fences both interfaces. */
