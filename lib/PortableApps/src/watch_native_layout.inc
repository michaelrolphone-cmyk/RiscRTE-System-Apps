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
  watch_layout_rect logical={x,y,1,1},physical;
  if(!watch_layout_native_rect(&native_watch_layout,&logical,&physical))return;
  if(alpha>255)alpha=255;
  for(int py=physical.y;py<physical.y+(int)physical.height;++py)
    for(int px=physical.x;px<physical.x+(int)physical.width;++px) {
      uint8_t *p=(uint8_t*)surface.pixels+(size_t)py*surface.stride_bytes+(size_t)px*2;
      uint16_t before=(uint16_t)(p[0]|p[1]<<8);
      unsigned r=(((before>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;
      unsigned g=(((before>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;
      unsigned b=((before&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;
      uint16_t c=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
      p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
    }
}
