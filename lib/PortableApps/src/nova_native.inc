#include "PortableRgb565Blend.h"
/* Native-resolution primitives share the submitted surface and custody gate.
 * No intermediate bitmap, scaling state, allocation or provider calls. */
/* Snapshot bands are physical; native primitives are content-local. */
static inline int nu_native_band_offset(bool columns) {
#ifdef PORTABLE_WATCH_SAFE_INSETS
 return columns?(int)content_view.x:(int)content_view.y;
#else
 (void)columns;return 0;
#endif
}
static void nu_native_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
 if(failed || (!(++raster_pixels&511u)&&!raster_checkpoint()))return;
 if(!raster_surface_writable()||x<0||y<0||x>=width()||y>=height()||!alpha)return;
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_replaying){int axis=(raster_band_columns?x:y)+nu_native_band_offset(raster_band_columns);if(axis<raster_band_top||axis>=raster_band_bottom)return;}
#endif
 if(surface_format==RISC_DISPLAY_FORMAT_MONO1){np_pixel(x,y,rgb,alpha);return;}
 native_point(&x,&y);
 uint8_t *p=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes+(size_t)x*2;
 uint16_t c;
 if(alpha>=255)c=portable_rgb565_pack(rgb);
 else c=portable_rgb565_over((uint16_t)(p[0]|p[1]<<8),rgb,alpha);
  p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);
}
void portable_nova_native_round(int x,int y,int w,int h,int radius,uint32_t rgb) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_NU_NATIVE_ROUND,(int32_t[8]){x,y,w,h,radius,(int32_t)rgb},NULL))return;
#endif
 if(w<=0||h<=0||w>2048||h>2048)return;
 if(radius<0)radius=0;
 if(radius>w/2)radius=w/2;
 if(radius>h/2)radius=h/2;
 int first=y<0?-y:0,end=h;
 if((int64_t)y+end>height())end=height()-y;
 int left=x<0?-x:0,right=w;
 if((int64_t)x+right>width())right=width()-x;
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_band_columns)raster_rows((int64_t)y+nu_native_band_offset(false),&first,&end);
#endif
 for(int j=first;j<end;j++)for(int i=left;i<right;i++){
  int dx=i<radius?radius-i:i>=w-radius?i-(w-radius-1):0;
  int dy=j<radius?radius-j:j>=h-radius?j-(h-radius-1):0;
  if(!dx||!dy||dx*dx+dy*dy<=radius*radius)nu_native_pixel(x+i,y+j,rgb,255);
 }
}
void portable_nova_native_fill(int x,int y,int w,int h,uint32_t rgb) {
 portable_nova_native_round(x,y,w,h,0,rgb);
}
#ifdef PORTABLE_RASTER_SNAPSHOT
static bool nu_native_text_bounds(unsigned face,int x,int y,int max_width,const char*s,int *left,int *top,int *right,int *bottom){
 if(!s||face>NOVA_FACE_ORBITRON_10||max_width<=0)return false;
 unsigned source=face==NOVA_FACE_ORBITRON_10?0u:face,scale=face==NOVA_FACE_ORBITRON_10?183u:256u;
 if(source>=sizeof(rps_text)/sizeof(rps_text[0])/95u)return false;
 bool have=false;int pen=x*16;
 for(unsigned k=0;k<96&&s[k];k++){
  unsigned c=(unsigned char)s[k];if(c<32||c>126)c='?';const rps_glyph*g=&rps_text[source*95+c-32];
  int advance=(int)(g->advance_q4*scale/256u);if(pen/16+(advance+15)/16>x+max_width)break;
  int w=(int)(g->width*scale/256u),h=(int)(g->height*scale/256u);
  if(w<1&&g->width)w=1;
  if(h<1&&g->height)h=1;
  int l=pen/16+(int)g->left*(int)scale/256,t=y+(int)g->top*(int)scale/256,r=l+w,b=t+h;
  if(l<x)l=x;
  if(r>x+max_width)r=x+max_width;
  if(l<r&&h){if(!have){*left=l;*right=r;*top=t;*bottom=b;have=true;}else{if(l<*left)*left=l;if(r>*right)*right=r;if(t<*top)*top=t;if(b>*bottom)*bottom=b;}}
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
 int pen=x*16;
 for(unsigned k=0;k<96&&s[k];k++){
  unsigned c=(unsigned char)s[k];if(c<32||c>126)c='?';const rps_glyph*g=&rps_text[source*95+c-32];
  int advance=(int)(g->advance_q4*scale/256u);if(pen/16+(advance+15)/16>x+max_width)break;
  int gw=(int)(g->width*scale/256u),gh=(int)(g->height*scale/256u);
  if(gw<1&&g->width)gw=1;
  if(gh<1&&g->height)gh=1;
  int first=0,end=gh;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(!raster_band_columns)raster_rows((int64_t)y+((int)g->top*(int)scale/256)+nu_native_band_offset(false),&first,&end);
#endif
  int col_first=0,col_end=gw;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_band_columns)raster_rows((int64_t)pen/16+((int)g->left*(int)scale/256)+nu_native_band_offset(true),&col_first,&col_end);
#endif
  for(int j=first;j<end;j++)for(int i=col_first;i<col_end;i++){
   unsigned sx=(unsigned)i*256u/scale,sy=(unsigned)j*256u/scale;if(sx>=g->width||sy>=g->height)continue;
   unsigned n=sy*g->width+sx,a=(g->bits[n/4]>>(6-2*(n%4)))&3;
   int px=pen/16+((int)g->left*(int)scale/256)+i,py=y+((int)g->top*(int)scale/256)+j;
   if(px>=x&&px<x+max_width)nu_native_pixel(px,py,rgb,a*85);
  }
  pen+=advance;
 }
}
