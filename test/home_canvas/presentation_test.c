#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_provider.h"
#include "PortableHomeCanvas.h"
static unsigned w,h,format,stride,acquires,releases,submits,seeds,refuse,fail_seed;
static uint64_t native_lease,seeded_lease,sequence;
static unsigned char *pixels,*previous;
static risc_display_rect_v1 submitted_damage;
static unsigned submitted_intent;
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=w,.height=h,.supported_formats=RISC_DISPLAY_FORMAT_BIT(format)};return true;}
static bool acquire(void*c,uint32_t requested,risc_display_surface_v1*out){(void)c;assert(requested==format&&!native_lease);native_lease=++sequence;acquires++;*out=(risc_display_surface_v1){native_lease,pixels,w,h,stride,stride*h,format};return true;}
static void release(void*c,uint64_t lease){(void)c;assert(lease==native_lease);native_lease=0;releases++;}
static bool seed(void*c,uint64_t lease){(void)c;assert(lease==native_lease);seeds++;seeded_lease=lease;memcpy(previous,pixels,(size_t)stride*h);return !fail_seed;}
static bool submit(void*c,uint64_t lease,const risc_display_rect_v1*rect,size_t count,const risc_display_present_options_v1*options,uint64_t*out){
 (void)c;assert(lease==native_lease&&lease==seeded_lease);submits++;assert(count==1&&rect&&options);
 submitted_damage=*rect;submitted_intent=options->intent;
 if(refuse){refuse--;return false;}native_lease=0;*out=++sequence;return true;
}
static bool status(void*c,uint64_t token,risc_display_present_status_v1*out){(void)c;assert(token==sequence);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static void fill(risc_display_surface_v1*f,uint16_t value){
 if(f->pixel_format==RISC_DISPLAY_FORMAT_MONO1){memset(f->pixels,value?0:255,f->size_bytes);return;}
 for(unsigned y=0;y<f->height;y++)for(unsigned x=0;x<f->width;x++){unsigned char*p=(unsigned char*)f->pixels+(size_t)y*f->stride_bytes+x*2;p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);}
}
static void check_pixels(const unsigned char*buffer,uint16_t value){
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  if(format==RISC_DISPLAY_FORMAT_RGB565){const unsigned char*p=buffer+(size_t)y*stride+x*2;assert((uint16_t)(p[0]|(uint16_t)p[1]<<8)==value);}
  else assert(!!(buffer[(size_t)y*stride+x/8]&(0x80u>>(x&7)))!=canvas_rgb565_white(value,x,y));
 }
}
static void run(unsigned rotation,unsigned physical_format,unsigned logical_format){
 w=7;h=11;format=physical_format;stride=format==RISC_DISPLAY_FORMAT_RGB565?w*2:(w+7)/8;
 pixels=calloc(h,stride);previous=calloc(h,stride);assert(pixels&&previous);
 native_lease=seeded_lease=0;acquires=releases=submits=seeds=fail_seed=0;refuse=1;
 risc_display_output_api_v1_history display={.base={.api_version=1,.struct_size=sizeof(display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=seed};
 canvas_provider p;canvas_provider_init(&p,&display.base,NULL,rotation);
 home_canvas_surface bridge;assert(home_canvas_open_content_owned(&bridge,&p.api,logical_format,true,NULL,NULL,NULL,NULL));
 const risc_display_output_api_v1*d=&bridge.display;
 const risc_display_output_api_v1_history*history=&bridge.history;
 assert(history->extension_tag==RISC_DISPLAY_HISTORY_TAG);
 risc_display_surface_v1 f;assert(d->acquire(&bridge,logical_format,&f));
 uint16_t old=logical_format==RISC_DISPLAY_FORMAT_MONO1?0xffff:0x07e0;
 uint16_t next=logical_format==RISC_DISPLAY_FORMAT_MONO1?0:0xf800;
 fill(&f,old);assert(history->seed_previous(&bridge,f.frame));
 assert(seeds==1&&acquires==1&&!submits&&native_lease);
 check_pixels(previous,old);fill(&f,next);
 risc_display_rect_v1 damage={1,1,2,3};
 risc_display_present_options_v1 intent={RISC_DISPLAY_PRESENT_QUALITY,RISC_DISPLAY_QUEUE_FIFO,0};uint64_t receipt=0;
 assert(!d->submit(&bridge,f.frame,&damage,1,&intent,&receipt));
 assert(acquires==1&&!releases&&native_lease&&bridge.lease==f.frame);
 assert(d->submit(&bridge,f.frame,&damage,1,&intent,&receipt));
 assert(acquires==1&&!releases&&submits==2&&!native_lease);
 check_pixels(previous,old);check_pixels(pixels,next);assert(submitted_intent==RISC_DISPLAY_PRESENT_QUALITY);
 const risc_display_rect_v1 expected[]={{1,1,2,3},{3,1,3,2},{4,7,2,3},{1,8,3,2}};
 assert(submitted_damage.x==expected[rotation].x&&submitted_damage.y==expected[rotation].y&&submitted_damage.width==expected[rotation].width&&submitted_damage.height==expected[rotation].height);
 risc_display_present_status_v1 r;assert(d->present_status(&bridge,receipt,&r)&&r.state==RISC_DISPLAY_PRESENT_COMPLETE);
 assert(d->acquire(&bridge,logical_format,&f));fill(&f,0xffff);assert(history->seed_previous(&bridge,f.frame));
 d->release(&bridge,f.frame);assert(releases==1&&!native_lease);
 assert(home_canvas_close(&bridge));free(previous);free(pixels);
}
int main(void){
 for(unsigned r=0;r<4;r++)for(unsigned p=0;p<2;p++)for(unsigned l=0;l<2;l++)
 run(r,p?RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1,l?RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1);
 puts("Common Home Canvas:16 logical/physical raster and rotation combinations, prior-image same-lease reconstruction, refusal retry, semantic damage and cancellation PASS");
}
