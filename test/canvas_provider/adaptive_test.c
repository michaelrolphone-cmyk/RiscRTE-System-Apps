#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_provider.h"
static unsigned width,height,format,stride;
static unsigned char *allocation,*pixels;
static uint64_t sequence;
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=width,.height=height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(format)};return true;}
static bool acquire(void*c,uint32_t encoding,risc_display_surface_v1*out){(void)c;assert(encoding==format);*out=(risc_display_surface_v1){1,pixels,width,height,stride,(size_t)stride*height,format};return true;}
static void release(void*c,uint64_t id){(void)c;assert(id==1);}
static bool submit(void*c,uint64_t id,const risc_display_rect_v1*r,size_t count,const risc_display_present_options_v1*o,uint64_t*out){(void)c;(void)r;(void)count;(void)o;assert(id==1);*out=++sequence;return true;}
static bool status(void*c,uint64_t id,risc_display_present_status_v1*out){(void)c;assert(id==sequence);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint16_t color(unsigned x,unsigned y){return (uint16_t)(x+256u*y);}
static void run(unsigned w,unsigned h,unsigned output,unsigned rotation){
 width=w;height=h;format=output;stride=(format==RISC_DISPLAY_FORMAT_RGB565?width*2:(width+7)/8)+3;
 size_t bytes=(size_t)stride*height;allocation=malloc(bytes+32);assert(allocation);memset(allocation,0xa5,bytes+32);pixels=allocation+16;
 risc_display_output_api_v1 display={.api_version=1,.struct_size=sizeof(display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&display,NULL,rotation);const risc_canvas_api_v1*a=&p.api;
 risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_RGBA8,RISC_CANVAS_RESPONSIVE,0,0};uint64_t session=0;
 if(output==RISC_DISPLAY_FORMAT_MONO1){assert(a->open(a->context,&request,&session)==RISC_CANVAS_UNSUPPORTED);assert(!session);}
 request.encoding=RISC_CANVAS_RGB565_ADAPTIVE;assert(!a->open(a->context,&request,&session));
 risc_canvas_frame_v1 frame={.struct_size=sizeof(frame)};assert(!a->begin_frame(a->context,session,&frame));
 assert(frame.encoding==RISC_CANVAS_RGB565_ADAPTIVE&&frame.stride==frame.width*2&&frame.byte_count==(size_t)frame.stride*frame.height);
 for(unsigned y=0;y<frame.height;y++)for(unsigned x=0;x<frame.width;x++){uint16_t v=color(x,y);unsigned char*b=(unsigned char*)frame.pixels+(size_t)y*frame.stride+x*2;b[0]=(unsigned char)v;b[1]=(unsigned char)(v>>8);}
 uint64_t receipt=0;assert(!a->submit(a->context,session,frame.lease,&receipt));
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
  unsigned lx=x,ly=y;switch(rotation){case 1:lx=y;ly=width-1-x;break;case 2:lx=width-1-x;ly=height-1-y;break;case 3:lx=height-1-y;ly=x;break;default:break;}
  uint16_t expected=color(lx,ly);
  if(output==RISC_DISPLAY_FORMAT_RGB565){unsigned char*b=pixels+(size_t)y*stride+x*2;assert((uint16_t)(b[0]|(uint16_t)b[1]<<8)==expected);}
  else {bool black=(pixels[(size_t)y*stride+x/8]&(0x80u>>(x&7)))!=0;assert(black!=canvas_rgb565_white(expected,x,y));}
 }
 for(unsigned n=0;n<16;n++)assert(allocation[n]==0xa5&&allocation[16+bytes+n]==0xa5);
 risc_canvas_receipt_v1 settled={.struct_size=sizeof(settled)};assert(!a->poll(a->context,session,receipt,&settled)&&settled.state==RISC_CANVAS_COMPLETE);
 assert(!a->forget_receipt(a->context,session,receipt));assert(!a->close(a->context,session));free(allocation);
}
int main(void){
 unsigned previous=0;
 for(unsigned gray=0;gray<256;gray++){
  uint16_t p=(uint16_t)((gray*31/255)<<11|(gray*63/255)<<5|(gray*31/255));unsigned white=0;
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)white+=canvas_rgb565_white(p,x,y);
  assert(white>=previous);previous=white;if(!gray)assert(!white);if(gray==255)assert(white==16);
 }
 unsigned red=0,green=0,blue=0;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){red+=canvas_rgb565_white(0xf800,x,y);green+=canvas_rgb565_white(0x07e0,x,y);blue+=canvas_rgb565_white(0x001f,x,y);}
 assert(green>red&&red>blue);
 const unsigned sizes[][2]={{7,11},{240,240},{410,502},{480,800},{256,256}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)for(unsigned rotation=0;rotation<4;rotation++){
  run(sizes[i][0],sizes[i][1],RISC_DISPLAY_FORMAT_RGB565,rotation);
  run(sizes[i][0],sizes[i][1],RISC_DISPLAY_FORMAT_MONO1,rotation);
 }
 puts("Adaptive RGB565: all65536 colors exact, explicit monochrome intent, grayscale monotonicity, 40 native geometry/rotation/format cases PASS");
}
