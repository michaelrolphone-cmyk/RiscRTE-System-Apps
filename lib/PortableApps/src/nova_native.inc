/* Native Springboard raster helpers. Geometry comes from the capability-sized
 * controller; only design measures/font metrics are scaled, before sampling. */
static int np_native_extent(void) { return width()<height()?width():height(); }
static int np_native_measure(int value) { return (value*np_native_extent()+120)/240; }
#include "nova_vector.inc"
static void np_native_span(int y,int left,int right,unsigned alpha) {
  if(y<0 || y>=height() || left>=right || !alpha || failed || !surface.frame)return;
  if(left<0)left=0;
  if(right>width())right=width();
  if(alpha<255) {for(int x=left;x<right;x++)np_pixel(x,y,0,alpha);return;}
  uint8_t *row=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes;
  for(int x=left;x<right;x++) {
#ifdef PORTABLE_CONTEXTS_RASTER
    if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;
#endif
    row[2*x]=row[2*x+1]=0;
  }
}
static void np_native_backing(int left,int top,int right,int bottom) {
  enum { MAX_FEATHER=52 };
  static uint8_t falloff[MAX_FEATHER][MAX_FEATHER];static int cached_feather;
  int feather=np_native_measure(6);if(feather<1)feather=1;
  if(feather>=MAX_FEATHER)feather=MAX_FEATHER-1;
  if(cached_feather!=feather) {
    int range=feather*16;
    for(int dy=0;dy<=feather;dy++)for(int dx=0;dx<=feather;dx++) {
      int t=range-(int)np_root((unsigned)(dx*dx+dy*dy)*256u);
      falloff[dy][dx]=t<=0?0:(uint8_t)((uint64_t)(t*t)*(unsigned)(3*range-2*t)*255u/(unsigned)(range*range*range));
    }
    cached_feather=feather;
  }
  int pad=np_native_measure(2);left-=pad;top-=pad;right+=pad;bottom+=pad;
  for(int y=top-feather;y<=bottom+feather;y++) {
    int dy=y<top?top-y:y>bottom?y-bottom:0;
    np_native_span(y,left,right+1,falloff[dy][0]);
    for(int dx=1;dx<=feather;dx++) {
      unsigned alpha=falloff[dy][dx];if(!alpha)continue;
      np_pixel(left-dx,y,0,alpha);np_pixel(right+dx,y,0,alpha);
    }
  }
}
static void np_native_color_span(int y,int left,int right,uint16_t color) {
  if(y<0 || y>=height() || left>=right || failed || !surface.frame)return;
  if(left<0)left=0;
  if(right>width())right=width();
  uint8_t *row=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes;
  for(int x=left;x<right;x++) {
#ifdef PORTABLE_CONTEXTS_RASTER
    if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;
#endif
    row[2*x]=(uint8_t)color;row[2*x+1]=(uint8_t)(color>>8);
  }
}
static void np_native_circle(int x,int y,int radius,uint32_t rgb,unsigned opacity) {
  const unsigned outer=(unsigned)(radius*radius),inner=(unsigned)((radius-1)*(radius-1));
  const uint16_t color=np_rgb(rgb);
  for(int j=-radius+1;j<radius;j++) {
    unsigned yy=(unsigned)(j*j);int extent=(int)np_root(outer-yy-1u);
    int solid=yy<=inner?(int)np_root(inner-yy):-1;
    if(solid>=0) {
      if(opacity==255)np_native_color_span(y+j,x-solid,x+solid+1,color);
      else for(int i=-solid;i<=solid;i++)np_pixel(x+i,y+j,rgb,opacity);
    }
    for(int i=solid+1;i<=extent;i++) {
      int edge=radius*16-(int)np_root(((unsigned)(i*i)+yy)*256u);
      unsigned a=edge>=16?opacity:edge>0?(unsigned)edge*opacity/16:0;
      np_pixel(x-i,y+j,rgb,a);if(i)np_pixel(x+i,y+j,rgb,a);
    }
  }
}
static const np_vector_glyph *np_native_text_glyph(unsigned c,bool clock) {
 if(c>='a'&&c<='z')c-=32;
 if(c<32||c>126)c='?';
 return &np_vector_glyphs[np_vector_text[(clock?0:95)+c-32]];
}
static void np_native_caption(int y,const char *s,bool clock,uint32_t rgb) {
 if(!s)return;
 int extent=np_native_extent();if(extent>960)extent=960;
 int font=((clock?11:15)*extent+120)/240;if(font<1)font=1;
 int spacing=(clock?21:34)*extent/240,length=0,total=0,max=width()-np_native_measure(16);
 while(length<95&&s[length]) {
  const np_vector_glyph*g=np_native_text_glyph((unsigned char)s[length],clock);
  int next=total+(int)g->advance*font*16/g->units+(length?spacing:0);
  if(next>max*16)break;
  total=next;length++;
 }
 if(!length)return;
 int start=(width()*16-total)/2,x=start,left=width(),top=height(),right=-1,bottom=-1;
 for(int k=0;k<length;k++) {
  const np_vector_glyph*g=np_native_text_glyph((unsigned char)s[k],clock);
  if(g->length) {
   int w=((g->right-g->left)*font+g->units-1)/g->units,h=((g->top-g->bottom)*font+g->units-1)/g->units;
   int l=x/16+g->left*font/g->units,t=y+(g->ascent*font+g->units-1)/g->units-(g->top*font+g->units-1)/g->units;
   if(l<left)left=l;
   if(t<top)top=t;
   if(l+w-1>right)right=l+w-1;
   if(t+h-1>bottom)bottom=t+h-1;
  }
  x+=(int)g->advance*font*16/g->units+spacing;
 }
 if(right>=left&&bottom>=top)np_native_backing(left,top,right,bottom);
 x=start;
 for(int k=0;k<length;k++) {
  const np_vector_glyph*g=np_native_text_glyph((unsigned char)s[k],clock);
  int w=((g->right-g->left)*font+g->units-1)/g->units,h=((g->top-g->bottom)*font+g->units-1)/g->units;
  (void)np_vector_draw(x/16+g->left*font/g->units,y+(g->ascent*font+g->units-1)/g->units-(g->top*font+g->units-1)/g->units,w,h,g,rgb,255);
  x+=(int)g->advance*font*16/g->units+spacing;
 }
}
static unsigned np_native_edge(int distance,int inner,int outer) {
  return distance<=0?0:distance<inner?(unsigned)distance*102u/(unsigned)inner:
         distance<outer?102u+(unsigned)(distance-inner)*153u/(unsigned)(outer-inner):255;
}
static void np_native_mask(void) {
  const int w=width(),h=height(),inner=np_native_measure(14),outer=np_native_measure(46),corner=np_native_measure(22);
  /* Geometry is stable across frames. Cache only the 1-D edge coverage, never
   * a completed view or icon image. This removes a division from each pixel. */
  static uint8_t edge[2048];static int edge_width,edge_extent;
  if(w>2048)return;
  if(edge_width!=w || edge_extent!=np_native_extent()){for(int x=0;x<w;x++)edge[x]=(uint8_t)np_native_edge(x<w-x?x:w-x,inner,outer);edge_width=w;edge_extent=np_native_extent();}
  for(int y=0;y<h;y++) {
    int dy=y<h-y?y:h-y;unsigned ay=np_native_edge(dy,inner,outer);
    uint8_t *row=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes;
    for(int x=0;x<w;x++) {
      if(ay==255&&x==outer){x=w-outer+1;if(x>=w)break;}
#ifdef PORTABLE_CONTEXTS_RASTER
      if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;
#endif
      uint16_t c=(uint16_t)(row[2*x]|row[2*x+1]<<8);if(!c)continue;
      unsigned alpha=(unsigned)edge[x]*ay/255u;int dx=x<w-x?x:w-x;
      if(dx<corner&&dy<corner&&(corner-dx)*(corner-dx)+(corner-dy)*(corner-dy)>corner*corner)alpha=0;
      if(alpha==255)continue;
      c=(uint16_t)((((c>>11)&31)*alpha/255)<<11|(((c>>5)&63)*alpha/255)<<5|((c&31)*alpha/255));
      row[2*x]=(uint8_t)c;row[2*x+1]=(uint8_t)(c>>8);
    }
  }
}
