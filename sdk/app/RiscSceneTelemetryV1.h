#pragma once
#include "RiscSceneReadingLinksV1.h"
/* Optional copied plot attachment. Values are normalized to 0..10000, evenly
 * spaced over the stated time span; 65535 means a gap, never a zero reading.
 * Apps own units, sampling, history, scaling and alerts. The shared presenter
 * owns geometry, fonts, glyphs, e-ink policy and physical hit regions.
 * Never a callback, app pointer, device name or ECU protocol in the provider. */
#define RISC_SCENE_TELEMETRY_TAG UINT32_C(0x54454c31)
#define RISC_TELEMETRY_POINTS 151u
#define RISC_TELEMETRY_CURVES 11u
#define RISC_TELEMETRY_GAP UINT16_MAX
/* Curve style: 0 solid, 1 dashed, 2 dotted. At most three per plot or one per
 * tile. count=0 is an empty series. x spans the full visible interval. */
typedef struct { uint32_t node,style,count; uint16_t points[RISC_TELEMETRY_POINTS]; } risc_scene_curve_v1;
typedef struct { uint32_t struct_size,count; risc_scene_curve_v1 curves[RISC_TELEMETRY_CURVES]; } risc_scene_telemetry_v1;
typedef struct {
 risc_scene_reading_links_api_v1 reading;
 uint32_t telemetry_tag,telemetry_version;
 /* Uses an ordinary component session. Both document and attachment are
  * validated and copied as one revision; caller may overwrite after return. */
 int32_t (*present)(void*,uint64_t,const risc_components_document_v1*,const risc_scene_telemetry_v1*);
} risc_scene_telemetry_api_v1;
static inline const risc_scene_telemetry_api_v1 *risc_scene_telemetry_get_v1(const risc_scene_api_v1*b){
 if(!risc_scene_reading_links_get_v1(b)||b->struct_size<sizeof(risc_scene_telemetry_api_v1))return NULL;
 const risc_scene_telemetry_api_v1*p=(const risc_scene_telemetry_api_v1*)b;
 return p->telemetry_tag==RISC_SCENE_TELEMETRY_TAG&&p->telemetry_version==1&&p->present?p:NULL;
}
