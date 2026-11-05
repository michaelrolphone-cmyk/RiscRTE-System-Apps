#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RiscDisplayOutputV1.h"
#ifndef PORTABLE_WATCH_NATIVE_HEIGHT
#define PORTABLE_WATCH_NATIVE_HEIGHT
#endif
static risc_display_surface_v1 surface;
static uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
static bool failed,list_mode;
static unsigned raster_pixels;
static int width(void){return surface.width;}
static int height(void){return surface.height;}
static bool raster_checkpoint(void){return true;}
static bool raster_surface_writable(void){return surface.pixels!=NULL;}
static void np_pixel(int x,int y,uint32_t c,unsigned a){(void)x;(void)y;(void)c;(void)a;assert(0);}
static void clear_color(uint32_t c){(void)c;for(unsigned y=0;y<surface.height;y++)memset((char*)surface.pixels+y*surface.stride_bytes,0,surface.width*2);}
#include "../../lib/PortableApps/src/nova_ui.inc"
#include "../../Apps/file_browser_native_layout.inc"
static void test(unsigned w,unsigned h){
 unsigned stride=w+4;uint16_t *p=malloc((size_t)stride*h*2);assert(p);
 for(unsigned i=0;i<stride*h;i++)p[i]=0xa5a5;
 surface=(risc_display_surface_v1){.pixels=p,.width=w,.height=h,.stride_bytes=stride*2,.size_bytes=stride*h*2,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 assert(watch_layout_open(&fb_layout,w,h,NULL));
 portable_nova_begin();portable_nova_header("FILES");portable_nova_button(16,188,64,44,"Prev",false);
 fb_layout_text(3,18,175,204,"2 of 9",NOVA_CAP);assert(fb_layout_wrap(1,18,52,204,17,2,"Long file name",NOVA_WHITE));
 watch_layout_rect footer={16,fb_layout_y(188),64,44},physical;
 assert(watch_layout_native_rect(&fb_layout,&footer,&physical));
 unsigned lit=0;for(unsigned py=0;py<h;py++)for(unsigned px=0;px<w;px++){
  int x=-1,y=-1;bool valid=fb_layout_touch(px,py,&x,&y);
  bool hit=valid&&portable_nova_hit(x,y,16,188,64,44);
  assert(hit==watch_layout_contains(&physical,px,py));
  if(hit&&p[py*stride+px])lit++;
 }
 assert(lit>1000);int x,y;assert(!fb_layout_touch(-1,0,&x,&y));assert(!fb_layout_touch(w,0,&x,&y));assert(!fb_layout_touch(0,h,&x,&y));
 portable_nova_fill(0,nu_height()-1,240,1,0xffffff);
 assert(p[(h-1)*stride]==0xffff&&p[(h-1)*stride+w-1]==0xffff);
 for(unsigned py=0;py<h;py++)for(unsigned px=w;px<stride;px++)assert(p[py*stride+px]==0xa5a5);
 free(p);
}
int main(void){test(240,240);test(410,502);test(480,600);puts("Actual NOVA native renderer: footer pixels/touch inverse exhaustive, last physical row and padding guards PASS");}
