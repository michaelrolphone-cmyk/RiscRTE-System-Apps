/* Reusable software surfaces. Captures and blits are ordered with other paint
 * operations, but never acquire a display lease. A layer becomes immutable at
 * END; replacement allocates a new generation, retained by pending commands. */
static void raster_layer_drop(portable_raster_layer *p) {
 if(p && !--p->refs)free(p);
}
void portable_raster_layer_release(portable_raster_layer **p) {
 if(!p||raster_terminal())return;
 raster_layer_drop(*p);*p=NULL;
}
bool portable_raster_layer_valid(const portable_raster_layer *p) {
 return p && !failed && p->width==info.width && p->height==info.height &&
  p->format==surface_format && p->rotated==paper_rotated &&
#ifdef PORTABLE_PAPER_PREFERENCES
  p->flipped==paper_flip_ui &&
#endif
  (p->complete || raster_recording);
}
static risc_display_surface_v1 raster_layer_surface(portable_raster_layer *p) {
 return (risc_display_surface_v1){.pixels=p->pixels,.width=p->width,.height=p->height,
  .stride_bytes=p->stride,.size_bytes=p->stride*p->height,.pixel_format=p->format};
}
bool portable_raster_layer_begin(portable_raster_layer **slot) {
 if(!slot||failed||!raster_recording||raster_replaying||raster_layer_recording)return false;
 unsigned stride=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 if(!stride||info.height>(SIZE_MAX-sizeof(portable_raster_layer))/stride)return false;
 portable_raster_layer *p=malloc(sizeof(*p)+(size_t)stride*info.height);
 if(!p)return false;
 *p=(portable_raster_layer){.refs=1,.width=info.width,.height=info.height,.stride=stride,
  .format=surface_format,.rotated=paper_rotated};
#ifdef PORTABLE_PAPER_PREFERENCES
 p->flipped=paper_flip_ui;
#endif
 if(!raster_record(RS_LAYER_BEGIN,(int32_t[8]){0},NULL)) {if(!raster_terminal())raster_layer_drop(p);return false;}
 if(failed){if(!raster_terminal())raster_layer_drop(p);return false;}
 raster_tail->layer=p;++p->refs;
 portable_raster_layer_release(slot);*slot=p;raster_layer_recording=true;
 /* Clear in packed native spans through the normal bounded replay. */
 fill(0,0,width(),height(),0xffff);
 return true;
}
void portable_raster_layer_end(void) {
 if(failed||!raster_layer_recording)return;
 raster_layer_recording=false;
 if(raster_record(RS_LAYER_END,(int32_t[8]){0},NULL))return;
 if(raster_layer_paint){raster_layer_paint->complete=true;raster_layer_paint=NULL;surface=raster_layer_parent;}
}
/* Copy packed MONO1 spans without per-pixel transforms. Edge bytes retain the
 * destination's untouched bits; a shifted span reads only its source bytes. */
