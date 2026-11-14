#include <assert.h>
#include <stdio.h>
#include "canvas_frames.h"
static unsigned char raster[8*8*2];static unsigned acquired,released,submitted;static bool refuse;static unsigned state=3;
static bool acquire(void*ctx,uint32_t format,risc_display_surface_v1*out){(void)ctx;acquired++;*out=(risc_display_surface_v1){1,raster,8,8,format==5?16:1,128,format};return true;}
static void release(void*ctx,uint64_t frame){(void)ctx;assert(frame==1);released++;}
static bool submit(void*ctx,uint64_t frame,const risc_display_rect_v1*d,size_t count,const risc_display_present_options_v1*o,uint64_t*token){(void)ctx;(void)d;(void)count;(void)o;assert(frame==1);if(refuse)return false;submitted++;*token=submitted;return true;}
static bool status(void*ctx,uint64_t token,risc_display_present_status_v1*out){(void)ctx;assert(token);if(token!=submitted)return false;out->state=state;return true;}
int main(void){
 risc_display_output_api_v1 display={.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_frames c={.display=&display,.serial=1,.session=1,.revision=1,.encoding=1,.stride=1,.bytes=8,.format=1};
 risc_canvas_open_v1 request={sizeof(request),1,3,0,0};assert(canvas_mapping_open(&c.mapping,8,8,0,0,0,0,0,&request));
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};assert(!canvas_frames_begin(&c,1,&f));assert(f.byte_count==8);assert(canvas_frames_begin(&c,1,&f)==RISC_CANVAS_BUSY);
 memset(f.pixels,255,8);((unsigned char*)f.pixels)[0]=0x7f;uint64_t receipt=999;
 refuse=true;assert(canvas_frames_submit(&c,1,f.lease,&receipt)==RISC_CANVAS_AGAIN);assert(receipt==999&&c.lease==f.lease&&released==1);
 refuse=false;assert(!canvas_frames_submit(&c,1,f.lease,&receipt));assert(!c.lease&&!c.writable);assert(raster[0]==0x80);for(unsigned i=1;i<8;i++)assert(raster[i]==0);
 assert(canvas_frames_cancel(&c,1,f.lease)==RISC_CANVAS_STALE);assert(canvas_frames_forget(&c,1,receipt)==RISC_CANVAS_BUSY);
 risc_canvas_receipt_v1 out={.struct_size=sizeof(out)};state=2;assert(!canvas_frames_poll(&c,1,receipt,&out)&&out.state==RISC_CANVAS_PRESENTING);
 state=3;assert(!canvas_frames_poll(&c,1,receipt,&out)&&out.state==RISC_CANVAS_COMPLETE);assert(!canvas_frames_forget(&c,1,receipt));assert(!c.receipts);
 assert(!canvas_frames_begin(&c,1,&f));c.revision++;assert(canvas_frames_submit(&c,1,f.lease,&receipt)==RISC_CANVAS_STALE);assert(c.writable);assert(!canvas_frames_cancel(&c,1,f.lease));
 c.format=5;assert(!canvas_frames_begin(&c,1,&f));memset(f.pixels,255,8);((unsigned char*)f.pixels)[0]=0x7f;assert(!canvas_frames_submit(&c,1,f.lease,&receipt));uint16_t black,white;memcpy(&black,raster,2);memcpy(&white,raster+2,2);assert(black==0&&white==65535);
 state=4;assert(!canvas_frames_poll(&c,1,receipt,&out));assert(out.state==RISC_CANVAS_SUPERSEDED);assert(!canvas_frames_forget(&c,1,receipt));assert(acquired==submitted+released);
 c.encoding=RISC_CANVAS_RGBA8;c.stride=32;c.bytes=256;
 assert(!canvas_frames_begin(&c,1,&f));unsigned char*rgba=f.pixels;
 rgba[0]=255;rgba[1]=0;rgba[2]=0;rgba[3]=255;rgba[4]=0;rgba[5]=0;rgba[6]=0;rgba[7]=0;rgba[8]=0;rgba[9]=0;rgba[10]=255;rgba[11]=128;
 state=3;assert(!canvas_frames_submit(&c,1,f.lease,&receipt));uint16_t red,transparent;memcpy(&red,raster,2);memcpy(&transparent,raster+2,2);assert(red==0xf800&&transparent==0xffff);uint16_t half_blue;memcpy(&half_blue,raster+4,2);assert(half_blue==0x7bff);
 uint64_t previous=receipt;
 assert(!canvas_frames_begin(&c,1,&f));assert(!canvas_frames_submit(&c,1,f.lease,&receipt));
 assert(!canvas_frames_poll(&c,1,previous,&out)&&out.state==RISC_CANVAS_COMPLETE);assert(!canvas_frames_forget(&c,1,previous));state=5;
 assert(canvas_frames_poll(&c,1,receipt,&out)==RISC_CANVAS_RETAINED);
 assert(canvas_frames_forget(&c,1,receipt)==RISC_CANVAS_RETAINED);
 /* Test teardown only: actual retained invocation must keep its resources. */
 while(c.receipts){canvas_receipt_node*n=c.receipts;c.receipts=n->next;free(n);}
 printf("Actual display-table lease/refusal/custody/polarity checks passed\n");
}
