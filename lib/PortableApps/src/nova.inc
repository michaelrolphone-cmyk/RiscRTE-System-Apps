/* Generic format-selected row acceleration; explicit zero retains the scalar
 * oracle for differential qualification. No board or app capability is added. */
#ifndef PORTABLE_RGB565_ROW_WRITER
#define PORTABLE_RGB565_ROW_WRITER 1
#endif
#if PORTABLE_RGB565_ROW_WRITER != 0 && PORTABLE_RGB565_ROW_WRITER != 1
#error "PORTABLE_RGB565_ROW_WRITER must be zero or one"
#endif
#include "PortableRgb565Blend.h"
#include "PortableFrameCadence.h"
/* Client implementation of the shared compact presentation. No hardware pins,
 * register access, or runtime ABI. True FA glyphs are copied OFL font rasters. */
#include "../../../Apps/SpringboardPresentation.h"
#if defined(PORTABLE_RTC_UTC8_DENVER) && defined(PORTABLE_RTC_WALL_TIME)
#error "Select exactly one explicit RTC policy"
#endif
#if defined(PORTABLE_WATCH_NATIVE_HEIGHT)
#include "../fonts/watch-native-icons.inc"
#elif defined(PORTABLE_WATCH_FROZEN_ICONS)
/* Product-selected icon bytes from the delivered Watch contract. */
#include "../fonts/watch-240-icons.inc"
#else
#include "../fonts/icons.inc"
#endif
#if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || defined(PORTABLE_RESIDENT_LOADING)
#include "../fonts/springboard_gamepad.inc"
#endif
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
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_NP_PIXEL,(int32_t[8]){x,y,(int32_t)rgb,(int32_t)a},NULL))return;
  if(raster_replaying&&(y<raster_band_top||y>=raster_band_bottom))return;
#endif
  if(failed || (!(++raster_pixels&511u)&&!raster_checkpoint()))return;
#if defined(PORTABLE_TOUCH_SCROLL) && (defined(PORTABLE_FILE_BROWSER_APP) || defined(PORTABLE_APP_TOUCH_SCROLL) || defined(PORTABLE_SETTINGS_LIST_SCROLL))
  if(raster_clip_active&&!portable_scroll_inside(&raster_clip,x,y))return;
#endif
  if(!raster_surface_writable() || x<0 || y<0 || x>=width() || y>=height() || !a)return;
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
  native_point(&x,&y);
  uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+x*2;
  if(a>=255){uint16_t color=np_rgb(rgb);p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);return;}
  uint16_t old=(uint16_t)(p[0]|p[1]<<8),color;
  color=portable_rgb565_over(old,rgb,a);p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);
}
#if PORTABLE_RGB565_ROW_WRITER
#include "nova_rgb565_row.inc"
#endif
/* Exact four-neighbor coverage. Unsigned coordinates preserve the -1 edge
 * sample while allowing its +1 neighbor; no out-of-range byte is loaded. */
static inline __attribute__((always_inline)) unsigned np_bilinear_coverage(
    const rpi_glyph *g,int x,int y,unsigned fx,unsigned fy) {
  unsigned w=g->width,h=g->height,ux=(unsigned)x,uy=(unsigned)y;
  unsigned a=0,b=0,c=0,d=0,n;
  if(uy<h){
    unsigned row=uy*w;
    if(ux<w){n=row+ux;a=(g->bits[n/4]>>(6-2*(n%4)))&3;}
    if(ux+1u<w){n=row+ux+1u;b=(g->bits[n/4]>>(6-2*(n%4)))&3;}
  }
  if(uy+1u<h){
    unsigned row=(uy+1u)*w;
    if(ux<w){n=row+ux;c=(g->bits[n/4]>>(6-2*(n%4)))&3;}
    if(ux+1u<w){n=row+ux+1u;d=(g->bits[n/4]>>(6-2*(n%4)))&3;}
  }
  return (a*(256-fx)+b*fx)*(256-fy)+(c*(256-fx)+d*fx)*fy;
}
static bool np_icon(int x,int y,int size,const char *name,uint8_t opacity) {
  if(!name || size<1 || size>256)return false;
  const rpi_glyph *g=NULL;
  for(unsigned i=0;i<sizeof(rpi_icons)/sizeof(rpi_icons[0]);++i)
    if(!strcmp(rpi_icons[i].name,name)){g=&rpi_icons[i];break;}
#if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || defined(PORTABLE_RESIDENT_LOADING)
  if(!g && !strcmp(name,rpi_springboard_gamepad.name))g=&rpi_springboard_gamepad;
#endif
  if(!g)return false;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_NP_ICON,(int32_t[8]){x,y,size,opacity},name))return true;