static void raster_mono_span(uint8_t *dst,unsigned dx,const uint8_t *src,unsigned sx,unsigned count) {
 while(count) {
  unsigned offset=dx&7u,n=8u-offset;if(n>count)n=count;
  if(!offset && !(sx&7u) && count>=8u) {
   unsigned bytes=count/8u;memcpy(dst+dx/8u,src+sx/8u,bytes);
   dx+=bytes*8u;sx+=bytes*8u;count-=bytes*8u;continue;
  }
  unsigned bits=(unsigned)src[sx/8u]<<8u;
  if((sx&7u)+n>8u)bits|=src[sx/8u+1u];
  unsigned value=((bits<<(sx&7u))>>8u)&255u;
  uint8_t mask=(uint8_t)((255u>>offset)&(255u<<(8u-offset-n)));
  dst[dx/8u]=(dst[dx/8u]&~mask)|((value>>offset)&mask);
  dx+=n;sx+=n;count-=n;
 }
}
static bool raster_layer_rect(const raster_command *c,int *sx,int *sy,int *dx,int *dy,int *w,int *h) {
 *sx=c->args[0];*sy=c->args[1];*w=c->args[2];*h=c->args[3];*dx=c->args[4];*dy=c->args[5];
 if(*w<=0||*h<=0)return false;
 /* API coordinates are screen bounded; clip both source and destination. */
 int left=0,top=0,right=width(),bottom=height();
#ifdef PORTABLE_TOUCH_SCROLL
 if(c->clipped){if(left<c->clip.x)left=c->clip.x;if(top<c->clip.y)top=c->clip.y;
  if(right>c->clip.x+c->clip.width)right=c->clip.x+c->clip.width;
  if(bottom>c->clip.y+c->clip.height)bottom=c->clip.y+c->clip.height;}
#endif
 if(*sx<0){int n=-*sx;*sx=0;*dx+=n;*w-=n;}
 if(*sy<0){int n=-*sy;*sy=0;*dy+=n;*h-=n;}
 if(*dx<left){int n=left-*dx;*dx=left;*sx+=n;*w-=n;}
 if(*dy<top){int n=top-*dy;*dy=top;*sy+=n;*h-=n;}
 if(*w>width()-*sx)*w=width()-*sx;
 if(*h>height()-*sy)*h=height()-*sy;
 if(*w>right-*dx)*w=right-*dx;
 if(*h>bottom-*dy)*h=bottom-*dy;
 if(*w<=0||*h<=0)return false;
 int sr=*sx+*w-1,sb=*sy+*h-1,dr=*dx+*w-1,db=*dy+*h-1;
 native_point(sx,sy);native_point(&sr,&sb);native_point(dx,dy);native_point(&dr,&db);
 if(*sx>sr){int n=*sx;*sx=sr;sr=n;}if(*sy>sb){int n=*sy;*sy=sb;sb=n;}
 if(*dx>dr){int n=*dx;*dx=dr;dr=n;}if(*dy>db){int n=*dy;*dy=db;db=n;}
 *w=sr-*sx+1;*h=sb-*sy+1;return true;
}
static void raster_layer_execute(const raster_command *c) {
 if(c->kind==RS_LAYER_BEGIN){
  raster_layer_parent=surface;raster_layer_paint=c->layer;surface=raster_layer_surface(c->layer);return;
 }
 if(c->kind==RS_LAYER_END){
  if(raster_layer_paint){raster_layer_paint->complete=true;raster_layer_paint=NULL;surface=raster_layer_parent;}return;
 }
 if(!c->layer||!c->layer->complete)return;
 int sx,sy,dx,dy,w,h;
 if(!raster_layer_rect(c,&sx,&sy,&dx,&dy,&w,&h))return;
 int first=0,end=h;
 if(raster_replaying){if(dy<raster_band_top)first=raster_band_top-dy;if(dy+end>raster_band_bottom)end=raster_band_bottom-dy;}
 for(int row=first;row<end;row++) {
  uint8_t *dst=(uint8_t*)surface.pixels+(size_t)(dy+row)*surface.stride_bytes;
  const uint8_t *src=c->layer->pixels+(size_t)(sy+row)*c->layer->stride;
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1)raster_mono_span(dst,(unsigned)dx,src,(unsigned)sx,(unsigned)w);
  else memcpy(dst+dx*2,src+sx*2,(size_t)w*2);
  raster_pixels+=(unsigned)w;if(!raster_checkpoint())return;
 }
}
void portable_raster_layer_blit(portable_raster_layer *p,int sx,int sy,int w,int h,int x,int y) {
 if(!portable_raster_layer_valid(p)||raster_layer_recording)return;
 int32_t args[8]={sx,sy,w,h,x,y};
 if(raster_record(RS_LAYER_BLIT,args,NULL)) {
  if(!failed){raster_tail->layer=p;++p->refs;}return;
 }
 raster_command c={.kind=RS_LAYER_BLIT,.layer=p};memcpy(c.args,args,sizeof(args));
#ifdef PORTABLE_TOUCH_SCROLL
 c.clipped=raster_clip_active;c.clip=raster_clip;
#endif
 if(raster_surface_writable())raster_layer_execute(&c);
}
