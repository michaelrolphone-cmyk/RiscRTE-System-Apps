static void raster_free_commands(void) {
 while(raster_commands){raster_command*n=raster_commands->next;if(!raster_commands->borrowed)free(raster_commands->pixels);raster_layer_drop(raster_commands->layer);raster_commands=n;}
 raster_block_tail=NULL;raster_tail=raster_cursor=NULL;raster_command_count=0;
}
static void raster_discard(void) {
 free(raster_offscreen);raster_offscreen=NULL;
 raster_free_commands();
 while(raster_blocks){raster_command_block*n=raster_blocks->next;free(raster_blocks);raster_blocks=n;}
 raster_layer_paint=NULL;raster_layer_recording=false;raster_started=false;
 raster_recording=raster_sealed=raster_begin_allowed=false;
 raster_ready_to_submit=false;
}
static bool raster_record(unsigned kind,const int32_t args[8],const char *text) {
 if(failed)return true;
 if(!raster_recording || raster_replaying)return false;
 raster_command *c=NULL;
 if(raster_command_count<4096) {
  unsigned slot=raster_command_count%RASTER_COMMANDS_PER_BLOCK;
  if(!slot) {
   raster_command_block *block=raster_block_tail?raster_block_tail->next:raster_blocks;
   if(!block){block=malloc(sizeof(*block));if(block){block->next=NULL;if(raster_block_tail)raster_block_tail->next=block;else raster_blocks=block;}}
   if(block){raster_block_tail=block;}
   else goto allocation_failed;
  }
  c=&raster_block_tail->commands[slot];
 }
allocation_failed:
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
  else if(kind==RS_QA_RENDER)limit=8;
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
bool portable_raster_defer(void (*draw)(const void *,int,int),const void *payload,
 size_t bytes,int top,int bottom) {
 if(failed)return true;
 if(!raster_recording||raster_replaying)return false;
 if(!draw||!payload||bytes>sizeof(raster_tail->text))return false;
 if(!raster_record(RS_CUSTOM,(int32_t[8]){top,bottom},NULL))return false;
 if(!failed&&raster_tail&&raster_tail->kind==RS_CUSTOM) {
  raster_tail->custom=draw;memcpy(raster_tail->text,payload,bytes);
 }
 return true;
}
static bool raster_begin(uint16_t color) {
 if(failed)return true;
 if(raster_replaying || (!raster_begin_allowed&&!raster_recording))return false;
 raster_begin_allowed=false;
#ifdef PORTABLE_TOUCH_SCROLL
 /* An initially clipped clear preserves unknown provider pixels outside the
  * clip. Keep the original immediate lease for that partial-frame contract. */
 if(raster_clip_active){if(!raster_immediate())return true;return false;}
#endif
 if(raster_sealed&&!raster_drain())return true;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 /* The outgoing image is already visible when logical drawing starts. Replay
  * must advance that elapsed timeline, not restart it at the final copy. */
 if(handoff_pending){raster_handoff_origin=millis_now();if(failed)return true;}
#endif
 raster_free_commands();raster_recording=true;raster_ready_to_submit=false;raster_started=false;raster_band_top=0;
 raster_saved_rotation=paper_rotated;
#ifdef PORTABLE_PAPER_PREFERENCES
 raster_saved_flip=paper_flip_ui;
#endif
 return raster_record(RS_FILL,(int32_t[8]){0,0,width(),height(),color},NULL);
}
static bool raster_begin_pixels_owned(const void *pixels,unsigned stride,bool borrow) {
 if(failed)return true;
 if(raster_replaying||!raster_begin_allowed)return false;
 raster_begin_allowed=false;
 if(raster_sealed&&!raster_drain())return true;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 /* The outgoing image is already visible when logical drawing starts. Replay
  * must advance that elapsed timeline, not restart it at the final copy. */
 if(handoff_pending){raster_handoff_origin=millis_now();if(failed)return true;}
#endif
 unsigned row_bytes=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 if(!pixels||stride<row_bytes)return false;
 void *copy=borrow?(void*)pixels:malloc((size_t)row_bytes*info.height);if(!copy)return false;
 if(!borrow)for(unsigned y=0;y<info.height;y++)memcpy((uint8_t*)copy+(size_t)y*row_bytes,(const uint8_t*)pixels+(size_t)y*stride,row_bytes);
 raster_free_commands();raster_recording=true;raster_ready_to_submit=false;raster_started=false;raster_band_top=0;raster_saved_rotation=paper_rotated;
#ifdef PORTABLE_PAPER_PREFERENCES
 raster_saved_flip=paper_flip_ui;
#endif
 if(!raster_record(RS_BITMAP,(int32_t[8]){(int32_t)row_bytes},NULL)){if(!borrow&&!raster_terminal())free(copy);return false;}
 if(!raster_tail){if(!borrow&&!raster_terminal())free(copy);return true;}
 raster_tail->pixels=copy;raster_tail->borrowed=borrow;return true;
}
static inline bool raster_begin_pixels(const void *pixels,unsigned stride) {
 return raster_begin_pixels_owned(pixels,stride,false);
}
#include "raster_layers.inc"
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
  case RS_LAYER_BEGIN:case RS_LAYER_END:case RS_LAYER_BLIT:raster_layer_execute(c);break;
  case RS_CUSTOM:c->custom(c->text,raster_band_top,raster_band_bottom);break;
  case RS_BITMAP:
   for(int y=raster_band_top;y<raster_band_bottom&&(unsigned)y<info.height;y++) {
    memcpy((uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes,(const uint8_t*)c->pixels+(size_t)y*(unsigned)a[0],(unsigned)a[0]);
    raster_pixels+=info.width;if(!raster_checkpoint())break;
   }
   break;
  #if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL)||defined(RASTER_SBH_TEST)
  case RS_SBH_TEXT:(void)portable_springboard_header_text((unsigned)a[0],a[1],a[2],s,a[3],a[4]);break;
#endif
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
  case RS_QA_RENDER:
   if(!pqa_render_rows(&surface,c->pixels,s,a[0],(uint8_t)a[1],(unsigned)raster_band_top,(unsigned)(raster_band_bottom-raster_band_top)))failed=true;
   raster_pixels+=(unsigned)(raster_band_bottom-raster_band_top)*240u;(void)raster_checkpoint();break;
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
 raster_band_columns=false;raster_band_top=0;raster_band_bottom=height();
 for(raster_command*c=raster_commands;c&&!failed;c=c->next){raster_band_bottom=(c->kind==RS_BITMAP||c->kind==RS_LAYER_BLIT)?(int)info.height:height();raster_execute(c);}
 raster_replaying=false;if(!raster_terminal())raster_free_commands();return !failed;
}
/* Commands retain their complete ordering, but empty rows are not CPU work.
 * Bounds are conservative; original primitives still do exact clipping. */
#ifdef PORTABLE_QUICK_ACTIONS
static int raster_paper_y(int64_t y) {
 /* Only visible endpoints matter. Clamp before multiplication so the bounded
  * span needs no 64-bit division/runtime helper on admitted Xtensa modules. */
 if(y<-800)y=-800;
 if(y>1600)y=1600;
 return (int)y*height()/800;
}
#endif
static void raster_command_span(const raster_command *c) {
 const int32_t *a=c->args;int64_t top=0,bottom=height();
 /* A rotated one-row logical clear scatters 480 one-bit writes. Traverse
  * plain fills in native row order instead: one packed span per step, with
  * the same bounded input checkpoint and unchanged clipping/rotation. */
 raster_band_columns=c->kind==RS_FILL&&paper_rotated;
 switch(c->kind) {
  case RS_CUSTOM:top=a[0];bottom=a[1];break;
#if defined(PORTABLE_SPRINGBOARD_TOUCH_SCROLL)||defined(RASTER_SBH_TEST)
  case RS_SBH_TEXT:{
   const sbh_font*font=a[0]?&SBH_700:&SBH_900;int lo=0,hi=0;
   for(unsigned n=0;c->text[n];n++){const sbh_glyph*g=raster_sbh_lookup(font,(unsigned char)c->text[n]);if(!g)continue;if(g->top<lo)lo=g->top;if(g->top+g->height>hi)hi=g->top+g->height;}
   top=(int64_t)a[2]+lo;bottom=(int64_t)a[2]+hi;break;
  }
#endif
  case RS_LAYER_BEGIN:case RS_LAYER_END:bottom=1;break;
  case RS_LAYER_BLIT:case RS_BITMAP:bottom=info.height;break;
  case RS_FILL:top=a[raster_band_columns?0:1];bottom=top+a[raster_band_columns?2:3];break;
  case RS_ROUNDED:top=a[1];bottom=top+a[3];break;
  case RS_TEXT:top=a[1];bottom=top+7;break;
  case RS_ICON:top=a[1];bottom=top+a[2];break;
  case RS_PP_CIRCLE:case RS_NP_CIRCLE:top=(int64_t)a[1]-a[2];bottom=(int64_t)a[1]+a[2]+1;break;
  case RS_NP_ICON:top=(int64_t)a[1]-a[2]/2;bottom=(int64_t)a[1]+a[2]+1;break;
  case RS_NP_PIXEL:top=a[1];bottom=top+1;break;
  case RS_PP_TEXT:{
   int lo=0,hi=8,zoom=(a[3]&PAPER_TEXT_CLOCK)?3:1;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   if(zoom==1&&(a[3]&PAPER_TEXT_LARGE))zoom=2;
#endif
   for(unsigned n=0;n<95;n++){const rpp_glyph*g=&rpp_text[(a[4]?0:95)+n];if(g->top<lo)lo=g->top;if(g->top+g->height>hi)hi=g->top+g->height;}
   top=(int64_t)a[1]+lo*zoom;bottom=(int64_t)a[1]+hi*zoom;break;
  }
  case RS_NU_PIXEL:case RS_SV_PIXEL:case RS_WV_PIXEL:top=(int64_t)a[1]+(height()-240)/2;bottom=top+1;break;
  case RS_NU_RECT:case RS_NU_ROUND:case RS_SV_RECT:case RS_WV_RECT:
   top=(int64_t)a[1]+(height()-240)/2;bottom=top+a[3];break;
#ifdef PORTABLE_QUICK_ACTIONS
  case RS_QP_ICON:top=raster_paper_y((int64_t)a[2]+c->paper_offset);bottom=top+34*width()/480+1;break;
  case RS_QR_ROUND:top=raster_paper_y((int64_t)a[1]+c->paper_offset);bottom=raster_paper_y((int64_t)a[1]+a[3]+c->paper_offset)+2;break;
  case RS_QR_ICON:top=raster_paper_y((int64_t)a[2]+c->paper_offset);bottom=raster_paper_y((int64_t)a[2]+40+c->paper_offset)+2;break;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  case RS_QR_TEXT:{
   const quick_ref_font *fonts[]={&QUICK_REF_TITLE,&QUICK_REF_TIME,&QUICK_REF_NUMBER,&QUICK_REF_LABEL,&QUICK_REF_TILE,&QUICK_REF_SMALL,&QUICK_REF_BATTERY};
   const quick_ref_font *font=fonts[a[0]];int lo=0,hi=0;
   for(unsigned n=0;n<95;n++){const quick_ref_glyph*g=&font->glyphs[n];if(g->top<lo)lo=g->top;if(g->top+g->height>hi)hi=g->top+g->height;}
   for(unsigned n=0;n<sizeof(quick_ref_words)/sizeof(*quick_ref_words);n++){
    const quick_ref_word*w=&quick_ref_words[n];if(w->font!=font)continue;
    if(w->top<lo)lo=w->top;
    if(w->top+(int)w->height>hi)hi=w->top+(int)w->height;
   }
   top=raster_paper_y((int64_t)a[2]+lo+c->paper_offset)-1;bottom=raster_paper_y((int64_t)a[2]+hi+c->paper_offset)+2;break;
  }
#endif
#endif
 }
 int maximum=(c->kind==RS_BITMAP||c->kind==RS_LAYER_BLIT)?(int)info.height:raster_band_columns?width():height();
 if(top<0)top=0;
 if(top>maximum)top=maximum;
 if(bottom<top)bottom=top;
 if(bottom>maximum)bottom=maximum;
 raster_band_top=(int)top;raster_band_limit=(int)bottom;
}
static bool raster_progress(void) {
 if(!raster_sealed || raster_replaying)return !failed;
 if(!paper_token_progress())return false;
 raster_replaying=true;
 bool prior_rotation=paper_rotated;paper_rotated=raster_saved_rotation;
#ifdef PORTABLE_PAPER_PREFERENCES
 bool prior_flip=paper_flip_ui;paper_flip_ui=raster_saved_flip;
#endif
 const unsigned row_bytes=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 const size_t bytes=(size_t)row_bytes*info.height;
 if(raster_ready_to_submit)goto ready;
 if(!raster_started){
  if(!raster_offscreen)raster_offscreen=malloc(bytes);
  if(!raster_offscreen){
   /* Preserve the complete frame through the original immediate renderer when
    * an owned replay buffer cannot be allocated. No command/service is lost. */
   if(!raster_materialize())goto restore;
   raster_replaying=true;
   goto submit;
  }
  raster_started=true;raster_cursor=raster_commands;if(raster_cursor)raster_command_span(raster_cursor);
 }
 /* Only app-owned pixels are exposed during a band. No provider frame lease
  * crosses the return to application/service state. A real frame identifier is
  * never fabricated, including when input capture installs a terminal fence. */
 risc_display_surface_v1 prior_surface=surface;
 surface=(risc_display_surface_v1){.pixels=raster_layer_paint?raster_layer_paint->pixels:raster_offscreen,.width=info.width,.height=info.height,
   .stride_bytes=row_bytes,.size_bytes=(uint32_t)bytes,.pixel_format=surface_format};
 unsigned start=millis_now(),pixels=raster_pixels;
 for(unsigned steps=0;raster_cursor&&!failed&&steps<256;steps++) {
  if(raster_band_top<raster_band_limit){
   /* Packed copies are cheap: amortize dispatch and clock calls across
    * eight rows. Primitives still checkpoint input inside longer operations. */
   int band=(raster_cursor->kind==RS_LAYER_BLIT||raster_cursor->kind==RS_BITMAP)?8:1;
   raster_band_bottom=raster_band_top+band;if(raster_band_bottom>raster_band_limit)raster_band_bottom=raster_band_limit;
   raster_execute(raster_cursor);raster_band_top=raster_band_bottom;
  }
  if(failed)break; /* Checkpoint retention forbids even a budget-clock read. */
  if(raster_band_top>=raster_band_limit){raster_cursor=raster_cursor->next;if(raster_cursor)raster_command_span(raster_cursor);}
  if((unsigned)(millis_now()-start)>=2 || raster_pixels-pixels>=65536)break;
 }
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(!native_custody_retained)
#endif
 surface=prior_surface;
 if(failed || raster_cursor)goto restore;
 raster_ready_to_submit=true;
ready:
 /* Software pixels can finish while the previously submitted frame is busy.
  * Do not acquire or drain a provider lease until that token has completed. */
 if(paper_token)goto restore;
 if(!acquire_surface())goto restore;
 for(unsigned y=0;y<info.height;y++) {
  uint8_t *to=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes;
  const uint8_t *from=(const uint8_t*)raster_offscreen+(size_t)y*row_bytes;
  uint8_t padding=to[row_bytes-1];
  memcpy(to,from,row_bytes);
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1&&(info.width&7u)&&raster_commands&&raster_commands->kind!=RS_BITMAP) {
   uint8_t mask=(uint8_t)(0xffu<<(8u-(info.width&7u)));
   to[row_bytes-1]=(to[row_bytes-1]&mask)|(padding&~mask);
  }
  if(!(y&7u)&&!raster_checkpoint())goto restore;
 }
submit:
 raster_sealed=false;raster_ready_to_submit=false;raster_free_commands();
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_RESIDENT_SHELL_HOST)
 bool prior_clean=quick_clean_present;quick_clean_present=raster_saved_clean;
#endif
 present(raster_full);
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_RESIDENT_SHELL_HOST)
 quick_clean_present=prior_clean;
#endif
restore:
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
