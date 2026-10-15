/* Client implementation of the shared compact presentation. No hardware pins,
 * register access, or runtime ABI. Original OFL glyphs use native outlines. */
#include "../../../Apps/SpringboardPresentation.h"
static const char np_native_marker[] __attribute__((used))="native-springboard-vector-v1";
static bool nova_mode, nova_mask_pending;
static springboard_contact nova_contact;
#ifdef PORTABLE_RTC_UTC8_DENVER
#include "PortableTime.h"
static risc_runtime_capability_v1 np_rtc_grant;
static const twatch_rtc_api_v1 *np_rtc;
#endif
static unsigned np_root(unsigned n) {
  unsigned r=0,b=1u<<30;while(b>n)b>>=2;
  while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;
}
static int np_width(void) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
  return nova_mode?width():(int)native_watch_layout.width;
#else
  return width();
#endif
}
static int np_height(void) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
  return nova_mode?height():native_watch_height();
#else
  return height();
#endif
}
static uint16_t np_rgb(uint32_t rgb) {
  return (uint16_t)(((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31));
}
static void np_pixel(int x,int y,uint32_t rgb,unsigned a) {
#ifdef PORTABLE_CONTEXTS_RASTER
  if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;
#endif
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
  if(!springboard_native_input && !nova_mode && native_watch_layout.physical_width!=WATCH_LAYOUT_WIDTH) {native_watch_pixel(x,y,rgb,a);return;}
#endif
  if(!surface.frame || x<0 || y<0 || x>=width() || y>=height() || !a)return;
  uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+x*2;
  if(a>=255){uint16_t color=np_rgb(rgb);p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);return;}
  uint16_t old=(uint16_t)(p[0]|p[1]<<8),color;
  if(!old && !rgb)return;
  unsigned r=((old>>11)&31)*255/31,g=((old>>5)&63)*255/63,b=(old&31)*255/31;
  r=(r*(255-a)+((rgb>>16)&255)*a)/255;
  g=(g*(255-a)+((rgb>>8)&255)*a)/255;b=(b*(255-a)+(rgb&255)*a)/255;
  color=np_rgb((r<<16)|(g<<8)|b);p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);
}
#include "nova_native.inc"
static bool np_icon_color(int x,int y,int size,const char *name,uint8_t opacity,uint32_t rgb) {
  if(!name || size<1 || size>2048)return false;
  const np_vector_glyph *g=NULL;
  for(unsigned i=0;i<sizeof(np_vector_icons)/sizeof(np_vector_icons[0]);i++)
    if(!strcmp(np_vector_icons[i].name,name)){g=&np_vector_glyphs[np_vector_icons[i].glyph];break;}
  if(!g)return false;
  if(size>96)size=96;
  int gw=g->right-g->left,gh=g->top-g->bottom,maximum=gw>gh?gw:gh;
  int w=gw*size/maximum,h=gh*size/maximum;if(w<1)w=1;if(h<1)h=1;
  return np_vector_draw(x-w/2,y-h/2,w,h,g,rgb,opacity);
}
static bool np_icon(int x,int y,int size,const char *name,uint8_t opacity) {
  return np_icon_color(x,y,size,name,opacity,0xffffff);
}
static void np_begin(void) { clear_color(0);nova_mask_pending=true; }
static void np_circle(int x,int y,int radius,uint32_t rgb,uint8_t opacity) {
  if(radius<1 || radius>(nova_mode?512:64))return;
  if(nova_mode){np_native_circle(x,y,radius,rgb,opacity);return;}
  for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i){
    unsigned squared=(unsigned)(i*i+j*j);
    /* Only the one-pixel antialiased rim needs a square root. */
    if(squared>=(unsigned)(radius*radius))continue;
    if(squared<=(unsigned)((radius-1)*(radius-1))) {
      np_pixel(x+i,y+j,rgb,opacity);continue;
    }
    int d=(int)np_root(squared*256u);
    int edge=radius*16-d;
    unsigned a=edge>=16?opacity:edge>0?(unsigned)edge*opacity/16:0;
    np_pixel(x+i,y+j,rgb,a);
  }
}
static void np_mask(void) {
#ifdef PORTABLE_CONTEXTS_RASTER
  if(failed)return;
#endif
  if(!nova_mask_pending || !surface.frame)return;
  nova_mask_pending=false;
  if(nova_mode){np_native_mask();return;}

  const int physical_width=width(),physical_height=height();
  const int logical_width=np_width(),logical_height=np_height();
  /* The opaque center is unchanged by this mask. Map its half-open span
   * with the same ceil boundaries as logical pixels, preserving the right
   * edge's inclusive distance=46 sample at fractional native scales. */
  int center_first=46,center_end=logical_width-45;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
  center_first=(46*physical_width+239)/240;
  center_end=((logical_width-45)*physical_width+239)/240;
#endif
  for(int y=0;y<physical_height;++y) {
    int ly=y;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    ly=(int)((uint32_t)y*WATCH_LAYOUT_WIDTH/native_watch_layout.physical_width);
    uint32_t logical_x=0,remainder=0;
#endif
    int dy=ly<logical_height-ly?ly:logical_height-ly;
    unsigned ay=dy<=0?0:dy<14?(unsigned)dy*102/14:dy<46?102+(unsigned)(dy-14)*153/32:255;
    for(int x=0;x<physical_width;++x){
      if(ay==255 && x==center_first && center_end>center_first) {
        x=center_end;
        if(x>=physical_width)break;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
        logical_x=(uint32_t)x*WATCH_LAYOUT_WIDTH/(uint32_t)physical_width;
        remainder=(uint32_t)x*WATCH_LAYOUT_WIDTH%(uint32_t)physical_width;
#endif
      }
#ifdef PORTABLE_CONTEXTS_RASTER
      if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;
#endif
      int lx=x;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
      lx=(int)logical_x;
      remainder+=WATCH_LAYOUT_WIDTH;
      while(remainder>=native_watch_layout.physical_width) {
        remainder-=native_watch_layout.physical_width;++logical_x;
      }
#endif
      uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+x*2;
      uint16_t c=(uint16_t)(p[0]|p[1]<<8);
      /* Clear background stays black at every opacity; avoid blending it. */
      if(!c)continue;
      int dx=lx<logical_width-lx?lx:logical_width-lx;
      unsigned ax=dx<=0?0:dx<14?(unsigned)dx*102/14:dx<46?102+(unsigned)(dx-14)*153/32:255;
      unsigned a=ax*ay/255;
      if(a==255)continue;
      if(dx<22 && dy<22 && (22-dx)*(22-dx)+(22-dy)*(22-dy)>22*22)a=0;
      c=(uint16_t)((((c>>11)&31)*a/255)<<11|(((c>>5)&63)*a/255)<<5|((c&31)*a/255));
      p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
    }
  }
}
static void np_caption(int y,const char *s,bool clock,uint32_t rgb) {
  np_mask();np_native_caption(y,s,clock,rgb);
}
static void np_contact(springboard_contact *out) { *out=nova_contact; }
static bool np_clock(uint8_t *hour,uint8_t *minute) {
#ifdef PORTABLE_RTC_UTC8_DENVER
  twatch_rtc_time_v1 raw,local;
  if(!np_rtc || !np_rtc->read(np_rtc->context,&raw) || !portable_time_forward(&raw,&local))return false;
  *hour=local.hour;*minute=local.minute;return true;
#else
  /* No implicit timezone guess when the deployment has not selected a policy. */
  (void)hour;(void)minute;return false;
#endif
}
static void np_close(void) {
#ifdef PORTABLE_RTC_UTC8_DENVER
  if(np_rtc_grant.api && !rt->release(&np_rtc_grant))rt->diagnostic("PORTABLE_APP error=toolbar-clock-release");
  np_rtc=NULL;
#endif
}
static void np_poll(t5_app_input_t *out);
static const springboard_presentation np_view={sizeof(np_view),np_begin,np_circle,np_icon,np_caption,np_contact,np_clock};
const springboard_presentation *springboard_presentation_get(void) {
  springboard_native_input=true;
#ifndef PORTABLE_WATCH_NATIVE_HEIGHT
  if(width()>=400 && height()>=400)return NULL;
#endif
  /* Zero is unknown timing, not evidence that the selected presentation is
   * incompatible. Keep known slow/retained displays on their existing path;
   * optional retained-frame animation has its own conservative timing gate. */
  if((info.nominal_refresh_millihz && info.nominal_refresh_millihz<20000) ||
      (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) || info.typical_present_latency_us>50000)return NULL;
#ifdef PORTABLE_RTC_UTC8_DENVER
  np_rtc_grant.struct_size=sizeof(np_rtc_grant);
  if(!np_rtc_grant.api && rt->acquire("rtc.clock",TWATCH_RTC_API_V1,0,&np_rtc_grant)) {
    const twatch_rtc_api_v1 *clock=np_rtc_grant.api;
    if(clock && clock->api_version==TWATCH_RTC_API_V1 && clock->struct_size>=sizeof(*clock) && clock->read)np_rtc=clock;
    else if(np_rtc_grant.api && !rt->release(&np_rtc_grant))failed=true;
  }
#endif
  nova_mode=true;memset(&nova_contact,0,sizeof(nova_contact));
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_pending=true;
  t5_app_input_t initial={0};np_poll(&initial);
#endif
  return &np_view;
}
static void np_poll(t5_app_input_t *out) {
  portable_touch_sample s;input_take_native(&s);
  nova_contact=(springboard_contact){s.valid,s.down,s.began,s.released,s.cancelled,s.tap_eligible&&!s.moved,(int16_t)s.x,(int16_t)s.y};
  if(s.x>=np_width() || s.y>=np_height()){nova_contact.valid=false;nova_contact.cancelled=true;}
  if(s.released && s.tap_eligible && !s.moved && s.x<(unsigned)np_native_measure(44) && s.y<(unsigned)np_native_measure(40)){out->exit_requested=true;out->buttons=T5_APP_BUTTON_BACK;}
}