#endif
  int max=g->width>g->height?g->width:g->height;
  int w=g->width*size/max,h=g->height*size/max;
  if(w<1)w=1;
  if(h<1)h=1;
  int row_first=0,row_end=h;
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_rows((int64_t)y-h/2,&row_first,&row_end);
#endif
  /* Exact quotient/remainder stepping of the positive pixel-center
   * numerator. No nearest-neighbor substitution or coverage quantization. */
  const unsigned numerator=(unsigned)g->width*128u;
  const unsigned x_start=numerator/(unsigned)w,x_remainder=numerator%(unsigned)w;
  const unsigned delta=numerator*2u,x_step=delta/(unsigned)w,x_carry=delta%(unsigned)w;
  for(int j=row_first;j<row_end;++j){
#if PORTABLE_RGB565_ROW_WRITER
    np_rgb565_row row;np_rgb565_row_begin(&row,y-h/2+j,0xffffff);
#endif
    int v=((2*j+1)*g->height*128/h)-128;
    int sy=v>=0?v/256:-1;
    unsigned fy=(unsigned)(v-sy*256),x_value=x_start,x_error=x_remainder;
    for(int i=0;i<w;++i){
      int u=(int)x_value-128,sx=u>=0?u/256:-1;
      unsigned fx=(unsigned)(u-sx*256);
      unsigned coverage=np_bilinear_coverage(g,sx,sy,fx,fy);
#if PORTABLE_RGB565_ROW_WRITER
      np_rgb565_row_pixel(&row,x-w/2+i,(coverage*opacity+98304u)/196608u);
#else
      np_pixel(x-w/2+i,y-h/2+j,0xffffff,(coverage*opacity+98304u)/196608u);
#endif
      x_value+=x_step;x_error+=x_carry;
      if(x_error>=(unsigned)w){x_error-=(unsigned)w;++x_value;}
    }
  }
  return true;
}
static void np_begin(void) { clear_color(0);nova_mask_pending=true; }
static void np_circle(int x,int y,int radius,uint32_t rgb,uint8_t opacity) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_NP_CIRCLE,(int32_t[8]){x,y,radius,(int32_t)rgb,opacity},NULL))return;
#endif
  if(radius<1 || radius>256)return;
  int row_first=-radius,row_end=radius+1;
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_rows(y,&row_first,&row_end);
#endif
  /* Only the exactly opaque RGB565 interior is a solid span. Preserve the
   * anti-aliased rim and all other formats/alpha/active clipping verbatim. */
  bool solid=opacity==255 && surface_format==RISC_DISPLAY_FORMAT_RGB565;
#ifdef PORTABLE_TOUCH_SCROLL
  if(raster_clip_active)solid=false;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  solid=false;
