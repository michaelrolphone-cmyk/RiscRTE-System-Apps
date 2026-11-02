#pragma once
#include <stdlib.h>
#include <string.h>
#include "canvas_geometry.h"
#include "RiscDisplayOutputV1.h"
#include "RiscDisplayHistoryV1.h"
/* Presentation ownership extracted from CanvasApp: logical writable image is
 * distinct from the physical submitted image. Receipts are dynamically retained
 * until explicitly forgotten, not expired by an invented time/count policy. */
/* Explicit adaptive-color intent only. The provider, not Home or a device
 * branch in an application, owns monochrome quantization. Ordered thresholds
 * are tied to output pixels and require no additional retained image. */
static inline bool canvas_rgb565_white(uint16_t pixel,uint32_t x,uint32_t y){
 static const uint8_t bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
 unsigned r=((pixel>>11)&31u)*255u/31u,g=((pixel>>5)&63u)*255u/63u,b=(pixel&31u)*255u/31u;
 unsigned luminance=(77u*r+150u*g+29u*b+128u)>>8;
 return luminance>(unsigned)bayer[(y&3u)*4u+(x&3u)]*16u+7u;
}
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
 risc_display_surface_v1 seeded;
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
 if(c->seeded.frame){
  c->display->release(c->display->context,c->seeded.frame);
  if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
  c->seeded=(risc_display_surface_v1){0};
 }
 free(c->writable);c->writable=NULL;c->lease=0;return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_poll(canvas_frames*,uint64_t,uint64_t,risc_canvas_receipt_v1*);
