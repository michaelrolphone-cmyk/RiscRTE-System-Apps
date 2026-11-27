#pragma once
#include "canvas_frames.h"
/* Exact selected native snapshot suffix: Reader c98e422 history/power and
 * X4 6b7d1abe minimal/interfaces metrics/snapshot. Local names avoid redefining
 * those separately staged canonical headers. No intermediate capability is
 * exposed to an application through this adapter. */
typedef struct {
 risc_display_output_api_v1_history history;
 uint32_t power_tag,power_version;
 int32_t (*prepare)(void*,uint32_t);
 int32_t (*resume)(void*,uint32_t);
 uint32_t metrics_tag,metrics_version;
 bool (*metrics_snapshot)(void*,void*);
 uint32_t snapshot_tag,snapshot_version;
 bool (*copy_completed)(void*,uint32_t,void*,size_t,uint32_t);
} canvas_native_snapshot;
static inline const canvas_native_snapshot*canvas_snapshot_api(const risc_display_output_api_v1*d){
 if(!risc_display_output_history(d)||d->struct_size<sizeof(canvas_native_snapshot))return NULL;
 const canvas_native_snapshot*x=(const canvas_native_snapshot*)d;
 return x->power_tag==UINT32_C(0x44505731)&&x->power_version==1&&x->prepare&&x->resume&&
 x->metrics_tag==UINT32_C(0x444d5431)&&x->metrics_version==1&&x->metrics_snapshot&&
 x->snapshot_tag==UINT32_C(0x44534e31)&&x->snapshot_version==1&&x->copy_completed?x:NULL;
}
static inline int32_t canvas_frames_copy_completed(canvas_frames*c,uint64_t session,uint32_t revision,void*out,size_t capacity,uint32_t stride){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(c->lease||c->receipts||c->seeded.frame||c->quiesced)return RISC_CANVAS_BUSY;
 if(revision!=c->revision)return RISC_CANVAS_STALE;
 uint64_t row=c->encoding==RISC_CANVAS_MONO1?((uint64_t)c->mapping.width+7)/8:
  (uint64_t)c->mapping.width*(c->encoding==RISC_CANVAS_RGBA8?4:2);
 if(!out||stride<row||c->mapping.height>capacity/stride)return RISC_CANVAS_INVALID;
 const canvas_native_snapshot*x=canvas_snapshot_api(c->display);
 if(!x||c->format!=RISC_DISPLAY_FORMAT_MONO1)return RISC_CANVAS_UNSUPPORTED;
 const canvas_mapping*m=&c->mapping;
 uint64_t native_stride=((uint64_t)m->physical_width+7)/8;
 if(!native_stride||m->physical_height>SIZE_MAX/native_stride||m->height>SIZE_MAX/row)return RISC_CANVAS_UNAVAILABLE;
 unsigned char*native=malloc((size_t)native_stride*m->physical_height),*logical=malloc((size_t)row*m->height);
 if(!native||!logical){free(native);free(logical);return RISC_CANVAS_UNAVAILABLE;}
 bool copied=x->copy_completed(c->display->context,RISC_DISPLAY_FORMAT_MONO1,native,(size_t)native_stride*m->physical_height,(uint32_t)native_stride);
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED; /* custody pins allocations */
 if(!copied){free(native);free(logical);return RISC_CANVAS_UNAVAILABLE;}
 memset(logical,255,(size_t)row*m->height);
 for(uint32_t y=0;y<m->height;y++)for(uint32_t xx=0;xx<m->width;xx++){
  int64_t sx=(int64_t)m->safe.x+m->output_x+(int64_t)((uint64_t)xx*m->output_width/m->width);
  int64_t sy=(int64_t)m->safe.y+m->output_y+(int64_t)((uint64_t)y*m->output_height/m->height);
  bool white=true;
  if(sx>=(int64_t)m->safe.x&&sy>=(int64_t)m->safe.y&&sx<(int64_t)m->safe.x+m->safe.width&&sy<(int64_t)m->safe.y+m->safe.height){
   uint32_t px,py;canvas_mapping_physical(m,(uint32_t)sx,(uint32_t)sy,&px,&py);
   white=!(native[(size_t)py*native_stride+px/8]&(0x80u>>(px&7)));
  }
  unsigned char*b=logical+(size_t)y*row;
  if(c->encoding==RISC_CANVAS_MONO1){if(!white)b[xx/8]&=(unsigned char)~(0x80u>>(xx&7));}
  else {unsigned bytes=c->encoding==RISC_CANVAS_RGBA8?4:2;for(unsigned k=0;k<bytes;k++)b[xx*bytes+k]=(k==3||white)?255:0;}
 }
 for(uint32_t y=0;y<m->height;y++)memcpy((unsigned char*)out+(size_t)y*stride,logical+(size_t)y*row,(size_t)row);
 free(native);free(logical);return RISC_CANVAS_OK;
}