#endif
  const unsigned inner=(unsigned)((radius-1)*(radius-1));
  for(int j=row_first;j<row_end;++j){
#if PORTABLE_RGB565_ROW_WRITER
   np_rgb565_row row;np_rgb565_row_begin(&row,y+j,rgb);
#endif
   unsigned jj=(unsigned)(j*j);
   int extent=solid&&jj<=inner?(int)np_root(inner-jj):-1;
   for(int i=-radius;i<=radius;++i){
    if(extent>=0 && i==-extent){
      if(failed || !raster_surface_writable())return;
      fill(x-extent,y+j,extent*2+1,1,np_rgb(rgb));
#if PORTABLE_RGB565_ROW_WRITER
      np_rgb565_row_invalidate(&row);
#endif
      /* A span can cross an input/authority checkpoint. Never continue to
       * its rim or another row after loss of surface or invocation custody. */
      if(failed || !raster_surface_writable())return;
      i=extent;continue;
    }
    unsigned d2=(unsigned)(i*i+j*j);
    if(d2>=(unsigned)(radius*radius))continue;
    unsigned a=opacity;
    if(d2>(unsigned)((radius-1)*(radius-1))) {
      int edge=radius*16-(int)np_root(d2*256u);
      a=(unsigned)edge*opacity/16;
    }
#if PORTABLE_RGB565_ROW_WRITER
    np_rgb565_row_pixel(&row,x+i,a);
#else
    np_pixel(x+i,y+j,rgb,a);
#endif
   }
  }
}
static void np_mask_region(const int32_t *bounds) {
  (void)bounds;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(nova_mask_pending&&raster_record(RS_NP_MASK,(int32_t[8]){0},NULL)){nova_mask_pending=false;return;}
#endif
  if(failed || !nova_mask_pending || !raster_surface_writable())return;
  nova_mask_pending=false;

  int row_first=0,row_end=height();
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_rows(0,&row_first,&row_end);
#endif
  const int screen_w=width(),screen_h=height();
#ifdef PORTABLE_NOVA_MASK_BOUNDS
  const void *mask_pixels=surface.pixels;
  const risc_display_frame_v1 mask_frame=surface.frame;
  const unsigned mask_stride=surface.stride_bytes;
#endif
  for(int y=row_first;y<row_end;++y){
    int dy=y<screen_h-y?y:screen_h-y;
    unsigned ay=dy<=0?0:dy<14?(unsigned)dy*102/14:dy<46?102+(unsigned)(dy-14)*153/32:255;
    if(!(y&7) && !raster_checkpoint())return;
    if(failed || !raster_surface_writable())return;
    int px0=0,py0=y,px1=1,py1=y;native_point(&px0,&py0);native_point(&px1,&py1);
    ptrdiff_t origin=(ptrdiff_t)py0*surface.stride_bytes+(ptrdiff_t)px0*2;
    ptrdiff_t step=(ptrdiff_t)(py1-py0)*surface.stride_bytes+(ptrdiff_t)(px1-px0)*2;
    int x_first=0,x_end=screen_w;
#ifdef PORTABLE_NOVA_MASK_BOUNDS
    /* A row checkpoint still runs before an empty proven-black region. Never
     * inspect the provider or infer black from a previous image. */
    bool bounded=bounds && bounds[4]==1 && bounds[5]==screen_w && bounds[6]==screen_h &&
      surface_format==RISC_DISPLAY_FORMAT_RGB565 && !paper_rotated &&
      surface.pixels==mask_pixels && surface.frame==mask_frame && surface.stride_bytes==mask_stride &&
      width()==screen_w && height()==screen_h;
#ifdef PORTABLE_PAPER_PREFERENCES
    if(paper_flip_ui)bounded=false;
#endif
#ifdef PORTABLE_SAFE_VIEWPORT
    if(content_view.x || content_view.y || content_view.width!=info.width || content_view.height!=info.height)bounded=false;
#endif
    if(!bounded)bounds=NULL; /* Never re-enable after an identity/geometry change. */
    if(bounded){
      if(y<bounds[1] || y>=bounds[3])continue;
      x_first=bounds[0];x_end=bounds[2];
    }
#endif
    for(int x=x_first;x<x_end;++x){
    /* Fully opaque interior needs no per-pixel work. */
    if(y>=46 && y<=screen_h-46 && x>=46 && x<screen_w-46 && screen_w>92){
      x=screen_w-46;if(x>=x_end)break;
    }
    uint8_t *p=(uint8_t*)surface.pixels+origin+(ptrdiff_t)x*step;
    uint16_t c=(uint16_t)(p[0]|p[1]<<8);
    if(!c)continue; /* Exact multiplicative identity, not omitted graphics. */
    int dx=x<screen_w-x?x:screen_w-x;
    unsigned ax=dx<=0?0:dx<14?(unsigned)dx*102/14:dx<46?102+(unsigned)(dx-14)*153/32:255;
    unsigned a=ax*ay/255;
    if(a==255)continue;
    if(dx<22 && dy<22 && (22-dx)*(22-dx)+(22-dy)*(22-dy)>22*22)a=0;
    c=(uint16_t)((((c>>11)&31)*a/255)<<11|(((c>>5)&63)*a/255)<<5|((c&31)*a/255));
    p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
  }
  }
}

