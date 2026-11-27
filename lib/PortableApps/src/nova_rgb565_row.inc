/* Format-selected owner-local row writer. np_pixel remains the authoritative fallback
 * and executes every original checkpoint pixel (including its post-callback
 * surface selection). No cached pointer crosses that callback without refresh.
 * Row users must invalidate after any other operation which may call services. */
typedef struct np_rgb565_row {
  int y,left,right;
  uint32_t rgb;
  uint16_t packed;
  uint8_t *pixels;
  risc_display_frame_v1 frame;
  ptrdiff_t origin,step;
  bool ready;
} np_rgb565_row;
static void np_rgb565_row_refresh(np_rgb565_row *row) {
  row->ready=false;
  if(failed || surface_format!=RISC_DISPLAY_FORMAT_RGB565)return;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_recording&&!raster_replaying)return;
#endif
  if(!raster_surface_writable() || !surface.pixels)return;
  row->left=0;row->right=width();
  if(row->y<0 || row->y>=height())row->right=0;
#if defined(PORTABLE_TOUCH_SCROLL) && (defined(PORTABLE_FILE_BROWSER_APP) || defined(PORTABLE_APP_TOUCH_SCROLL) || defined(PORTABLE_SETTINGS_LIST_SCROLL))
  if(raster_clip_active){
    if(row->left<raster_clip.x)row->left=raster_clip.x;
    int64_t end=(int64_t)raster_clip.x+raster_clip.width;
    if(end<row->right)row->right=(int)end;
    if(row->y<raster_clip.y || (int64_t)row->y>=(int64_t)raster_clip.y+raster_clip.height)row->right=0;
  }
#endif
  row->pixels=surface.pixels;row->frame=surface.frame;
  row->origin=0;row->step=0;row->ready=true;
  /* Scalar np_pixel rejects clipped coordinates before native_point. Keep
   * that ordering for extreme offscreen rows too; counting still happens in
   * row_pixel and the next checkpoint still uses the authoritative scalar. */
  if(row->right<=row->left)return;
  /* Integer offsets only: do not form an out-of-bounds pointer for clipped x/y.
   * native_point is affine for the selected rotation/flip/safe-content origin. */
  int x0=0,y0=row->y,x1=1,y1=row->y;
  native_point(&x0,&y0);native_point(&x1,&y1);
  row->origin=(ptrdiff_t)y0*surface.stride_bytes+(ptrdiff_t)x0*2;
  row->step=(ptrdiff_t)(y1-y0)*surface.stride_bytes+(ptrdiff_t)(x1-x0)*2;

}
static inline void np_rgb565_row_begin(np_rgb565_row *row,int y,uint32_t rgb) {
  row->y=y;row->rgb=rgb;row->packed=np_rgb(rgb);np_rgb565_row_refresh(row);
}
static inline void np_rgb565_row_invalidate(np_rgb565_row *row) {row->ready=false;}
static inline __attribute__((always_inline)) void np_rgb565_row_pixel(np_rgb565_row *row,int x,unsigned alpha) {
  if(!row->ready){np_pixel(x,row->y,row->rgb,alpha);return;}
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_replaying&&(row->y<raster_band_top||row->y>=raster_band_bottom))return;
#endif
  if(failed)return;
  /* The original routine owns the count/checkpoint/write ordering at512,
   * including format, clip or frame changes caused by that callback. */
  if(!((raster_pixels+1u)&511u)) {
    np_pixel(x,row->y,row->rgb,alpha);
    np_rgb565_row_refresh(row);return;
  }
  if(surface.pixels!=row->pixels || surface.frame!=row->frame ||
     surface_format!=RISC_DISPLAY_FORMAT_RGB565){
    np_rgb565_row_refresh(row);
    if(!row->ready){np_pixel(x,row->y,row->rgb,alpha);return;}
  }
  ++raster_pixels;
  if(x<row->left || x>=row->right || !alpha)return;
  uint8_t *p=row->pixels+row->origin+(ptrdiff_t)x*row->step;
  uint16_t color=alpha>=255?row->packed:portable_rgb565_over(
      (uint16_t)(p[0]|p[1]<<8),row->rgb,alpha);
  p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8);
}