static inline int32_t canvas_frames_raster(canvas_frames*c,risc_display_surface_v1 s){
 uint64_t row=((uint64_t)c->mapping.physical_width*(c->format==RISC_DISPLAY_FORMAT_RGB565?16:1)+7)/8;
 if(!s.frame||!s.pixels||s.width!=c->mapping.physical_width||s.height!=c->mapping.physical_height||s.pixel_format!=c->format||s.stride_bytes<row||s.height>s.size_bytes/s.stride_bytes){c->retained=true;return RISC_CANVAS_RETAINED;}
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
  if(c->encoding==RISC_CANVAS_RGB565_ADAPTIVE){
   const unsigned char*source=c->writable+(size_t)sy*c->stride+(size_t)sx*2;
   uint16_t value=(uint16_t)(source[0]|(uint16_t)source[1]<<8);
   if(c->format==RISC_DISPLAY_FORMAT_RGB565)memcpy(p+px*2,&value,2);
   else if(!canvas_rgb565_white(value,px,py))p[px/8]|=(unsigned char)(0x80u>>(px&7));
  }else if(c->encoding==RISC_CANVAS_RGBA8){
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
 return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_submit_options(canvas_frames*c,uint64_t session,uint64_t lease,const risc_canvas_present_v1*intent,uint64_t*out){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(!out)return RISC_CANVAS_INVALID;
 if(intent&&(intent->struct_size<sizeof(*intent)||intent->intent>RISC_CANVAS_PRESENT_CLEAN||
   (intent->damage_count&&!intent->damage)))return RISC_CANVAS_INVALID;
 if(intent)for(size_t i=0;i<intent->damage_count;i++){
  const risc_canvas_rect_v1*r=intent->damage+i;
  if(r->x<0||r->y<0||!r->width||!r->height||(uint64_t)(uint32_t)r->x+r->width>c->mapping.width||
   (uint64_t)(uint32_t)r->y+r->height>c->mapping.height)return RISC_CANVAS_INVALID;
 }
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
 risc_display_surface_v1 s=c->seeded;
 bool acquired=s.frame!=0||c->display->acquire(c->display->context,c->format,&s);
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!acquired){free(n);return RISC_CANVAS_AGAIN;}
 int32_t copied=canvas_frames_raster(c,s);if(copied)return copied;
 risc_display_present_options_v1 options={(uint8_t)(intent?intent->intent:RISC_CANVAS_PRESENT_DEFAULT),RISC_DISPLAY_QUEUE_FIFO,0};uint64_t token=0;
 risc_display_rect_v1 damage={0};bool has_damage=false;
 if(intent)for(size_t i=0;i<intent->damage_count;i++){
  const risc_canvas_rect_v1*r=intent->damage+i;canvas_mapping*m=&c->mapping;
  int64_t x0=m->safe.x+m->output_x+(uint64_t)(uint32_t)r->x*m->output_width/m->width;
  int64_t y0=m->safe.y+m->output_y+(uint64_t)(uint32_t)r->y*m->output_height/m->height;
  int64_t x1=m->safe.x+m->output_x+((uint64_t)((uint32_t)r->x+r->width)*m->output_width+m->width-1)/m->width;
  int64_t y1=m->safe.y+m->output_y+((uint64_t)((uint32_t)r->y+r->height)*m->output_height+m->height-1)/m->height;
  uint32_t lw=m->rotation%2?m->physical_height:m->physical_width,lh=m->rotation%2?m->physical_width:m->physical_height;
  if(x0<0)x0=0;
  if(y0<0)y0=0;
  if(x1>lw)x1=lw;
  if(y1>lh)y1=lh;
  if(x0>=x1||y0>=y1)continue;
  uint32_t ax,ay,bx,by;canvas_mapping_physical(m,(uint32_t)x0,(uint32_t)y0,&ax,&ay);canvas_mapping_physical(m,(uint32_t)x1-1,(uint32_t)y1-1,&bx,&by);
  uint32_t left=ax<bx?ax:bx,top=ay<by?ay:by,right=(ax>bx?ax:bx)+1,bottom=(ay>by?ay:by)+1;
  if(has_damage){
   if((uint32_t)damage.x<left)left=(uint32_t)damage.x;
   if((uint32_t)damage.y<top)top=(uint32_t)damage.y;
   if((uint32_t)damage.x+damage.width>right)right=(uint32_t)damage.x+damage.width;
   if((uint32_t)damage.y+damage.height>bottom)bottom=(uint32_t)damage.y+damage.height;
  }
  damage=(risc_display_rect_v1){(int32_t)left,(int32_t)top,right-left,bottom-top};has_damage=true;
 }
 bool submitted=c->display->submit(c->display->context,s.frame,has_damage?&damage:NULL,has_damage?1:0,&options,&token);
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!submitted){if(c->seeded.frame){free(n);return RISC_CANVAS_AGAIN;}c->display->release(c->display->context,s.frame);if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;free(n);return RISC_CANVAS_AGAIN;}
 if(!token){free(n);c->retained=true;return RISC_CANVAS_RETAINED;}
 c->seeded=(risc_display_surface_v1){0};
 n->id=++c->serial;n->token=token;n->state=RISC_CANVAS_QUEUED;n->next=c->receipts;c->receipts=n;
 free(c->writable);c->writable=NULL;c->lease=0;*out=n->id;return RISC_CANVAS_OK;
}
static inline int32_t canvas_frames_submit(canvas_frames*c,uint64_t session,uint64_t lease,uint64_t*out){
 return canvas_frames_submit_options(c,session,lease,NULL,out);
}
static inline int32_t canvas_frames_seed(canvas_frames*c,uint64_t session,uint64_t lease){
 int32_t rc=canvas_frames_fence(c,session);if(rc)return rc;
 if(!lease||lease!=c->lease||c->lease_revision!=c->revision)return RISC_CANVAS_STALE;
 if(c->quiesced||c->seeded.frame)return RISC_CANVAS_BUSY;
 const risc_display_output_api_v1_history*h=risc_display_output_history(c->display);
 if(!h)return RISC_CANVAS_UNSUPPORTED;
 for(canvas_receipt_node*n=c->receipts;n;n=n->next){
  risc_canvas_receipt_v1 receipt={.struct_size=sizeof(receipt)};
  rc=canvas_frames_poll(c,session,n->id,&receipt);if(rc)return rc;
  if(receipt.state==RISC_CANVAS_QUEUED||receipt.state==RISC_CANVAS_PRESENTING)return RISC_CANVAS_BUSY;
 }
 bool acquired=c->display->acquire(c->display->context,c->format,&c->seeded);
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!acquired)return RISC_CANVAS_AGAIN;
 rc=canvas_frames_raster(c,c->seeded);if(rc)return rc;
 bool seeded=h->seed_previous(c->display->context,c->seeded.frame);
 if(!canvas_frames_alive(c)||!seeded){c->retained=true;return RISC_CANVAS_RETAINED;}
 return RISC_CANVAS_OK;
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
