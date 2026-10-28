#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Content coordinates only. The provider owns the unchanged physical raster. */
typedef struct {uint32_t x,y,width,height;} portable_safe_viewport;
static inline bool portable_safe_viewport_open(portable_safe_viewport*out,uint32_t width,uint32_t height,uint32_t left,uint32_t top,uint32_t right,uint32_t bottom){
 if(!out||!width||!height||left>=width||right>=width-left||top>=height||bottom>=height-top)return false;
 *out=(portable_safe_viewport){left,top,width-left-right,height-top-bottom};return true;
}
static inline bool portable_safe_viewport_touch(const portable_safe_viewport*v,uint16_t*x,uint16_t*y){
 if(!v||!x||!y||*x<v->x||*y<v->y||*x-v->x>=v->width||*y-v->y>=v->height)return false;
 *x=(uint16_t)(*x-v->x);*y=(uint16_t)(*y-v->y);return true;
}
