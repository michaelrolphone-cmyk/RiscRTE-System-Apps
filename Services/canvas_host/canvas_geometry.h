#pragma once
/* CanvasApp's logical-to-output fit and inverse touch mapping generalized to
 * explicit content intent. Shared safe viewport owns inset validation. */
#include <limits.h>
#include <stdint.h>
#include "PortableSafeViewport.h"
#include "RiscCanvasV1.h"
typedef struct {
 portable_safe_viewport safe;
 uint32_t physical_width,physical_height,rotation,width,height;
 uint32_t output_width,output_height;
 bool wide_x,wide_y;
 int64_t output_x,output_y;
} canvas_mapping;
static inline bool canvas_mapping_open(canvas_mapping *out,uint32_t pw,uint32_t ph,
 uint32_t rotation,uint32_t left,uint32_t top,uint32_t right,uint32_t bottom,
 const risc_canvas_open_v1 *request){
 if(!out||!request||request->struct_size<sizeof(*request)||rotation>3||!pw||!ph||pw>INT32_MAX||ph>INT32_MAX)return false;
 uint32_t insets[4]={left,top,right,bottom},r[4];
 for(unsigned i=0;i<4;i++)r[i]=insets[(i+rotation)%4];
 canvas_mapping m={0};m.physical_width=pw;m.physical_height=ph;m.rotation=rotation;
 uint32_t w=rotation%2?ph:pw,h=rotation%2?pw:ph;
 if(!portable_safe_viewport_open(&m.safe,w,h,r[0],r[1],r[2],r[3]))return false;
 if(request->layout==RISC_CANVAS_RESPONSIVE){
  if(request->logical_width||request->logical_height)return false;
  m.width=m.safe.width;m.height=m.safe.height;
 }else{
  if(request->layout!=RISC_CANVAS_FIT&&request->layout!=RISC_CANVAS_FILL)return false;
  m.width=request->logical_width;m.height=request->logical_height;
  if(!m.width||!m.height||m.width>INT32_MAX||m.height>INT32_MAX)return false;
 }
 uint64_t by_width=(uint64_t)m.safe.width*m.height/m.width;
 bool use_width=request->layout==RISC_CANVAS_FILL?by_width>=m.safe.height:by_width<=m.safe.height;
 uint64_t ow=use_width?m.safe.width:(uint64_t)m.safe.height*m.width/m.height;
 uint64_t oh=use_width?by_width:m.safe.height;
 if(!ow||!oh||ow>INT32_MAX||oh>INT32_MAX)return false;
 m.output_width=(uint32_t)ow;m.output_height=(uint32_t)oh;
 m.wide_x=(ow-1)*m.width>UINT32_MAX;m.wide_y=(oh-1)*m.height>UINT32_MAX;
 m.output_x=((int64_t)m.safe.width-m.output_width)/2;
 m.output_y=((int64_t)m.safe.height-m.output_height)/2;
 *out=m;return true;
}
/* Sample content at a logical surface pixel. False is background/letterbox. */
static inline bool canvas_mapping_sample(const canvas_mapping*m,uint32_t x,uint32_t y,uint32_t*sx,uint32_t*sy){
 if(x<m->safe.x||y<m->safe.y||x-m->safe.x>=m->safe.width||y-m->safe.y>=m->safe.height)return false;
 int64_t u=(int64_t)x-m->safe.x-m->output_x,v=(int64_t)y-m->safe.y-m->output_y;
 if(u<0||v<0||u>=m->output_width||v>=m->output_height)return false;
 *sx=m->wide_x?(uint32_t)((uint64_t)u*m->width/m->output_width):((uint32_t)u*m->width)/m->output_width;*sy=m->wide_y?(uint32_t)((uint64_t)v*m->height/m->output_height):((uint32_t)v*m->height)/m->output_height;return true;
}
static inline void canvas_mapping_physical(const canvas_mapping*m,uint32_t x,uint32_t y,uint32_t*px,uint32_t*py){
 switch(m->rotation){
 case 1:*px=m->physical_width-1-y;*py=x;break;
 case 2:*px=m->physical_width-1-x;*py=m->physical_height-1-y;break;
 case 3:*px=y;*py=m->physical_height-1-x;break;
 default:*px=x;*py=y;break;
 }
}
/* Captured drags preserve coordinates outside visible content. New-contact
 * admission calls sample separately; no clamping of ongoing gestures. */
static inline bool canvas_mapping_contact(const canvas_mapping*m,uint32_t raw_x,uint32_t raw_y,uint32_t raw_w,uint32_t raw_h,int32_t*x,int32_t*y){
 if(!raw_w||!raw_h||raw_x>=raw_w||raw_y>=raw_h)return false;
 int64_t px=(uint64_t)raw_x*m->physical_width/raw_w,py=(uint64_t)raw_y*m->physical_height/raw_h,lx,ly;
 switch(m->rotation){case 1:lx=py;ly=m->physical_width-1-px;break;case 2:lx=m->physical_width-1-px;ly=m->physical_height-1-py;break;case 3:lx=m->physical_height-1-py;ly=px;break;default:lx=px;ly=py;break;}
 int64_t cx=(lx-m->safe.x-m->output_x)*m->width/m->output_width,cy=(ly-m->safe.y-m->output_y)*m->height/m->output_height;
 if(cx<INT32_MIN||cx>INT32_MAX||cy<INT32_MIN||cy>INT32_MAX)return false;
 *x=(int32_t)cx;*y=(int32_t)cy;return true;
}

/* Admission uses surface coverage, not rounded content-coordinate sign. */
static inline bool canvas_mapping_admit(const canvas_mapping*m,uint32_t rx,uint32_t ry,uint32_t rw,uint32_t rh){
 if(!rw||!rh||rx>=rw||ry>=rh)return false;
 uint32_t px=(uint64_t)rx*m->physical_width/rw,py=(uint64_t)ry*m->physical_height/rh,x,y,sx,sy;
 switch(m->rotation){case 1:x=py;y=m->physical_width-1-px;break;case 2:x=m->physical_width-1-px;y=m->physical_height-1-py;break;case 3:x=m->physical_height-1-py;y=px;break;default:x=px;y=py;break;}
 return canvas_mapping_sample(m,x,y,&sx,&sy);
}
