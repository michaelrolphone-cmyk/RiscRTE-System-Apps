/* Opt-in native-height NOVA canvas. Provider dimensions remain physical;
 * every logical pixel and contact uses the same width-based transform. */
#include "WatchLayout.h"
#if !defined(PORTABLE_NOVA_UI)
#error Native-height Watch layout requires the original NOVA presentation
#endif
static watch_layout native_watch_layout;
static int native_watch_height(void) { return (int)native_watch_layout.height; }
static void native_watch_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
  if(failed || !surface.frame || !alpha)return;
  /* Layout is validated once at open. A one-pixel rectangle needs only
   * bounded 32-bit arithmetic; avoid generic 64-bit validation per pixel. */
  if(x<0 || y<0 || (uint32_t)x>=native_watch_layout.width ||
      (uint32_t)y>=native_watch_layout.height)return;
  uint32_t scale=native_watch_layout.physical_width;
  uint32_t x0=((uint32_t)x*scale+239u)/240u;
  uint32_t x1=((uint32_t)(x+1)*scale+239u)/240u;
  uint32_t y0=((uint32_t)y*scale+239u)/240u;
  uint32_t y1=((uint32_t)(y+1)*scale+239u)/240u;
  if(y1>native_watch_layout.physical_height)y1=native_watch_layout.physical_height;
  watch_layout_rect physical={(int32_t)x0,(int32_t)y0,x1-x0,y1-y0};
  if(alpha>255)alpha=255;
  for(int py=physical.y;py<physical.y+(int)physical.height;++py)
    for(int px=physical.x;px<physical.x+(int)physical.width;++px) {
      uint8_t *p=(uint8_t*)surface.pixels+(size_t)py*surface.stride_bytes+(size_t)px*2;
      if(alpha==255) {
        uint16_t c=(uint16_t)(((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31));
        p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);continue;
      }
      uint16_t before=(uint16_t)(p[0]|p[1]<<8);
      unsigned r=(((before>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;
      unsigned g=(((before>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;
      unsigned b=((before&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;
      uint16_t c=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
      p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
    }
}
