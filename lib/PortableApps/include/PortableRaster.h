#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Private linked-app renderer extension. The payload is copied immediately.
 * It may contain pointers only to immutable invocation-lifetime constants.
 * The callback draws the supplied [first,last) logical rows using ordinary
 * drawing primitives. It must not access mutable model state or services.
 * False selects the caller's immediate implementation on older adapters.
 * No Runtime/provider ABI or worker-thread ownership change is involved. */
typedef void (*portable_raster_draw)(const void *payload,int first,int last);
extern bool portable_raster_defer(portable_raster_draw,const void *,size_t,int,int)
  __attribute__((weak));
static inline bool portable_raster_record_draw(portable_raster_draw draw,
 const void *payload,size_t bytes,int top,int bottom) {
 return portable_raster_defer && portable_raster_defer(draw,payload,bytes,top,bottom);
}
