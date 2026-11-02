#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_provider.h"
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
static void fill(risc_canvas_frame_v1*f,uint16_t value){
 for(unsigned y=0;y<f->height;y++)for(unsigned x=0;x<f->width;x++){unsigned char*p=(unsigned char*)f->pixels+(size_t)y*f->stride+x*2;p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);}
}
static void check_pixels(const unsigned char*buffer,uint16_t value){
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  if(format==RISC_DISPLAY_FORMAT_RGB565){const unsigned char*p=buffer+(size_t)y*stride+x*2;assert((uint16_t)(p[0]|(uint16_t)p[1]<<8)==value);}
  else assert(!!(buffer[(size_t)y*stride+x/8]&(0x80u>>(x&7)))!=canvas_rgb565_white(value,x,y));
 }
}
static void run(unsigned rotation,unsigned physical_format){
 w=7;h=11;format=physical_format;stride=format==RISC_DISPLAY_FORMAT_RGB565?w*2:(w+7)/8;
 pixels=calloc(h,stride);previous=calloc(h,stride);assert(pixels&&previous);
 native_lease=seeded_lease=0;acquires=releases=submits=seeds=fail_seed=0;refuse=1;
 risc_display_output_api_v1_history display={.base={.api_version=1,.struct_size=sizeof(display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=seed};
 canvas_provider p;canvas_provider_init(&p,&display.base,NULL,rotation);
 const risc_canvas_presentation_v1*present=NULL;
 risc_canvas_open_presentation_v1 request={.base={sizeof(request),RISC_CANVAS_RGB565_ADAPTIVE,RISC_CANVAS_RESPONSIVE,0,0},.presentation_tag=RISC_CANVAS_PRESENTATION_TAG,.presentation_out=&present};
 uint64_t session=0;assert(!p.api.open(&p,&request.base,&session)&&present&&risc_canvas_presentation_get_v1(present,sizeof(*present)));
 assert(present->flags==RISC_CANVAS_PRESENT_HISTORY_SUPPORTED);
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};assert(!p.api.begin_frame(&p,session,&f));
 fill(&f,0x07e0);assert(!present->seed_previous(&p,session,f.lease));
 assert(seeds==1&&acquires==1&&!submits&&native_lease&&p.frames.seeded.frame==native_lease);
 check_pixels(previous,0x07e0);
 fill(&f,0xf800); /* Repaint the same logical lease after reconstructing old image. */
 risc_canvas_rect_v1 damage={1,1,2,3};risc_canvas_present_v1 intent={sizeof(intent),RISC_CANVAS_PRESENT_QUALITY,&damage,1};uint64_t receipt=0;
 assert(present->submit(&p,session,f.lease,&intent,&receipt)==RISC_CANVAS_AGAIN);
 assert(acquires==1&&!releases&&native_lease&&p.frames.lease==f.lease);
 assert(!present->submit(&p,session,f.lease,&intent,&receipt));
 assert(acquires==1&&!releases&&submits==2&&!native_lease&&!p.frames.seeded.frame);
 check_pixels(previous,0x07e0);check_pixels(pixels,0xf800);assert(submitted_intent==RISC_DISPLAY_PRESENT_QUALITY);
 const risc_display_rect_v1 expected[]={{1,1,2,3},{3,1,3,2},{4,7,2,3},{1,8,3,2}};
 assert(submitted_damage.x==expected[rotation].x&&submitted_damage.y==expected[rotation].y&&submitted_damage.width==expected[rotation].width&&submitted_damage.height==expected[rotation].height);
 risc_canvas_receipt_v1 r={.struct_size=sizeof(r)};assert(!p.api.poll(&p,session,receipt,&r));assert(!p.api.forget_receipt(&p,session,receipt));
 assert(!p.api.begin_frame(&p,session,&f));fill(&f,0xffff);assert(!present->seed_previous(&p,session,f.lease));assert(!p.api.cancel_frame(&p,session,f.lease));assert(releases==1&&!native_lease);
 /* Actual absence is explicit and leaves the logical lease cancellable. */
 display.extension_tag=0;assert(!p.api.begin_frame(&p,session,&f));assert(present->seed_previous(&p,session,f.lease)==RISC_CANVAS_UNSUPPORTED);assert(!p.api.cancel_frame(&p,session,f.lease));display.extension_tag=RISC_DISPLAY_HISTORY_TAG;
 assert(!p.api.close(&p,session));
 free(previous);free(pixels);
}
int main(void){
 for(unsigned rotation=0;rotation<4;rotation++){run(rotation,RISC_DISPLAY_FORMAT_MONO1);run(rotation,RISC_DISPLAY_FORMAT_RGB565);}
 puts("Canvas presentation: existing base API preserved,8 rotated color/mono seed/repaint cases, same native lease, no seed refresh, semantic quality+damage, refusal retry and cancellation PASS");
}
