#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Bounds use half-open pixel rectangles and doubled pixel-center coordinates.
 * The advertised visible circle is centered in the physical raster, with
 * diameter min(width,height). A margin is a radial inset, never a square. */
typedef struct { int32_t left,right; } portable_round_span;
static inline bool portable_round_contains(uint32_t width,uint32_t height,
                                           int32_t x,int32_t y,uint32_t margin) {
 uint32_t diameter=width<height?width:height;
 if(!diameter||2*(uint64_t)margin>=diameter||x<0||y<0||(uint32_t)x>=width||(uint32_t)y>=height)return false;
 int64_t dx=2*(int64_t)x+1-width,dy=2*(int64_t)y+1-height,r=(int64_t)diameter-2*margin;
 return dx*dx+dy*dy<=r*r;
}
static inline bool portable_round_band(uint32_t width,uint32_t height,
                                       int32_t top,int32_t bottom,uint32_t margin,
                                       portable_round_span*out) {
 if(!out||top<0||bottom<=top||(uint32_t)bottom>height)return false;
 /* A circle's horizontal chord is concave: the intersection over a vertical
  * band is determined by its endpoint furthest from the circle center. */
 int64_t a=2*(int64_t)top+1-height,b=2*(int64_t)(bottom-1)+1-height;
 int32_t y=a*a>b*b?top:bottom-1;
 int32_t mid=(int32_t)(width/2);
 if(!portable_round_contains(width,height,mid,y,margin))return false;
 int32_t lo=0,hi=mid;
 while(lo<hi){int32_t m=lo+(hi-lo)/2;
  if(portable_round_contains(width,height,m,y,margin))hi=m;else lo=m+1;}
 out->left=lo;out->right=(int32_t)width-lo;
 return out->right>out->left;
}
