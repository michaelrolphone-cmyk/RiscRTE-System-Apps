static void raster_free_commands(void) {
 while(raster_commands){raster_command*n=raster_commands->next;free(raster_commands->pixels);free(raster_commands);raster_commands=n;}
 raster_tail=raster_cursor=NULL;raster_command_count=0;
}
static void raster_discard(void) {
 raster_free_commands();raster_recording=raster_sealed=raster_begin_allowed=false;
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
 if(text){
  unsigned limit=128;
  if(kind==RS_TEXT)limit=args[2]<=0?0u:args[2]<128?(unsigned)args[2]:128u;
  else if(kind==RS_NP_CAPTION)limit=95;
  else if(kind==RS_QR_TEXT)limit=64;
  else if(kind==RS_PP_TEXT||kind==RS_NU_TEXT||kind==RS_NU_BUTTON||kind==RS_SV_TEXT||kind==RS_SV_BUTTON||kind==RS_WV_TEXT||kind==RS_WV_BUTTON)limit=96;
  unsigned n=0;while(n<limit && text[n]){c->text[n]=text[n];++n;}
#ifdef PORTABLE_PAPER_PREFERENCES
  if(kind==RS_PP_TEXT&&n==96&&(unsigned char)c->text[95]==0xc2)c->text[n++]=text[96];
#endif
  c->text[n]=0;
 }
#ifdef PORTABLE_TOUCH_SCROLL
 c->clipped=raster_clip_active;c->clip=raster_clip;
#endif
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_PAPER_TRANSITIONS)
 c->paper_offset=qp_offset;
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
static inline bool raster_begin_pixels(const void *pixels,unsigned stride) {
 if(raster_replaying||!raster_begin_allowed)return false;
 raster_begin_allowed=false;
 if(raster_sealed&&!raster_drain())return true;
 unsigned row_bytes=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 if(!pixels||stride<row_bytes)return false;
 void *copy=malloc((size_t)row_bytes*info.height);if(!copy)return false;
 for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)copy+(size_t)y*row_bytes,(const uint8_t*)pixels+(size_t)y*stride,row_bytes);
 raster_free_commands();raster_recording=true;raster_band_top=0;raster_saved_rotation=paper_rotated;
#ifdef PORTABLE_PAPER_PREFERENCES
 raster_saved_flip=paper_flip_ui;
#endif
 if(!raster_record(RS_BITMAP,(int32_t[8]){(int32_t)row_bytes},NULL)){free(copy);return false;}
 if(!raster_tail){free(copy);return true;}
 raster_tail->pixels=copy;return true;
}
static void raster_execute(const raster_command*c) {
 const int32_t *a=c->args;const char*s=c->text_present?c->text:NULL;
#ifdef PORTABLE_TOUCH_SCROLL
 bool prior_clip=raster_clip_active;portable_scroll_viewport prior=raster_clip;
 raster_clip_active=c->clipped;raster_clip=c->clip;
#endif
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_PAPER_TRANSITIONS)
 int prior_offset=qp_offset;qp_offset=c->paper_offset;
