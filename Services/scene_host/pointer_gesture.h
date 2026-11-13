#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "RiscTouchV1.h"
#define POINTER_TAP_MS 300u
#define POINTER_TAP_SLOP 8
/* Copied authoritative reports only. IDs, not array order, own a gesture.
 * Scroll rails use direction ratio with a wider exit threshold (hysteresis).
 * A diagonal start stays free XY; deliberate cross-axis motion breaks a rail.
 * Reuses the qualified HID gesture algorithm, independent of any application. */
#define POINTER_DRAG_WINDOW_MS 300u
enum { POINTER_SCROLL_PENDING, POINTER_SCROLL_X, POINTER_SCROLL_Y, POINTER_SCROLL_FREE };
typedef struct {
 bool down,valid,blocked,owned,armed,drag_candidate,dragging,scrolling,two_ended;
 uint8_t peak,id,seen,axis,break_frames;
 uint8_t ids[RISC_TOUCH_MAX_CONTACTS],pair[2];
 int sx[RISC_TOUCH_MAX_CONTACTS],sy[RISC_TOUCH_MAX_CONTACTS];
 int x,y,tap_x,tap_y,pair_x,pair_y,origin_x,origin_y;
 int residual_x,residual_y,average_x,average_y,cross_motion;
 uint32_t began,tap_at;
} pointer_gesture;
typedef struct { int8_t dx,dy,wheel_x,wheel_y; uint8_t click,buttons; bool button_changed; } pointer_gesture_output;
static inline int pointer_abs(int v){return v<0?-v:v;}
static inline int8_t pointer_clamp(int v){return (int8_t)(v<-127?-127:v>127?127:v);}
static inline void pointer_gesture_reset(pointer_gesture*g,bool blocked){memset(g,0,sizeof(*g));g->blocked=blocked;}
static inline pointer_gesture_output pointer_gesture_cancel(pointer_gesture*g){
 pointer_gesture_output o={.button_changed=g->dragging};pointer_gesture_reset(g,true);return o;
}
static inline int8_t pointer_scroll_step(int *residual,int delta,int divisor){
 *residual+=delta;int steps=*residual/divisor;int8_t out=pointer_clamp(steps);
 /* Bound the remainder; a report jump cannot queue unbounded later motion. */
 *residual=steps==out?*residual-out*divisor:0;return out;
}
static inline pointer_gesture_output pointer_gesture_sample_bounds(pointer_gesture*g,const risc_touch_snapshot_v1*s,uint32_t now,int left,int top,int right,int bottom){
#define POINTER_INSIDE(x,y) ((x)>=left&&(x)<right&&(y)>=top&&(y)<bottom)
 pointer_gesture_output o={0};unsigned n=s->contact_count;
 const int scale=s->width>=480?2:1,slop=POINTER_TAP_SLOP*scale;
 if(n>2)return pointer_gesture_cancel(g);
 if(g->blocked){if(!n)g->blocked=false;return o;}
 if(!n){
  if(g->dragging)o.button_changed=true;
  else if(g->down&&g->valid&&!g->scrolling&&(uint32_t)(now-g->began)<=POINTER_TAP_MS)o.click=g->peak==2?2:1;
  const int x=g->sx[0],y=g->sy[0];bool arm=o.click==1;
  /* Preserve the inter-tap timer across repeated neutral controller reports. */
  if(!g->down){if(g->armed&&(uint32_t)(now-g->tap_at)>POINTER_DRAG_WINDOW_MS)g->armed=false;return o;}
  pointer_gesture_reset(g,false);if(arm){g->armed=true;g->tap_at=now;g->tap_x=x;g->tap_y=y;}return o;
 }
 const risc_touch_contact_v1*c=s->contacts;
 for(unsigned i=0;i<n;i++){
  if(!POINTER_INSIDE(c[i].x,c[i].y))return pointer_gesture_cancel(g);
  for(unsigned j=0;j<i;j++)if(c[i].id==c[j].id)return pointer_gesture_cancel(g);
 }
 if(!g->down){
  bool drag=n==1&&g->armed&&(uint32_t)(now-g->tap_at)<=POINTER_DRAG_WINDOW_MS&&
    pointer_abs((int)c[0].x-g->tap_x)<=3*slop&&pointer_abs((int)c[0].y-g->tap_y)<=3*slop;
  pointer_gesture_reset(g,false);g->down=g->valid=g->owned=true;g->drag_candidate=drag;
  g->began=now;g->peak=(uint8_t)n;g->id=c[0].id;g->x=c[0].x;g->y=c[0].y;
 }
 for(unsigned i=0;i<n;i++){
  unsigned j=0;while(j<g->seen&&g->ids[j]!=c[i].id)j++;
  if(j==g->seen){
   if(j==2 || (g->seen&&g->peak==1&&n==1))return pointer_gesture_cancel(g);
   g->ids[j]=c[i].id;g->sx[j]=c[i].x;g->sy[j]=c[i].y;g->seen++;
  }
  if(pointer_abs((int)c[i].x-g->sx[j])>slop||pointer_abs((int)c[i].y-g->sy[j])>slop)g->valid=false;
 }
 if(n==2){
  if(g->dragging||g->two_ended)return pointer_gesture_cancel(g);
  g->drag_candidate=false;
  const uint8_t a=c[0].id<c[1].id?c[0].id:c[1].id,b=c[0].id<c[1].id?c[1].id:c[0].id;
  const int x=(int)c[0].x+c[1].x,y=(int)c[0].y+c[1].y; /* doubled centroid */
  if(g->peak<2 || (!g->pair[0]&&!g->pair[1])){
   g->pair[0]=a;g->pair[1]=b;g->origin_x=g->pair_x=x;g->origin_y=g->pair_y=y;
  }else if(g->pair[0]!=a||g->pair[1]!=b)return pointer_gesture_cancel(g);
  int dx=x-g->pair_x,dy=y-g->pair_y;g->pair_x=x;g->pair_y=y;
  if(!g->scrolling){
   const int tx=x-g->origin_x,ty=y-g->origin_y,ax=pointer_abs(tx),ay=pointer_abs(ty);
   if(ax>2*slop||ay>2*slop){
    g->scrolling=true;g->valid=false;g->axis=ax>=2*ay?POINTER_SCROLL_X:ay>=2*ax?POINTER_SCROLL_Y:POINTER_SCROLL_FREE;
    dx=tx;dy=ty;
   }
  }
  if(g->scrolling){
   /* Q4 decayed absolute movement preserves slow motion and does not treat
    * a sign reversal as stationary. */
   g->average_x=(3*g->average_x+16*pointer_abs(dx))/4;g->average_y=(3*g->average_y+16*pointer_abs(dy))/4;
   if(g->axis==POINTER_SCROLL_X||g->axis==POINTER_SCROLL_Y){
    const int cross=g->axis==POINTER_SCROLL_X?dy:dx;
    const int cross_avg=g->axis==POINTER_SCROLL_X?g->average_y:g->average_x;
    const int along_avg=g->axis==POINTER_SCROLL_X?g->average_x:g->average_y;
    g->cross_motion+=cross;
    if(cross_avg>along_avg&&pointer_abs(g->cross_motion)>4*slop){if(g->break_frames<3)g->break_frames++;}
    else g->break_frames=0;
    if(g->break_frames==3){g->axis=POINTER_SCROLL_FREE;g->residual_x=g->residual_y=0;}
   }
   if(g->axis!=POINTER_SCROLL_Y)o.wheel_x=pointer_scroll_step(&g->residual_x,dx,8*scale);
   if(g->axis!=POINTER_SCROLL_X)o.wheel_y=pointer_scroll_step(&g->residual_y,-dy,8*scale);
  }
  g->peak=2;return o;
 }
 /* A lifted/replaced second finger cannot become a pointer or drag. */
 if(g->peak==2){g->two_ended=true;if(g->scrolling)g->valid=false;return o;}
 if(c[0].id!=g->id)return pointer_gesture_cancel(g);
 int dx=(int)c[0].x-g->x,dy=(int)c[0].y-g->y;
 if(g->drag_candidate&&!g->dragging){
  if(!g->valid){g->dragging=true;o.button_changed=true;dx=(int)c[0].x-g->sx[0];dy=(int)c[0].y-g->sy[0];}
  else dx=dy=0;
 }
 o.dx=pointer_clamp(dx);o.dy=pointer_clamp(dy);o.buttons=g->dragging?1:0;
 g->x=c[0].x;g->y=c[0].y;return o;
#undef POINTER_INSIDE
}
