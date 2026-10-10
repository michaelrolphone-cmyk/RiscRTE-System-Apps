static void raster_free_commands(void) {
 while(raster_commands){raster_command*n=raster_commands->next;free(raster_commands);raster_commands=n;}
 raster_tail=raster_cursor=NULL;raster_command_count=0;
}
static bool raster_record(unsigned kind,const int32_t args[8],const char *text) {
 if(!raster_recording || raster_replaying)return false;
 raster_command *c=raster_command_count<4096?malloc(sizeof(*c)):NULL;
 if(!c){
  /* A public drawing API is void: preserving every command requires a
   * synchronous compatibility fallback if allocation cannot retain it.
   * Materialize the complete prefix, then execute the current call directly.
   * This explicit fallback never silently truncates or omits UI. */
  raster_recording=false;
  if(!raster_materialize())return true;
  return false;
 }
 memset(c,0,sizeof(*c));c->kind=kind;memcpy(c->args,args,sizeof(c->args));
 c->text_present=text!=NULL;
 if(text){unsigned n=0;while(n<128 && text[n]){c->text[n]=text[n];++n;}c->text[n]=0;}
#ifdef PORTABLE_TOUCH_SCROLL
 c->clipped=raster_clip_active;c->clip=raster_clip;
#endif
 if(raster_tail)raster_tail->next=c;else raster_commands=c;
 raster_tail=c;++raster_command_count;return true;
}
static bool raster_begin(uint16_t color) {
 if(raster_replaying || (!raster_begin_allowed&&!raster_recording))return false;
 raster_begin_allowed=false;
 if(raster_sealed&&!raster_drain())return true;
 raster_free_commands();raster_recording=true;raster_band_top=0;
 raster_saved_rotation=paper_rotated;
#ifdef PORTABLE_PAPER_PREFERENCES
 raster_saved_flip=paper_flip_ui;
#endif
 return raster_record(RS_FILL,(int32_t[8]){0,0,width(),height(),color},NULL);
}
static void raster_execute(const raster_command*c) {
 const int32_t *a=c->args;const char*s=c->text_present?c->text:NULL;
#ifdef PORTABLE_TOUCH_SCROLL
 bool prior_clip=raster_clip_active;portable_scroll_viewport prior=raster_clip;
 raster_clip_active=c->clipped;raster_clip=c->clip;
#endif
 switch(c->kind){
  case RS_FILL:fill(a[0],a[1],a[2],a[3],(uint16_t)a[4]);break;
  case RS_ROUNDED:rounded(a[0],a[1],a[2],a[3],a[4],(uint8_t)a[5]);break;
  case RS_TEXT:text_color(a[0],a[1],s,a[2],(uint16_t)a[3]);break;
  case RS_ICON:(void)icon(a[0],a[1],s,(uint8_t)a[2],a[3]);break;
  case RS_PP_TEXT:pp_text(a[0],a[1],a[2],s,(unsigned)a[3],a[4],a[5]);break;
  case RS_PP_CIRCLE:pp_circle(a[0],a[1],a[2],a[3]);break;
  case RS_NP_ICON:(void)np_icon(a[0],a[1],a[2],s,(uint8_t)a[3]);break;
  case RS_NP_CIRCLE:np_circle(a[0],a[1],a[2],(uint32_t)a[3],(uint8_t)a[4]);break;
  case RS_NP_CAPTION:np_caption(a[0],s,a[1],(uint32_t)a[2]);break;
  case RS_NP_MASK:nova_mask_pending=true;np_mask();break;
 }
#ifdef PORTABLE_TOUCH_SCROLL
 raster_clip_active=prior_clip;raster_clip=prior;
#endif
}
static bool raster_materialize(void) {
 raster_replaying=true;
 if(!acquire_surface()){raster_replaying=false;return false;}
 raster_band_top=0;raster_band_bottom=height();
 for(raster_command*c=raster_commands;c&&!failed;c=c->next)raster_execute(c);
 raster_replaying=false;raster_free_commands();return !failed;
}
static bool raster_progress(void) {
 if(!raster_sealed || raster_replaying)return !failed;
 raster_replaying=true;
 bool prior_rotation=paper_rotated;paper_rotated=raster_saved_rotation;
#ifdef PORTABLE_PAPER_PREFERENCES
 bool prior_flip=paper_flip_ui;paper_flip_ui=raster_saved_flip;
#endif
 if(!raster_cursor){
  if(!acquire_surface()){raster_replaying=false;return false;}
  raster_cursor=raster_commands;raster_band_top=0;
 }
 unsigned start=millis_now(),pixels=raster_pixels;
 for(unsigned steps=0;raster_cursor&&!failed&&steps<256;steps++) {
  raster_band_bottom=raster_band_top+1;raster_execute(raster_cursor);
  if(++raster_band_top>=height()){raster_cursor=raster_cursor->next;raster_band_top=0;}
  if((unsigned)(millis_now()-start)>=2 || raster_pixels-pixels>=65536)break;
 }
 bool done=!raster_cursor;
 if(done){raster_sealed=false;raster_free_commands();present(raster_full);}
 paper_rotated=prior_rotation;
#ifdef PORTABLE_PAPER_PREFERENCES
 paper_flip_ui=prior_flip;
#endif
 raster_replaying=false;return !failed;
}
static bool raster_drain(void) {
 if(raster_replaying)return !failed;
 while(raster_sealed&&!failed){if(!raster_progress())return false;rt->yield_ms(1);}
 return !failed;
}
