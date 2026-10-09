/* Client implementation of the shared compact presentation. No hardware pins,
 * register access, or runtime ABI. True FA glyphs are copied OFL font rasters. */
#include "../../../Apps/SpringboardPresentation.h"
#if defined(PORTABLE_RTC_UTC8_DENVER) && defined(PORTABLE_RTC_WALL_TIME)
#error "Select exactly one explicit RTC policy"
#endif
#include "../fonts/icons.inc"
#include "../fonts/text.inc"
static bool nova_mode, nova_mask_pending;
static springboard_contact nova_contact;
#if defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
#include "PortableTime.h"
static risc_runtime_capability_v1 np_rtc_grant;
static const twatch_rtc_api_v1 *np_rtc;
#endif
static unsigned np_root(unsigned n) {
  unsigned r=0,b=1u<<30;while(b>n)b>>=2;
  while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;
}
static uint16_t np_rgb(uint32_t rgb) {
  return (uint16_t)(((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31));
}
static void np_pixel(int x,int y,uint32_t rgb,unsigned a) {
  if(!surface.frame || x<0 || y<0 || x>=width() || y>=height() || !a)return;
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1) {
    native_point(&x,&y);
    uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+x/8;
    uint8_t bit=(uint8_t)(0x80u>>(x&7));
    unsigned old=(*p&bit)?0:255;
    unsigned luminance=(((rgb>>16)&255)*299+((rgb>>8)&255)*587+(rgb&255)*114)/1000;
    if(a>255)a=255;
    if((old*(255-a)+luminance*a)/255<128)*p|=bit;else *p&=(uint8_t)~bit;
    return;
  }
  uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+x*2;
  if(a>=255){uint16_t color=np_rgb(rgb);p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);return;}
  uint16_t old=(uint16_t)(p[0]|p[1]<<8),color;
  unsigned r=((old>>11)&31)*255/31,g=((old>>5)&63)*255/63,b=(old&31)*255/31;
  r=(r*(255-a)+((rgb>>16)&255)*a)/255;
  g=(g*(255-a)+((rgb>>8)&255)*a)/255;b=(b*(255-a)+(rgb&255)*a)/255;
  color=np_rgb((r<<16)|(g<<8)|b);p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);
}
static bool np_icon(int x,int y,int size,const char *name,uint8_t opacity) {
  if(!name || size<1 || size>96)return false;
  const rpi_glyph *g=NULL;
  for(unsigned i=0;i<sizeof(rpi_icons)/sizeof(rpi_icons[0]);++i)
    if(!strcmp(rpi_icons[i].name,name)){g=&rpi_icons[i];break;}
  if(!g)return false;
  int max=g->width>g->height?g->width:g->height;
  int w=g->width*size/max,h=g->height*size/max;
  if(w<1)w=1;
  if(h<1)h=1;
  for(int j=0;j<h;++j)for(int i=0;i<w;++i){
    unsigned n=(unsigned)(j*g->height/h)*g->width+(unsigned)(i*g->width/w);
    unsigned coverage=(g->bits[n/4]>>(6-2*(n%4)))&3;
    np_pixel(x-w/2+i,y-h/2+j,0xffffff,coverage*opacity/3);
  }
  return true;
}
static void np_begin(void) { clear_color(0);nova_mask_pending=true; }
static void np_circle(int x,int y,int radius,uint32_t rgb,uint8_t opacity) {
  if(radius<1 || radius>64)return;
  for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i){
    int d=(int)np_root((unsigned)(i*i+j*j)*256u);
    int edge=radius*16-d;
    unsigned a=edge>=16?opacity:edge>0?(unsigned)edge*opacity/16:0;
    np_pixel(x+i,y+j,rgb,a);
  }
}
static void np_mask(void) {
  if(!nova_mask_pending || !surface.frame)return;
  nova_mask_pending=false;

  for(int y=0;y<height();++y)for(int x=0;x<width();++x){
    int dx=x<width()-x?x:width()-x,dy=y<height()-y?y:height()-y;
    unsigned ax=dx<=0?0:dx<14?(unsigned)dx*102/14:dx<46?102+(unsigned)(dx-14)*153/32:255;
    unsigned ay=dy<=0?0:dy<14?(unsigned)dy*102/14:dy<46?102+(unsigned)(dy-14)*153/32:255;
    unsigned a=ax*ay/255;
    if(a==255)continue;
    if(dx<22 && dy<22 && (22-dx)*(22-dx)+(22-dy)*(22-dy)>22*22)a=0;
    uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+x*2;
    uint16_t c=(uint16_t)(p[0]|p[1]<<8);
    c=(uint16_t)((((c>>11)&31)*a/255)<<11|(((c>>5)&63)*a/255)<<5|((c&31)*a/255));
    p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
  }
}
/* One solid backing for the whole text block, including spaces. Six pixels of
 * smooth alpha falloff hide the rectangle's edge; no glyph dilation, blur
 * buffer, or repeated darkening at overlapping letter shadows is needed. */
