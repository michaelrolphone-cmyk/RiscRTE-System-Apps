/* The 240px reference layout is a design space, not a fixed physical canvas.
 * Selected color profiles lay it out in the capability-safe square. Glyphs are
 * rasterized at native font sizes; no framebuffer enlargement or input scaling
 * is delegated to the display provider. Legacy and paper profiles are unchanged. */
#ifdef PORTABLE_SETTINGS_NATIVE_GEOMETRY
#include "../settings_fonts/native.inc"
#endif
#include "PortableRoundViewport.h"
static bool sv_round(void) {
#ifdef PORTABLE_SETTINGS_ROUND_GEOMETRY
 return surface_format==RISC_DISPLAY_FORMAT_RGB565 && (info.flags&RISC_DISPLAY_INFO_ROUND);
#else
 return false;
#endif
}
static int sv_native_side(void) {
#ifdef PORTABLE_SETTINGS_NATIVE_GEOMETRY
  if(surface_format!=RISC_DISPLAY_FORMAT_MONO1){int side=width()<height()?width():height();if(side>=300)return side;}
#endif
  return 240;
}
static int sv_native_edge(int value) {int n=value*sv_native_side();return n>=0?n/240:-((-n+239)/240);}
static int sv_native_x(int x){return (width()-sv_native_side())/2+sv_native_edge(x);}
static int sv_native_y(int y){return (height()-sv_native_side())/2+sv_native_edge(y);}
static int sv_layout_axis(int native,int extent) {
  int n=native-(extent-sv_native_side())/2;
  /* Invert floor-mapped cell edges, including negative/outside coordinates. */
  return n>=0?((n+1)*240-1)/sv_native_side():-((-n*240+sv_native_side()-1)/sv_native_side());
}
static int sv_layout_x(int x){return sv_layout_axis(x,width());}
static int sv_layout_y(int y){return sv_layout_axis(y,height());}
static int sv_native_spacing(int spacing){return spacing*sv_native_side()/240;}
static const rps_glyph *sv_glyph_at(unsigned face,unsigned ch,int side) {
#ifdef PORTABLE_SETTINGS_NATIVE_GEOMETRY
  if(side>240){unsigned bank=0;
    while(bank+1<sizeof(rps_native_sides)/sizeof(*rps_native_sides)&&
      (sv_round()?side>=rps_native_sides[bank+1]:side>(rps_native_sides[bank]+rps_native_sides[bank+1])/2))++bank;
    return &rps_native_text[bank*570+face*95+ch-32];}
#endif
  (void)side;return &rps_text[face*95+ch-32];
}
static const rps_glyph *sv_glyph(unsigned face,unsigned ch){return sv_glyph_at(face,ch,sv_native_side());}
/* Reserve three rows only at the density where their status band can hold
 * the complete retained status font plus three physical pixels on each side. */
static bool sv_round_compact_rows(void){
 if(!sv_round())return false;
 static int cached_side,cached_compact;int side=sv_native_side();if(cached_side==side)return cached_compact!=0;
 int top=999,bottom=-999;for(unsigned ch=33;ch<=126;++ch){const rps_glyph*g=sv_glyph(3,ch);if(!g->height)continue;if(g->top<top)top=g->top;if(g->top+g->height>bottom)bottom=g->top+g->height;}
 int minimum=(44*240+side-1)/side,action_y=194;
 if(minimum>26)action_y-=(minimum-26)/2;
 cached_compact=sv_native_edge(action_y)-sv_native_edge(180)<bottom-top+6;cached_side=side;return cached_compact!=0;
}
static void sv_native_button_geometry(int*x,int*y,int*w,int*h) {
  if(sv_round()&&*y==200&&*h==26)*y=sv_round_compact_rows()?190:194;
  if(sv_native_side()>240){int minimum=(44*240+sv_native_side()-1)/sv_native_side();
    if(*h<minimum){*y-=(minimum-*h)/2;*h=minimum;}}
  if(sv_round()){
    portable_round_span band;
    if(portable_round_band(width(),height(),sv_native_y(*y),sv_native_y(*y+*h),8,&band)){
      int left=sv_layout_x(band.left),right=sv_layout_x(band.right-1)+1;
      while(sv_native_x(left)<band.left)++left;
      while(sv_native_x(right)>band.right)--right;
      int end=*x+*w;if(*x<left)*x=left;if(end>right)end=right;*w=end-*x;
    }
  }
}
#ifdef PORTABLE_RASTER_SNAPSHOT
static void sv_layout_rows(int origin,int*first,int*end){
  if(!raster_replaying)return;
  int top=sv_layout_y(raster_band_top)-origin,bottom=sv_layout_y(raster_band_bottom-1)+1-origin;
  if(top>*first)*first=top<*end?top:*end;
  if(bottom<*end)*end=bottom>*first?bottom:*first;
}
#endif
