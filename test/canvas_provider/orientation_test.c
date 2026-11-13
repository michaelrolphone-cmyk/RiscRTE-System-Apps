#include <assert.h>
#include <stdio.h>
#include "canvas_provider.h"
static unsigned width=7,height=11;
static unsigned char pixels[1024];
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=width,.height=height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool acquire(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;*out=(risc_display_surface_v1){1,pixels,width,height,width*2,width*height*2,f};return true;}
static void release(void*c,uint64_t f){(void)c;(void)f;}
static bool submit(void*c,uint64_t f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)f;(void)d;(void)n;(void)o;*t=1;return true;}
static bool status(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;(void)t;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
int main(void){
 for(unsigned base=0;base<4;base++)for(unsigned shape=0;shape<3;shape++){
 width=shape==0?7:shape==1?11:9;height=shape==0?11:shape==1?7:9;
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&d,NULL,base);
 const risc_canvas_presentation_v1*x=NULL;risc_canvas_open_presentation_v1 q={.base={sizeof(q),RISC_CANVAS_RGB565_ADAPTIVE,RISC_CANVAS_RESPONSIVE,0,0},.presentation_tag=RISC_CANVAS_PRESENTATION_TAG,.presentation_out=&x};
 uint64_t session;assert(!p.api.open(&p,&q.base,&session));assert(risc_canvas_has_orientation(x));
 for(unsigned orientation=0;orientation<3;orientation++)for(unsigned reverse=0;reverse<2;reverse++){
 assert(!x->orientation(&p,session,orientation,reverse));
 risc_canvas_geometry_v1 g={.struct_size=sizeof(g)};assert(!p.api.geometry(&p,session,&g));
 if(orientation==1)assert(g.width<=g.height);
 if(orientation==2)assert(g.width>=g.height);
 unsigned rotation=base;unsigned bw=base%2?height:width,bh=base%2?width:height;
 if((orientation==1&&bw>bh)||(orientation==2&&bw<bh))rotation=(rotation+1)%4;
 rotation=(rotation+2*reverse)%4;
 assert(p.frames.mapping.rotation==rotation);
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};assert(!p.api.begin_frame(&p,session,&f));
 assert(x->orientation(&p,session,0,0)==RISC_CANVAS_BUSY);
 for(unsigned y=0;y<f.height;y++)for(unsigned xx=0;xx<f.width;xx++){uint16_t value=(uint16_t)(xx+y*256);unsigned char*b=(unsigned char*)f.pixels+y*f.stride+xx*2;b[0]=(unsigned char)value;b[1]=(unsigned char)(value>>8);}
 uint64_t receipt;assert(!p.api.submit(&p,session,f.lease,&receipt));assert(x->orientation(&p,session,0,0)==RISC_CANVAS_BUSY);
 for(unsigned y=0;y<f.height;y++)for(unsigned xx=0;xx<f.width;xx++){unsigned px,py;canvas_mapping_physical(&p.frames.mapping,xx,y,&px,&py);unsigned off=py*width*2+px*2;assert((unsigned)(pixels[off]|pixels[off+1]<<8)==xx+y*256);}
 risc_canvas_receipt_v1 r={.struct_size=sizeof(r)};assert(!p.api.poll(&p,session,receipt,&r));assert(!p.api.forget_receipt(&p,session,receipt));
 assert(p.input_gap);int32_t lx,ly;unsigned px,py;assert(canvas_provider_touch_point(&p,0,0,bw,bh,&px,&py));assert(canvas_mapping_contact(&p.frames.mapping,px,py,width,height,&lx,&ly));
 unsigned expected_px,expected_py;canvas_mapping original=p.frames.mapping;original.rotation=base;canvas_mapping_physical(&original,0,0,&expected_px,&expected_py);assert(px==expected_px&&py==expected_py);
 assert(lx>=0&&ly>=0&&(unsigned)lx<g.width&&(unsigned)ly<g.height);
 }
 assert(x->orientation(&p,session,3,0)==RISC_CANVAS_INVALID);assert(x->orientation(&p,session,0,2)==RISC_CANVAS_INVALID);assert(!p.api.close(&p,session));
 }
 puts("Canvas semantic orientation:72 shape/base/orientation/reverse cases, exact raster transform, physical touch basis, idle-only mutation and input gaps PASS");
}
