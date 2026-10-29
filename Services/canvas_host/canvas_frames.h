#pragma once
#include <stdlib.h>
#include <string.h>
#include "canvas_geometry.h"
#include "RiscDisplayOutputV1.h"
/* Presentation ownership extracted from CanvasApp: logical writable image is
 * distinct from the physical submitted image. Receipts are dynamically retained
 * until explicitly forgotten, not expired by an invented time/count policy. */
typedef struct canvas_receipt_node {
 struct canvas_receipt_node *next;
 uint64_t id,token;
 uint32_t state;
} canvas_receipt_node;
typedef struct {
 const risc_display_output_api_v1 *display;
 canvas_mapping mapping;
 uint64_t serial,session,lease;
 uint32_t revision,lease_revision,encoding,stride,format;
 size_t bytes;
 unsigned char *writable;
 canvas_receipt_node *receipts;
 bool retained,quiesced;
 bool (*alive)(void*);
 bool (*cooperate)(void*);
 void *cooperate_context;
} canvas_frames;
static inline bool canvas_frames_alive(canvas_frames*c){
 if(c->retained)return false;
 if(c->alive&&!c->alive(c->cooperate_context)){c->retained=true;return false;}
 return true;
}
static inline int32_t canvas_frames_fence(canvas_frames*c,uint64_t session){
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!session||session!=c->session)return RISC_CANVAS_STALE;
 return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_begin(canvas_frames*c,uint64_t session,risc_canvas_frame_v1*out){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(!out||out->struct_size<sizeof(*out))return RISC_CANVAS_INVALID;
 if(c->quiesced||c->lease)return RISC_CANVAS_BUSY;
 if(c->serial==UINT64_MAX)return RISC_CANVAS_UNAVAILABLE;
 unsigned char *p=malloc(c->bytes);if(!p)return RISC_CANVAS_AGAIN;
 c->writable=p;c->lease=++c->serial;c->lease_revision=c->revision;
 for(size_t offset=0;offset<c->bytes;){
  if(c->cooperate){bool ok=c->cooperate(c->cooperate_context);if(!canvas_frames_alive(c)||!ok){c->retained=true;return RISC_CANVAS_RETAINED;}}
  size_t chunk=c->bytes-offset;if(chunk>65536)chunk=65536;memset(p+offset,255,chunk);offset+=chunk;
 }
 *out=(risc_canvas_frame_v1){sizeof(*out),c->revision,c->encoding,c->mapping.width,c->mapping.height,c->stride,c->lease,p,c->bytes};return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_cancel(canvas_frames*c,uint64_t session,uint64_t lease){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(!lease||lease!=c->lease)return RISC_CANVAS_STALE;
 free(c->writable);c->writable=NULL;c->lease=0;return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_poll(canvas_frames*,uint64_t,uint64_t,risc_canvas_receipt_v1*);
static inline int32_t canvas_frames_submit(canvas_frames*c,uint64_t session,uint64_t lease,uint64_t*out){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(!out)return RISC_CANVAS_INVALID;
 if(!lease||lease!=c->lease)return RISC_CANVAS_STALE;
 if(c->lease_revision!=c->revision)return RISC_CANVAS_STALE;
 if(c->quiesced)return RISC_CANVAS_BUSY;
 if(c->serial==UINT64_MAX)return RISC_CANVAS_UNAVAILABLE;
 /* Some real display providers retain only their latest token status. Cache
  * settled outcomes before allowing a later submit to replace that token. */
 for(canvas_receipt_node*old=c->receipts;old;old=old->next){
  risc_canvas_receipt_v1 receipt={.struct_size=sizeof(receipt)};
  int32_t status=canvas_frames_poll(c,session,old->id,&receipt);if(status)return status;
  if(receipt.state==RISC_CANVAS_QUEUED||receipt.state==RISC_CANVAS_PRESENTING)return RISC_CANVAS_AGAIN;
 }
 canvas_receipt_node*n=malloc(sizeof(*n));if(!n)return RISC_CANVAS_AGAIN;memset(n,0,sizeof(*n));
 risc_display_surface_v1 s={0};
 bool acquired=c->display->acquire(c->display->context,c->format,&s);
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!acquired){free(n);return RISC_CANVAS_AGAIN;}
 uint64_t row=((uint64_t)c->mapping.physical_width*(c->format==RISC_DISPLAY_FORMAT_RGB565?16:1)+7)/8;
 if(!s.frame||!s.pixels||s.width!=c->mapping.physical_width||s.height!=c->mapping.physical_height||s.pixel_format!=c->format||s.stride_bytes<row||s.height>s.size_bytes/s.stride_bytes){free(n);c->retained=true;return RISC_CANVAS_RETAINED;}
 size_t physical_bytes=(size_t)s.stride_bytes*s.height;
 for(size_t offset=0;offset<physical_bytes;){
  if(c->cooperate){bool ok=c->cooperate(c->cooperate_context);if(!canvas_frames_alive(c)||!ok){c->retained=true;return RISC_CANVAS_RETAINED;}}
  size_t chunk=physical_bytes-offset;if(chunk>65536)chunk=65536;
  memset((unsigned char*)s.pixels+offset,c->format==RISC_DISPLAY_FORMAT_RGB565?255:0,chunk);offset+=chunk;
 }
 uint32_t lw=c->mapping.rotation%2?s.height:s.width,lh=c->mapping.rotation%2?s.width:s.height;
 for(uint32_t y=0;y<lh;y++){
  if((y&15u)==0&&c->cooperate){
   bool ok=c->cooperate(c->cooperate_context);
   if(!canvas_frames_alive(c)||!ok){c->retained=true;return RISC_CANVAS_RETAINED;}
  }
  for(uint32_t x=0;x<lw;x++){
  uint32_t sx,sy,px,py;if(!canvas_mapping_sample(&c->mapping,x,y,&sx,&sy))continue;
  /* Public MONO1 one=white; physical display MONO1 one=black.
   * RGBA is source-over the provider's white canvas background in sRGB
   * channel space, then quantized only to the actual RGB565 panel format. */
  canvas_mapping_physical(&c->mapping,x,y,&px,&py);
  unsigned char*p=(unsigned char*)s.pixels+(size_t)py*s.stride_bytes;
  if(c->encoding==RISC_CANVAS_RGBA8){
   const unsigned char*rgba=c->writable+(size_t)sy*c->stride+(size_t)sx*4;
   unsigned a=rgba[3],r=(rgba[0]*a+255u*(255u-a)+127)/255,g=(rgba[1]*a+255u*(255u-a)+127)/255,b=(rgba[2]*a+255u*(255u-a)+127)/255;
   uint16_t v=(uint16_t)(((r*31u+127)/255)<<11|((g*63u+127)/255)<<5|((b*31u+127)/255));memcpy(p+px*2,&v,2);
  }else{
   bool white=(c->writable[(size_t)sy*c->stride+sx/8]&(0x80u>>(sx&7)))!=0;
   if(c->format==RISC_DISPLAY_FORMAT_RGB565){uint16_t v=white?0xffff:0;memcpy(p+px*2,&v,2);}
   else if(!white)p[px/8]|=(unsigned char)(0x80u>>(px&7));
  }
 }
 }
 risc_display_present_options_v1 options={RISC_DISPLAY_PRESENT_DEFAULT,RISC_DISPLAY_QUEUE_FIFO,0};uint64_t token=0;
 bool submitted=c->display->submit(c->display->context,s.frame,NULL,0,&options,&token);
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!submitted){c->display->release(c->display->context,s.frame);if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;free(n);return RISC_CANVAS_AGAIN;}
 if(!token){free(n);c->retained=true;return RISC_CANVAS_RETAINED;}
 n->id=++c->serial;n->token=token;n->state=RISC_CANVAS_QUEUED;n->next=c->receipts;c->receipts=n;
 free(c->writable);c->writable=NULL;c->lease=0;*out=n->id;return RISC_CANVAS_OK;
}
static inline bool canvas_receipt_settled(uint32_t state){return state==RISC_CANVAS_COMPLETE||state==RISC_CANVAS_SUPERSEDED||state==RISC_CANVAS_FAILED;}
static inline int32_t canvas_frames_poll(canvas_frames*c,uint64_t session,uint64_t id,risc_canvas_receipt_v1*out){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(!out||out->struct_size<sizeof(*out))return RISC_CANVAS_INVALID;
 canvas_receipt_node*n=c->receipts;while(n&&n->id!=id)n=n->next;if(!n)return RISC_CANVAS_STALE;
 if(!canvas_receipt_settled(n->state)){
  risc_display_present_status_v1 status={0};
  bool valid=c->display->present_status(c->display->context,n->token,&status);
  if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
  if(!valid||status.state<1||status.state>4){n->state=RISC_CANVAS_CUSTODY_RETAINED;c->retained=true;return RISC_CANVAS_RETAINED;}
  /* Only COMPLETE/SUPERSEDED prove settlement. Physical FAILED may
   * precede failed hardware stop; retain rather than invent release proof. */
  n->state=status.state;
 }
 *out=(risc_canvas_receipt_v1){sizeof(*out),n->state,n->id,0,0};return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_forget(canvas_frames*c,uint64_t session,uint64_t id){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 canvas_receipt_node**at=&c->receipts;while(*at&&(*at)->id!=id)at=&(*at)->next;
 if(!*at)return RISC_CANVAS_STALE;
 if(!canvas_receipt_settled((*at)->state))return RISC_CANVAS_BUSY;
 canvas_receipt_node*n=*at;*at=n->next;free(n);return RISC_CANVAS_OK;
}