#endif
 switch(c->kind){
  case RS_BITMAP:
   for(int y=raster_band_top;y<raster_band_bottom&&(unsigned)y<info.height;y++) {
    memcpy((uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,(const uint8_t*)c->pixels+(size_t)y*(unsigned)a[0],(unsigned)a[0]);
    raster_pixels+=info.width;if(!raster_checkpoint())break;
   }
   break;
  case RS_FILL:fill(a[0],a[1],a[2],a[3],(uint16_t)a[4]);break;
  case RS_ROUNDED:rounded(a[0],a[1],a[2],a[3],a[4],(uint8_t)a[5]);break;
  case RS_TEXT:text_color(a[0],a[1],s,a[2],(uint16_t)a[3]);break;
  case RS_ICON:(void)icon(a[0],a[1],s,(uint8_t)a[2],a[3]);break;
  case RS_PP_TEXT:pp_text(a[0],a[1],a[2],s,(unsigned)a[3],a[4],a[5]);break;
  case RS_PP_CIRCLE:pp_circle(a[0],a[1],a[2],a[3]);break;
  case RS_NP_ICON:(void)np_icon(a[0],a[1],a[2],s,(uint8_t)a[3]);break;
  case RS_NP_CIRCLE:np_circle(a[0],a[1],a[2],(uint32_t)a[3],(uint8_t)a[4]);break;
  case RS_NP_CAPTION:np_caption(a[0],s,a[1],(uint32_t)a[2]);break;
#ifdef PORTABLE_SETTINGS_APP
  case RS_SV_PIXEL:sv_pixel(a[0],a[1],(uint32_t)a[2],(unsigned)a[3]);break;
  case RS_SV_RECT:sv_rect(a[0],a[1],a[2],a[3],(uint32_t)a[4]);break;
  case RS_SV_TEXT:sv_text((unsigned)a[0],a[1],a[2],s,(uint32_t)a[3],a[4]);break;
  case RS_SV_BUTTON:sv_button(a[0],a[1],a[2],a[3],s,a[4]);break;
  case RS_SV_FADE:sv_fade(a[0],a[1]);break;
#endif
#if (defined(PORTABLE_WIFI_SETTINGS_APP)||defined(PORTABLE_UPDATE_APP))&&!defined(PORTABLE_NOVA_UI)
  case RS_WV_PIXEL:wv_pixel(a[0],a[1],(uint32_t)a[2],(unsigned)a[3]);break;
  case RS_WV_RECT:wv_rect(a[0],a[1],a[2],a[3],(uint32_t)a[4]);break;
  case RS_WV_TEXT:wv_text((unsigned)a[0],a[1],a[2],a[3],s,(uint32_t)a[4]);break;
  case RS_WV_BUTTON:wv_button(a[0],a[1],a[2],a[3],s,a[4]);break;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  case RS_QR_TEXT:{const quick_ref_font *fonts[]={&QUICK_REF_TITLE,&QUICK_REF_TIME,&QUICK_REF_NUMBER,&QUICK_REF_LABEL,&QUICK_REF_TILE,&QUICK_REF_SMALL,&QUICK_REF_BATTERY};qr_text(fonts[a[0]],a[1],a[2],s,a[3],a[4],a[5],a[6]);break;}
  case RS_QR_ROUND:qr_round(a[0],a[1],a[2],a[3],a[4],a[5]);break;
  case RS_QR_ICON:qr_icon((unsigned)a[0],a[1],a[2],a[3]);break;
#else
  case RS_QP_ICON:qp_icon((unsigned)a[0],a[1],a[2],a[3]);break;
#endif
#endif
  case RS_NP_MASK:nova_mask_pending=true;np_mask();break;
  case RS_NP_PIXEL:np_pixel(a[0],a[1],(uint32_t)a[2],(unsigned)a[3]);break;
#ifdef PORTABLE_NOVA_UI
  case RS_NU_PIXEL:nu_pixel(a[0],a[1],(uint32_t)a[2],(unsigned)a[3]);break;
  case RS_NU_RECT:nu_rect(a[0],a[1],a[2],a[3],(uint32_t)a[4]);break;
  case RS_NU_TEXT:portable_nova_text((unsigned)a[0],a[1],a[2],a[3],s,(uint32_t)a[4]);break;
  case RS_NU_ROUND:portable_nova_round(a[0],a[1],a[2],a[3],a[4],(uint32_t)a[5]);break;
  case RS_NU_BUTTON:portable_nova_button(a[0],a[1],a[2],a[3],s,a[4]);break;
#endif
 }
#ifdef PORTABLE_TOUCH_SCROLL
 raster_clip_active=prior_clip;raster_clip=prior;
#endif
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_PAPER_TRANSITIONS)
 qp_offset=prior_offset;
#endif
}
static bool raster_materialize(void) {
 raster_replaying=true;
 if(!acquire_surface()){raster_replaying=false;return false;}
 raster_band_top=0;raster_band_bottom=height();
 for(raster_command*c=raster_commands;c&&!failed;c=c->next){raster_band_bottom=c->kind==RS_BITMAP?(int)info.height:height();raster_execute(c);}
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
  if(++raster_band_top>=(raster_cursor->kind==RS_BITMAP?(int)info.height:height())){raster_cursor=raster_cursor->next;raster_band_top=0;}
  if((unsigned)(millis_now()-start)>=2 || raster_pixels-pixels>=65536)break;
 }
 bool done=!raster_cursor;
 if(done){
  raster_sealed=false;raster_free_commands();
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_RESIDENT_SHELL_HOST)
  bool prior_clean=quick_clean_present;quick_clean_present=raster_saved_clean;
#endif
  present(raster_full);
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_RESIDENT_SHELL_HOST)
  quick_clean_present=prior_clean;
#endif
 }
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

/* Explicit compatibility barrier for APIs that expose writable framebuffer
 * memory. Callers keep their complete existing drawing and ownership contract. */
static inline bool raster_immediate(void) {
 raster_begin_allowed=false;
 if(raster_sealed&&!raster_drain())return false;
 if(raster_recording){raster_recording=false;return raster_materialize();}
 return !failed;
}