static void np_mask(void) {np_mask_region(NULL);}

/* One solid backing for the whole text block, including spaces. Six pixels of
 * smooth alpha falloff hide the rectangle's edge; no glyph dilation, blur
 * buffer, or repeated darkening at overlapping letter shadows is needed. */
static void np_caption_backing(int left,int top,int right,int bottom) {
  const int feather=6,range=feather*16;
  left-=2;top-=2;right+=2;bottom+=2;
  int row_first=top-feather,row_end=bottom+feather+1;
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_rows(0,&row_first,&row_end);
#endif
  for(int y=row_first;y<row_end;++y){
#if PORTABLE_RGB565_ROW_WRITER
    np_rgb565_row row;np_rgb565_row_begin(&row,y,0);
#endif
    for(int x=left-feather;x<=right+feather;++x) {
      int dx=x<left?left-x:x>right?x-right:0;
      int dy=y<top?top-y:y>bottom?y-bottom:0;
      /* Interior coverage is exactly opaque. Avoid integer-root iteration
       * and smoothstep arithmetic over every pixel of the solid backing. */
      unsigned alpha=255;
      if(dx||dy){
        int t=range-(int)np_root((unsigned)(dx*dx+dy*dy)*256u);
        if(t<=0)continue;
        alpha=(unsigned)(t*t*(3*range-2*t))*255u/(unsigned)(range*range*range);
      }
#if PORTABLE_RGB565_ROW_WRITER
      np_rgb565_row_pixel(&row,x,alpha);
#else
      np_pixel(x,y,0,alpha);
#endif
    }
  }
}
/* Shared geometry for immediate drawing and immutable replay metadata. The
 * complete fitting pass is authoritative: empty/space-only/truncated strings
 * must not create an optimistic band that loses glyphs or backing feather. */
typedef struct np_caption_layout {
  int length,start,spacing,left,top,right,bottom;
} np_caption_layout;
static bool np_caption_measure(int y,const char *s,bool clock,np_caption_layout *out) {
  if(!s)return false;
  int length=0,total=0,spacing=clock?21:34,max=width()-16;
  while(length<95 && s[length]){
    unsigned c=(unsigned char)s[length];if(c>='a' && c<='z')c-=32;
    if(c<32 || c>126)c='?';
    const rpt_glyph *g=&rpt_text[(clock?0:95)+c-32];
    int next=total+g->advance_q4+(length?spacing:0);
    if(next>max*16)break;
    total=next;++length;
  }
  if(!length)return false;
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
  *out=(np_caption_layout){length,start,spacing,left,top,right,bottom};
  return right>=left && bottom>=top;
}
static void np_caption(int y,const char *s,bool clock,uint32_t rgb) {
  np_mask();if(!s)return;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_NP_CAPTION,(int32_t[8]){y,clock,(int32_t)rgb},s))return;
