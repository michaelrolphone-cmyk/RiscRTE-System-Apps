#pragma once
#include <stdbool.h>
/* Linked renderer extension, not a display-driver ABI. Layers contain opaque
 * pixels in the current display format/orientation. Begin captures ordinary
 * drawing calls into a NEW layer; end seals it. Never nest captures. A cache
 * hit skips those drawing calls and blits a logical source rectangle instead.
 * Recorded frames retain their layers until replay completes, so replacing or
 * releasing a cache cannot mutate a pending image. Unsupported/OOM begin=false
 * leaves normal drawing available. Release caches when their scene exits. */
typedef struct portable_raster_layer portable_raster_layer;
extern bool portable_raster_layer_valid(const portable_raster_layer *) __attribute__((weak));
extern bool portable_raster_layer_begin(portable_raster_layer **) __attribute__((weak));
extern void portable_raster_layer_end(void) __attribute__((weak));
extern void portable_raster_layer_blit(portable_raster_layer *,int,int,int,int,int,int) __attribute__((weak));
extern void portable_raster_layer_release(portable_raster_layer **) __attribute__((weak));
static inline bool portable_layer_valid(const portable_raster_layer *p) {
 return portable_raster_layer_valid && portable_raster_layer_valid(p);
}
static inline bool portable_layer_begin(portable_raster_layer **p) {
 return portable_raster_layer_begin && portable_raster_layer_begin(p);
}
static inline void portable_layer_release(portable_raster_layer **p) {
 if(portable_raster_layer_release)portable_raster_layer_release(p);
}
