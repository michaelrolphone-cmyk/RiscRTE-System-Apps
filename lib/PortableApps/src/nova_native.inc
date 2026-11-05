/* Native-resolution primitives share the submitted surface and custody gate.
 * No intermediate bitmap, scaling state, allocation or provider calls. */
static void nu_native_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
 if(failed || (!(++raster_pixels&511u)&&!raster_checkpoint()))return;
 if(!raster_surface_writable()||x<0||y<0||x>=width()||y>=height()||!alpha)return;
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_replaying){int axis=raster_band_columns?x:y;if(axis<raster_band_top||axis>=raster_band_bottom)return;}
#endif
#ifdef PORTABLE_TOUCH_SCROLL
 if(raster_clip_active&&!portable_scroll_inside(&raster_clip,x,y))return;
#endif
 if(surface_format==RISC_DISPLAY_FORMAT_MONO1){np_pixel(x,y,rgb,alpha);return;}
 native_point(&x,&y);
 uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+(size_t)x*2;
 uint16_t old=(uint16_t)(p[0]|p[1]<<8);if(alpha>255)alpha=255;
 unsigned r=(((old>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;
 unsigned g=(((old>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;
 unsigned b=((old&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;
 uint16_t c=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
}
void portable_nova_native_round(int x,int y,int w,int h,int radius,uint32_t rgb) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_NU_NATIVE_ROUND,(int32_t[8]){x,y,w,h,radius,(int32_t)rgb},NULL))return;
#endif
 if(w<=0||h<=0)return;
 if(radius<0)radius=0;
 if(radius>w/2)radius=w/2;
 if(radius>h/2)radius=h/2;
 /* Clip native endpoints in a wide type before forming local offsets. */
 int64_t x0=x<0?0:x,y0=y<0?0:y,x1=(int64_t)x+w,y1=(int64_t)y+h;
 if(x1>width())x1=width();
 if(y1>height())y1=height();
 if(x0>=x1||y0>=y1)return;
 int first=(int)(y0-y),end=(int)(y1-y);
 int left=(int)(x0-x),right=(int)(x1-x);
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_band_columns)raster_rows(y,&first,&end);
#endif
 for(int j=first;j<end;j++)for(int i=left;i<right;i++){
  int dx=i<radius?radius-i:i>=w-radius?i-(w-radius-1):0;
  int dy=j<radius?radius-j:j>=h-radius?j-(h-radius-1):0;
  if(!dx||!dy||(int64_t)dx*dx+(int64_t)dy*dy<=(int64_t)radius*radius)nu_native_pixel(x+i,y+j,rgb,255);
 }
}
void portable_nova_native_fill(int x,int y,int w,int h,uint32_t rgb) {
 portable_nova_native_round(x,y,w,h,0,rgb);
}
/* Clip local glyph coordinates before sampling. Wide endpoints handle hostile
 * offscreen positions; every resulting offset is within the original glyph. */
static void nu_native_clip_axis(int64_t origin,int extent,int maximum,int *first,int *end) {
 int64_t lo=origin<0?-origin:0,hi=(int64_t)maximum-origin;
 if(lo>extent)lo=extent;
 if(hi<lo)hi=lo;
 if(hi>extent)hi=extent;
 *first=(int)lo;*end=(int)hi;
}
#ifdef PORTABLE_RASTER_SNAPSHOT
static bool nu_native_text_bounds(unsigned face,int x,int y,int max_width,const char*s,int *left,int *top,int *right,int *bottom){
 if(!s||face>NOVA_FACE_ORBITRON_10||max_width<=0)return false;
 unsigned source=face==NOVA_FACE_ORBITRON_10?0u:face,scale=face==NOVA_FACE_ORBITRON_10?183u:256u;
 if(source>=sizeof(rps_text)/sizeof(rps_text[0])/95u)return false;
 bool have=false;int64_t pen=(int64_t)x*16;
 for(unsigned k=0;k<96&&s[k];k++){
  unsigned c=(unsigned char)s[k];if(c<32||c>126)c='?';const rps_glyph*g=&rps_text[source*95+c-32];
  int advance=(int)(g->advance_q4*scale/256u);if(pen/16+(advance+15)/16>(int64_t)x+max_width)break;
  int w=(int)(g->width*scale/256u),h=(int)(g->height*scale/256u);
  if(w<1&&g->width)w=1;
  if(h<1&&g->height)h=1;
  int64_t l=pen/16+(int)g->left*(int)scale/256,t=(int64_t)y+(int)g->top*(int)scale/256,r=l+w,b=t+h;
  if(l<x)l=x;
  if(r>(int64_t)x+max_width)r=(int64_t)x+max_width;
  if(l<0)l=0;
  if(t<0)t=0;
  if(r>width())r=width();
  if(b>height())b=height();
  if(l<r&&t<b){if(!have){*left=l;*right=r;*top=t;*bottom=b;have=true;}else{if(l<*left)*left=l;if(r>*right)*right=r;if(t<*top)*top=t;if(b>*bottom)*bottom=b;}}
  pen+=advance;
 }
 return have;
}
#endif
void portable_nova_native_text(unsigned face,int x,int y,int max_width,const char *s,uint32_t rgb) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_NU_NATIVE_TEXT,(int32_t[8]){(int32_t)face,x,y,max_width,(int32_t)rgb},s))return;
#endif
 if(!s||face>NOVA_FACE_ORBITRON_10||max_width<=0)return;
 unsigned source=face==NOVA_FACE_ORBITRON_10?0u:face,scale=face==NOVA_FACE_ORBITRON_10?183u:256u;
#ifdef PORTABLE_NOVA_TEXT_FACES_012
 if(source>=sizeof(rps_text)/sizeof(rps_text[0])/95u)return;
#endif
 int64_t pen=(int64_t)x*16;
 for(unsigned k=0;k<96&&s[k];k++){
  unsigned c=(unsigned char)s[k];if(c<32||c>126)c='?';const rps_glyph*g=&rps_text[source*95+c-32];
  int advance=(int)(g->advance_q4*scale/256u);if(pen/16+(advance+15)/16>(int64_t)x+max_width)break;
  int gw=(int)(g->width*scale/256u),gh=(int)(g->height*scale/256u);
  if(gw<1&&g->width)gw=1;
  if(gh<1&&g->height)gh=1;
  int64_t gx=pen/16+((int)g->left*(int)scale/256),gy=(int64_t)y+((int)g->top*(int)scale/256);
  int first,end,col_first,col_end;
  nu_native_clip_axis(gy,gh,height(),&first,&end);
  nu_native_clip_axis(gx,gw,width(),&col_first,&col_end);
  int64_t clip_left=(int64_t)x-gx,clip_right=(int64_t)x+max_width-gx;
  if(clip_left>col_first)col_first=clip_left<col_end?(int)clip_left:col_end;
  if(clip_right<col_end)col_end=clip_right>col_first?(int)clip_right:col_first;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_band_columns)raster_rows(gx,&col_first,&col_end);
  else raster_rows(gy,&first,&end);
#endif
  for(int j=first;j<end;j++)for(int i=col_first;i<col_end;i++){
   unsigned sx=(unsigned)i*256u/scale,sy=(unsigned)j*256u/scale;if(sx>=g->width||sy>=g->height)continue;
   unsigned n=sy*g->width+sx,a=(g->bits[n/4]>>(6-2*(n%4)))&3;
   nu_native_pixel((int)(gx+i),(int)(gy+j),rgb,a*85);
  }
  pen+=advance;
 }
}
/* Shared native icon lookup retains the selected profile's original OFL
 * coverage bytes. Size describes a glyph box, never a completed UI surface. */