#endif
  np_caption_layout layout;
  if(!np_caption_measure(y,s,clock,&layout))return;
  int length=layout.length,start=layout.start,spacing=layout.spacing;
  int left=layout.left,top=layout.top,right=layout.right,bottom=layout.bottom,x;
  if(right>=left && bottom>=top)np_caption_backing(left,top,right,bottom);
  x=start;
  for(int k=0;k<length;++k){
    unsigned c=(unsigned char)s[k];if(c>='a' && c<='z')c-=32;if(c<32 || c>126)c='?';
    const rpt_glyph *g=&rpt_text[(clock?0:95)+c-32];
    int row_first=0,row_end=g->height;
#ifdef PORTABLE_RASTER_SNAPSHOT
    raster_rows((int64_t)y+g->top,&row_first,&row_end);
#endif
    for(unsigned j=(unsigned)row_first;j<(unsigned)row_end;++j){
#if PORTABLE_RGB565_ROW_WRITER
      np_rgb565_row row;np_rgb565_row_begin(&row,y+g->top+(int)j,rgb);
#endif
      for(unsigned i=0;i<g->width;++i){
      unsigned n=j*g->width+i,coverage=(g->bits[n/4]>>(6-2*(n%4)))&3;
#if PORTABLE_RGB565_ROW_WRITER
      np_rgb565_row_pixel(&row,x/16+g->left+(int)i,coverage*85);
#else
      np_pixel(x/16+g->left+(int)i,y+g->top+(int)j,rgb,coverage*85);
#endif
      }
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
static uint32_t np_frame_period_ms(void) {
  return portable_frame_cadence_ms(surface_format,info.flags,info.nominal_refresh_millihz,40u);
}
static const springboard_presentation np_view={sizeof(np_view),np_begin,np_circle,np_icon,np_caption,np_contact,np_clock,np_frame_period_ms};
const springboard_presentation *springboard_presentation_get(void) {
  if(surface_format!=RISC_DISPLAY_FORMAT_RGB565 || width()<160 || height()<160 || width()>1024 || height()>1024)return NULL;
  /* An explicitly provisioned Watch presentation may use an unknown refresh
   * rate. This is a UI choice, not a measured throughput claim. Known slow
   * or retaining panels still take the static path. */
  if((info.nominal_refresh_millihz<20000
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
      && info.nominal_refresh_millihz!=0
#endif
     ) ||
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
#ifdef PORTABLE_SPRINGBOARD_GESTURE_TRACE
  sb_diag_owner=true;portable_springboard_gesture_cancel();
#endif
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
  handoff_pending=true;
  t5_app_input_t initial={0};np_poll(&initial);
#endif
  return &np_view;
}
static void np_poll(t5_app_input_t *out) {
  (void)out;
  portable_touch_sample s;input_take(&s);
#ifdef PORTABLE_SPRINGBOARD_GESTURE_TRACE
  sb_diag_delivery=sb_diag_pending;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick.ui.paper && quick_replay_pending && s.valid && s.down) {
    input_sample=s;input_pending=true;quick_replay_delivery=true;
    s.x=(uint16_t)(quick.ui.start_x*width()/480);s.y=(uint16_t)(quick.ui.start_y*height()/800);s.began=true;
  }
  quick_replay_pending=false;
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
  file_scroll_sample=s;file_scroll_sampled_at=input_delivered_at;
  if(s.x>=width() || s.y>=height()){file_scroll_sample.valid=false;file_scroll_sample.cancelled=true;}
#endif
#ifdef PORTABLE_APP_TOUCH_SCROLL
  paper_scroll_sample=s;paper_scroll_sampled_at=input_delivered_at;
  if(s.x>=width()||s.y>=height()){paper_scroll_sample.valid=false;paper_scroll_sample.cancelled=true;}
#endif
#ifdef PORTABLE_SPRINGBOARD_GESTURE_TRACE
  if(!sb_diag_allowed()||!s.valid||s.cancelled)portable_springboard_gesture_cancel();
  else {sb_diag_delivery.x=s.x;sb_diag_delivery.y=s.y;}
#endif
  nova_contact=(springboard_contact){s.valid,s.down,s.began,s.released,s.cancelled,s.tap_eligible&&!s.moved,(int16_t)s.x,(int16_t)s.y};
  if(s.x>=width() || s.y>=height()){nova_contact.valid=false;nova_contact.cancelled=true;}
#ifndef PORTABLE_HOME_POINTS_NATIVE_UTC
  if(s.released && s.tap_eligible && !s.moved && s.x<44 && s.y<40){out->exit_requested=true;out->buttons=T5_APP_BUTTON_BACK;}
#endif
}
