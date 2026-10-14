/* Frozen renderer from 72f435fcee376ec62d17ece6180753385ea547d9 for pixel equivalence. */
static void reference_np_circle(int x,int y,int radius,uint32_t rgb,uint8_t opacity) {
  if(radius<1 || radius>64)return;
  for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i){
    int d=(int)np_root((unsigned)(i*i+j*j)*256u);
    int edge=radius*16-d;
    unsigned a=edge>=16?opacity:edge>0?(unsigned)edge*opacity/16:0;
    np_pixel(x+i,y+j,rgb,a);
  }
}
static void reference_np_mask(void) {
#ifdef PORTABLE_CONTEXTS_RASTER
  if(failed)return;
#endif
  if(!nova_mask_pending || !surface.frame)return;
  nova_mask_pending=false;

  for(int y=0;y<height();++y)for(int x=0;x<width();++x){
#ifdef PORTABLE_CONTEXTS_RASTER
    if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;
#endif
    int lx=x,ly=y;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    /* Use the same inverse as touch and logical-pixel rasterization. The
     * original edge feather keeps its proportions on a taller native panel. */
    lx=(int)((uint32_t)x*WATCH_LAYOUT_WIDTH/native_watch_layout.physical_width);
    ly=(int)((uint32_t)y*WATCH_LAYOUT_WIDTH/native_watch_layout.physical_width);
#endif
    int dx=lx<np_width()-lx?lx:np_width()-lx,dy=ly<np_height()-ly?ly:np_height()-ly;
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
static void reference_native_watch_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
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