static void np_caption_backing(int left,int top,int right,int bottom) {
  const int feather=6,range=feather*16;
  left-=2;top-=2;right+=2;bottom+=2;
  for(int y=top-feather;y<=bottom+feather;++y)
    for(int x=left-feather;x<=right+feather;++x) {
      int dx=x<left?left-x:x>right?x-right:0;
      int dy=y<top?top-y:y>bottom?y-bottom:0;
      int t=range-(int)np_root((unsigned)(dx*dx+dy*dy)*256u);
      if(t<=0)continue;
      unsigned alpha=(unsigned)(t*t*(3*range-2*t))*255u/(unsigned)(range*range*range);
      np_pixel(x,y,0,alpha);
    }
}
static void np_caption(int y,const char *s,bool clock,uint32_t rgb) {
  np_mask();if(!s)return;
  int length=0,total=0,spacing=clock?21:34,max=width()-16;
  while(length<95 && s[length]){
    unsigned c=(unsigned char)s[length];if(c>='a' && c<='z')c-=32;
    if(c<32 || c>126)c='?';
    const rpt_glyph *g=&rpt_text[(clock?0:95)+c-32];
    int next=total+g->advance_q4+(length?spacing:0);
    if(next>max*16)break;
    total=next;++length;
  }
  if(!length)return;
  int start=(width()*16-total)/2,x=start;
  int left=width(),top=height(),right=-1,bottom=-1;
  for(int k=0;k<length;++k){
    unsigned c=(unsigned char)s[k];if(c>='a' && c<='z')c-=32;if(c<32 || c>126)c='?';
    const rpt_glyph *g=&rpt_text[(clock?0:95)+c-32];
    if(g->width && g->height){
      int l=x/16+g->left,t=y+g->top,r=l+g->width-1,b=t+g->height-1;
      if(l<left)left=l;
      if(t<top)top=t;
      if(r>right)right=r;
      if(b>bottom)bottom=b;
    }
    x+=g->advance_q4+spacing;
  }
  if(right>=left && bottom>=top)np_caption_backing(left,top,right,bottom);
  x=start;
  for(int k=0;k<length;++k){
    unsigned c=(unsigned char)s[k];if(c>='a' && c<='z')c-=32;if(c<32 || c>126)c='?';
    const rpt_glyph *g=&rpt_text[(clock?0:95)+c-32];
    for(unsigned j=0;j<g->height;++j)for(unsigned i=0;i<g->width;++i){
      unsigned n=j*g->width+i,coverage=(g->bits[n/4]>>(6-2*(n%4)))&3;
      np_pixel(x/16+g->left+(int)i,y+g->top+(int)j,rgb,coverage*85);
    }
    x+=g->advance_q4+spacing;
  }
}
static void np_contact(springboard_contact *out) { *out=nova_contact; }
static bool np_clock(uint8_t *hour,uint8_t *minute) {
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
  twatch_rtc_time_v1 local;
  if(!hour || !minute || !native_toolbar_local(&local))return false;
  *hour=local.hour;*minute=local.minute;return true;
#elif defined(PORTABLE_DESK_CLOCK_SPARSE_START)
  return !failed && portable_desk_clock_time(hour,minute);
#elif defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
  twatch_rtc_time_v1 raw,local;
  if(!np_rtc || !np_rtc->read(np_rtc->context,&raw) || !portable_time_forward(&raw,&local))return false;
  *hour=local.hour;*minute=local.minute;return true;
#else
  /* No implicit timezone guess when the deployment has not selected a policy. */
  (void)hour;(void)minute;return false;
#endif
}
#if !defined(PORTABLE_SETTINGS_NATIVE_TIME) && !defined(PORTABLE_NATIVE_TIME_TOOLBAR)
static void np_close(void) {
#if defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
  if(np_rtc_grant.api && !rt->release(&np_rtc_grant))rt->diagnostic("PORTABLE_APP error=toolbar-clock-release");
  np_rtc=NULL;
#endif
}
static void np_poll(t5_app_input_t *out);
#endif
static const springboard_presentation np_view={sizeof(np_view),np_begin,np_circle,np_icon,np_caption,np_contact,np_clock};
const springboard_presentation *springboard_presentation_get(void) {
  if(surface_format!=RISC_DISPLAY_FORMAT_RGB565 || (width()>=400 && height()>=400))return NULL;
  if(info.nominal_refresh_millihz<20000 ||
      (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) || info.typical_present_latency_us>50000)return NULL;
#if defined(PORTABLE_RTC_UTC8_DENVER) || defined(PORTABLE_RTC_WALL_TIME)
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
  (void)out;
  portable_touch_sample s;input_take(&s);
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick.ui.paper && quick_replay_pending && s.valid && s.down) {
    input_sample=s;input_pending=true;quick_replay_delivery=true;
    s.x=(uint16_t)(quick.ui.start_x*width()/480);s.y=(uint16_t)(quick.ui.start_y*height()/800);s.began=true;
  }
  quick_replay_pending=false;
#endif
  nova_contact=(springboard_contact){s.valid,s.down,s.began,s.released,s.cancelled,s.tap_eligible&&!s.moved,(int16_t)s.x,(int16_t)s.y};
  if(s.x>=width() || s.y>=height()){nova_contact.valid=false;nova_contact.cancelled=true;}
#ifndef PORTABLE_HOME_POINTS_NATIVE_UTC
  if(s.released && s.tap_eligible && !s.moved && s.x<44 && s.y<40){out->exit_requested=true;out->buttons=T5_APP_BUTTON_BACK;}
#endif
}