static const rpi_glyph *nu_native_icon_glyph(const char *name) {
 if(!name)return NULL;
 for(unsigned i=0;i<sizeof(rpi_icons)/sizeof(rpi_icons[0]);i++)
  if(!strcmp(rpi_icons[i].name,name))return &rpi_icons[i];
#if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL) || defined(PORTABLE_RESIDENT_LOADING)
 if(!strcmp(name,rpi_springboard_gamepad.name))return &rpi_springboard_gamepad;
#endif
 return NULL;
}
_Static_assert((UINT64_C(2)*UINT16_MAX-1)*UINT8_MAX*128<=UINT32_MAX,
 "Native icon sampling must fit uint32 without division helpers");
static bool nu_native_icon_box(int x,int y,int size,const char *name,
 const rpi_glyph **glyph,int64_t *left,int64_t *top,int *w,int *h) {
 if(size<1||size>UINT16_MAX)return false;
 const rpi_glyph*g=nu_native_icon_glyph(name);if(!g)return false;
 unsigned maximum=g->width>g->height?g->width:g->height;
 if(!maximum)return false;
 *w=g->width*size/(int)maximum;*h=g->height*size/(int)maximum;
 if(*w<1)*w=1;
 if(*h<1)*h=1;
 *left=(int64_t)x+(size-*w)/2;*top=(int64_t)y+(size-*h)/2;*glyph=g;return true;
}
bool portable_nova_native_icon(int x,int y,int size,const char *name,uint32_t rgb) {
 const rpi_glyph*g;int64_t left,top;int w,h;
 if(!nu_native_icon_box(x,y,size,name,&g,&left,&top,&w,&h))return false;
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_NU_NATIVE_ICON,(int32_t[8]){x,y,size,(int32_t)rgb},name))return true;
#endif
 int first,end,col_first,col_end;
 nu_native_clip_axis(top,h,height(),&first,&end);
 nu_native_clip_axis(left,w,width(),&col_first,&col_end);
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_band_columns)raster_rows(left,&col_first,&col_end);
 else raster_rows(top,&first,&end);
#endif
 for(int j=first;j<end;j++)for(int i=col_first;i<col_end;i++){
  /* uint8 glyph dimensions and uint16 output extents keep these products
   * within uint32, without target 64-bit division helpers. */
  int u=(int)(((2u*(unsigned)i+1u)*g->width*128u)/(unsigned)w)-128;
  int v=(int)(((2u*(unsigned)j+1u)*g->height*128u)/(unsigned)h)-128;
  int sx=u>=0?u/256:-1,sy=v>=0?v/256:-1;
  unsigned fx=(unsigned)(u-sx*256),fy=(unsigned)(v-sy*256);
  unsigned a=np_coverage(g,sx,sy)*(256-fx)+np_coverage(g,sx+1,sy)*fx;
  unsigned b=np_coverage(g,sx,sy+1)*(256-fx)+np_coverage(g,sx+1,sy+1)*fx;
  unsigned coverage=a*(256-fy)+b*fy;
  nu_native_pixel((int)(left+i),(int)(top+j),rgb,(coverage*255u+98304u)/196608u);
 }
 return true;
}
