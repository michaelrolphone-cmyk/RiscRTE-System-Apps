#ifndef PORTABLE_RASTER_WORK_BUDGET_MS
#define PORTABLE_RASTER_WORK_BUDGET_MS 2
#endif
static void raster_free_commands(void) {
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
 sv_text_setup_valid=false;
#endif
 while(raster_commands){raster_command*n=raster_commands->next;if(!raster_commands->borrowed)free(raster_commands->pixels);raster_layer_drop(raster_commands->layer);raster_commands=n;}
 raster_block_tail=NULL;raster_tail=raster_cursor=NULL;raster_command_count=0;
}
static void raster_discard(void) {
#if defined(PORTABLE_RASTER_UNDERLAY_EXCHANGE) && defined(PORTABLE_ALARM_CLIENT)
 raster_copy_forget();
#endif
 free(raster_offscreen);raster_offscreen=NULL;
 raster_free_commands();
 while(raster_blocks){raster_command_block*n=raster_blocks->next;free(raster_blocks);raster_blocks=n;}
 raster_layer_paint=NULL;raster_layer_recording=false;raster_started=false;
 raster_recording=raster_sealed=raster_begin_allowed=false;
 raster_ready_to_submit=false;
}
#if defined(PORTABLE_NOVA_MASK_BOUNDS) && defined(PORTABLE_NOVA_UI)
/* Provenance lives in this immutable command, never a reused pointer/address.
 * Unknown drawing and incomplete base coverage keep the original mask path. */
static void raster_nova_mask_bounds(raster_command *mask) {
 if(surface_format!=RISC_DISPLAY_FORMAT_RGB565 || paper_rotated || raster_saved_rotation ||
    !info.width || !info.height || info.width>INT32_MAX || info.height>INT32_MAX ||
    width()!=(int)info.width || height()!=(int)info.height || mask->paper_offset)return;
#ifdef PORTABLE_PAPER_PREFERENCES
 if(paper_flip_ui || raster_saved_flip)return;
#endif
#ifdef PORTABLE_SAFE_VIEWPORT
 if(content_view.x || content_view.y || content_view.width!=info.width || content_view.height!=info.height)return;
#endif
#ifdef PORTABLE_TOUCH_SCROLL
 if(mask->clipped)return;
#endif
 const raster_command *c=raster_commands;
 if(!c || c->kind!=RS_FILL || c->args[0] || c->args[1] ||
    c->args[2]!=(int)info.width || c->args[3]!=(int)info.height || c->args[4])return;
 int32_t left=(int32_t)info.width,top=(int32_t)info.height,right=0,bottom=0;
 for(;c;c=c->next){
  if(c->paper_offset)return;
#ifdef PORTABLE_TOUCH_SCROLL
  if(c->clipped)return;
#endif
  if(c==raster_commands)continue;
  const int32_t *a=c->args;int64_t l,t,r,b;
  if(c->kind==RS_NP_CIRCLE){
   if(a[2]<1 || a[2]>256)return;
   l=(int64_t)a[0]-a[2];t=(int64_t)a[1]-a[2];r=(int64_t)a[0]+a[2]+1;b=(int64_t)a[1]+a[2]+1;
  }else if(c->kind==RS_NP_ICON){
   if(a[2]<1 || a[2]>256)return;
   l=(int64_t)a[0]-a[2]/2;t=(int64_t)a[1]-a[2]/2;r=(int64_t)a[0]+(a[2]+1)/2;b=(int64_t)a[1]+(a[2]+1)/2;
  }else if(c->kind==RS_NP_PIXEL){l=a[0];t=a[1];r=l+1;b=t+1;}
  else return;
  if(l<0)l=0;
  if(t<0)t=0;
  if(r>info.width)r=info.width;
  if(b>info.height)b=info.height;
  if(l>=r || t>=b)continue;
  if(l<left)left=(int32_t)l;
  if(t<top)top=(int32_t)t;
  if(r>right)right=(int32_t)r;
  if(b>bottom)bottom=(int32_t)b;
 }
 mask->args[0]=left;mask->args[1]=top;mask->args[2]=right;mask->args[3]=bottom;
 mask->args[5]=(int32_t)info.width;mask->args[6]=(int32_t)info.height;
 mask->args[4]=1; /* Empty union is a valid all-black proof. */
}
#endif
static bool raster_record(unsigned kind,const int32_t args[8],const char *text) {
#ifdef PORTABLE_FILE_SETUP
 if(!file_setup_io_allowed())return true;
#endif
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
  #ifdef PORTABLE_RF_NATIVE_CANVAS
  else if(kind==RS_NU_NATIVE_TEXT)limit=96;
#endif
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
#if defined(PORTABLE_NOVA_MASK_BOUNDS) && defined(PORTABLE_NOVA_UI)
 if(kind==RS_NP_MASK)raster_nova_mask_bounds(c);
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
#ifdef PORTABLE_FILE_SETUP
 if(!file_setup_io_allowed())return true;
#endif
 if(failed)return true;
 if(raster_replaying || (!raster_begin_allowed&&!raster_recording))return false;
 raster_begin_allowed=false;
#if defined(PORTABLE_RASTER_UNDERLAY_EXCHANGE) && defined(PORTABLE_ALARM_CLIENT)
 raster_copy_forget();
#endif
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
#ifdef PORTABLE_FILE_SETUP
 if(!file_setup_io_allowed())return true;
#endif
 if(failed)return true;
 if(raster_replaying||!raster_begin_allowed)return false;
 raster_begin_allowed=false;
#if defined(PORTABLE_RASTER_UNDERLAY_EXCHANGE) && defined(PORTABLE_ALARM_CLIENT)
 raster_copy_forget();
#endif
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
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
 /* Only these commands contain exactly one private Settings text call.
  * Arbitrary custom callbacks never borrow another command's preparation. */
 sv_text_setup_active=(c->kind==RS_SV_TEXT||c->kind==RS_SV_BUTTON)?c:NULL;
#endif
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
  case RS_SV_TEXT:sv_text_aligned((unsigned)a[0],a[1],a[2],s,(uint32_t)a[3],a[4],(unsigned)a[5]);break;
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
   raster_pixels+=(unsigned)(raster_band_bottom-raster_band_top)*
#ifdef PORTABLE_QUICK_SLICED_NATIVE
      info.width;
#else
      240u;
#endif
   (void)raster_checkpoint();break;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  case RS_QR_TEXT:{const quick_ref_font *fonts[]={&QUICK_REF_TITLE,&QUICK_REF_TIME,&QUICK_REF_NUMBER,&QUICK_REF_LABEL,&QUICK_REF_TILE,&QUICK_REF_SMALL,&QUICK_REF_BATTERY};qr_text(fonts[a[0]],a[1],a[2],s,a[3],a[4],a[5],a[6]);break;}
  case RS_QR_ROUND:qr_round(a[0],a[1],a[2],a[3],a[4],a[5]);break;
  case RS_QR_ICON:qr_icon((unsigned)a[0],a[1],a[2],a[3]);break;
#elif !defined(PORTABLE_WATCH_COLOR_ONLY)
  case RS_QP_ICON:qp_icon((unsigned)a[0],a[1],a[2],a[3]);break;
#endif
#endif
  case RS_NP_MASK:nova_mask_pending=true;
#ifdef PORTABLE_NOVA_MASK_BOUNDS
   /* Materialization/provider drawing deliberately retains the old path. */
   if(raster_started && raster_replaying && surface.pixels==raster_offscreen && !raster_layer_paint)
    np_mask_region(a);
   else
#endif
    {np_mask();}
   break;
  case RS_NP_PIXEL:np_pixel(a[0],a[1],(uint32_t)a[2],(unsigned)a[3]);break;
#ifdef PORTABLE_NOVA_UI
#ifdef PORTABLE_RF_NATIVE_CANVAS
  case RS_NU_NATIVE_TEXT:portable_nova_native_text((unsigned)a[0],a[1],a[2],a[3],s,(uint32_t)a[4]);break;
  case RS_NU_NATIVE_ROUND:portable_nova_native_round(a[0],a[1],a[2],a[3],a[4],(uint32_t)a[5]);break;
#endif
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
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
 sv_text_setup_active=NULL;
#endif
}
static bool raster_materialize(void) {
#if defined(PORTABLE_RASTER_UNDERLAY_EXCHANGE) && defined(PORTABLE_ALARM_CLIENT)
 raster_copy_forget();
#endif
 raster_replaying=true;
 if(!acquire_surface()){raster_replaying=false;return false;}
 raster_band_columns=false;raster_band_top=0;raster_band_bottom=height();
 for(raster_command*c=raster_commands;c&&!failed;c=c->next){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
 sv_text_setup_valid=false;
#endif
 raster_band_bottom=(c->kind==RS_BITMAP||c->kind==RS_LAYER_BLIT)?(int)info.height:height();raster_execute(c);}
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
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
 sv_text_setup_valid=false;
#endif
 const int32_t *a=c->args;int64_t top=0,bottom=height();
 /* A rotated one-row logical clear scatters 480 one-bit writes. Traverse
  * plain fills in native row order instead: one packed span per step, with
  * the same bounded input checkpoint and unchanged clipping/rotation. */
 raster_band_columns=c->kind==RS_FILL&&paper_rotated;
 switch(c->kind) {
  /* These compound/full-surface commands deliberately retain the safe full
   * span. Nova/Wi-Fi composite text/buttons include font fitting and (for
   * native Watch Nova) a nontrivial layout transform; unknown future kinds
   * also keep this fallback rather than silently clipping new drawing. */
  case RS_NP_MASK:case RS_QA_RENDER:
  case RS_NU_TEXT:case RS_NU_BUTTON:case RS_WV_TEXT:case RS_WV_BUTTON:break;
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
  case RS_NP_ICON:
   /* np_icon scales either glyph dimension to at most size, centered with
    * integer floor on the leading edge. Keep the union over both parities. */
   top=(int64_t)a[1]-a[2]/2;bottom=(int64_t)a[1]+((int64_t)a[2]+1)/2;break;
  case RS_NP_CAPTION:{
   np_caption_layout layout;
   if(!np_caption_measure(a[0],c->text_present?c->text:NULL,a[1],&layout)){top=bottom=0;break;}
   /* Backing expands the inclusive glyph bounds by two plus six feather
    * pixels; the replay upper endpoint is exclusive. */
   top=(int64_t)layout.top-8;bottom=(int64_t)layout.bottom+9;break;
  }
  case RS_NP_PIXEL:top=a[1];bottom=top+1;break;
  case RS_PP_TEXT:{
   int lo=0,hi=8,zoom=(a[3]&PAPER_TEXT_CLOCK)?3:1;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   if(zoom==1&&(a[3]&PAPER_TEXT_LARGE))zoom=2;
#endif
   for(unsigned n=0;n<95;n++){const rpp_glyph*g=&rpp_text[(a[4]?0:95)+n];if(g->top<lo)lo=g->top;if(g->top+g->height>hi)hi=g->top+g->height;}
   top=(int64_t)a[1]+lo*zoom;bottom=(int64_t)a[1]+hi*zoom;break;
  }
#ifdef PORTABLE_RF_NATIVE_CANVAS
  case RS_NU_NATIVE_ROUND:top=(int64_t)a[raster_band_columns?0:1]+nu_native_band_offset(raster_band_columns);bottom=top+a[raster_band_columns?2:3];break;
  case RS_NU_NATIVE_TEXT:{
   int left=0,above=0,right=0,below=0;
   if(!nu_native_text_bounds((unsigned)a[0],a[1],a[2],a[3],c->text,&left,&above,&right,&below)){top=bottom=0;break;}
   top=(raster_band_columns?left:above)+nu_native_band_offset(raster_band_columns);bottom=(raster_band_columns?right:below)+nu_native_band_offset(raster_band_columns);break;
  }
#endif
#ifdef PORTABLE_SETTINGS_APP
  case RS_SV_PIXEL:top=sv_native_y(a[1]);bottom=sv_native_y(a[1]+1);break;
  case RS_SV_RECT:top=sv_native_y(a[1]);bottom=sv_native_y(a[1]+a[3]);break;
  case RS_SV_FADE:top=sv_native_y(a[0]);bottom=sv_native_y(a[1]);break;
  case RS_SV_TEXT:{
   top=height();bottom=0;
   if(a[0]>=0&&a[0]<=5)for(unsigned i=0;i<96&&c->text[i];++i){
    unsigned ch=(unsigned char)c->text[i];if(ch<32||ch>126)ch='?';
    const rps_glyph*g=sv_glyph((unsigned)a[0],ch);if(!g->height)continue;
    int y=sv_native_y(a[2])+g->top;if(y<top)top=y;if(y+g->height>bottom)bottom=y+g->height;
#ifdef PORTABLE_SETTINGS_NATIVE_GEOMETRY
    if(sv_round())for(unsigned bank=0;bank<=sizeof(rps_native_sides)/sizeof(*rps_native_sides);++bank){
      const rps_glyph*small=sv_glyph_at((unsigned)a[0],ch,bank?rps_native_sides[bank-1]:240);
      int sy=sv_native_y(a[2])+small->top;if(sy<top)top=sy;if(sy+small->height>bottom)bottom=sy+small->height;
    }
#endif
   }
   break;
  }
  case RS_SV_BUTTON:{
   int x=a[0],y=a[1],w=a[2],h=a[3];
#ifdef PORTABLE_NOVA_UI
   sv_geometry(&x,&y,&w,&h);
#endif
   sv_native_button_geometry(&x,&y,&w,&h);
   top=sv_native_y(y);bottom=sv_native_y(y+h);
   for(unsigned i=0;i<96&&c->text[i];++i){
    unsigned ch=(unsigned char)c->text[i];if(ch<32||ch>126)ch='?';
    const rps_glyph*g=sv_glyph(2,ch);if(!g->height)continue;
    int yy=sv_native_y(y+(h-12)/2-2)+g->top;
    if(yy<top)top=yy;
    if(yy+g->height>bottom)bottom=yy+g->height;
   }
   break;
  }
#endif
  case RS_NU_PIXEL:case RS_NU_RECT:case RS_NU_ROUND:
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
  {
   int h=c->kind==RS_NU_PIXEL?1:a[3];
   watch_layout_rect logical={0,a[1],1,h>0?(uint32_t)h:0},physical;
   if(!nu_layout_ready()||!watch_layout_native_rect(&nu_layout,&logical,&physical)){top=bottom=0;break;}
   top=physical.y;bottom=top+physical.height;
#ifdef PORTABLE_WATCH_SAFE_INSETS
   top+=content_view.y;bottom+=content_view.y;
#endif
   break;
  }
#else
   top=(int64_t)a[1]+(height()-240)/2;bottom=top+(c->kind==RS_NU_PIXEL?1:a[3]);break;
#endif
  case RS_WV_PIXEL:top=(int64_t)a[1]+(height()-240)/2;bottom=top+1;break;
  case RS_WV_RECT:top=(int64_t)a[1]+(height()-240)/2;bottom=top+a[3];break;
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
#if defined(PORTABLE_RASTER_OVERWRITE_CLEAR) && defined(PORTABLE_SAFE_VIEWPORT)
/* Immutable first command must initialize every active physical RGB565 byte
 * before any blend/read command. Partial viewport/clip/transform paths keep
 * their original clear. Padding is never part of this packed software image. */
static bool raster_initializes_all_pixels(const raster_command *c) {
 if(!c || surface_format!=RISC_DISPLAY_FORMAT_RGB565 || paper_rotated || c->paper_offset)return false;
#ifdef PORTABLE_TOUCH_SCROLL
 if(c->clipped)return false;
#endif
#ifdef PORTABLE_SAFE_VIEWPORT
 if(content_view.x || content_view.y || content_view.width!=info.width || content_view.height!=info.height)return false;
#endif
 if(c->kind==RS_FILL)return c->args[0]==0 && c->args[1]==0 &&
    c->args[2]==(int)info.width && c->args[3]==(int)info.height;
 return false;
}
#endif
static bool raster_progress(void) {
#ifdef PORTABLE_FILE_SETUP
 if(!file_setup_checkpoint())return false;
#endif
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
#ifdef PORTABLE_SAFE_VIEWPORT
  /* acquire_surface clears the complete RGB565 provider image before drawing
   * its safe content. Replay must initialize the same outside-content pixels
   * before its complete-image copy, not copy uninitialized allocator bytes. */
  if(surface_format==RISC_DISPLAY_FORMAT_RGB565 && !paper_rotated
#ifdef PORTABLE_RASTER_OVERWRITE_CLEAR
     && !raster_initializes_all_pixels(raster_commands)
#endif
   )memset(raster_offscreen,0,bytes);
#endif
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
#ifdef PORTABLE_RASTER_COOPERATIVE_INPUT
   unsigned kind=raster_cursor->kind;
   raster_input_slice=kind==RS_FILL||kind==RS_NP_ICON||kind==RS_NP_CIRCLE||
       kind==RS_NP_CAPTION||kind==RS_NP_MASK||kind==RS_NP_PIXEL
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
       ||kind==RS_SV_PIXEL||kind==RS_SV_RECT||kind==RS_SV_TEXT||kind==RS_SV_BUTTON||kind==RS_SV_FADE
#endif
       ;
#endif
   raster_execute(raster_cursor);
#ifdef PORTABLE_RASTER_COOPERATIVE_INPUT
   raster_input_slice=false;
#endif
   raster_band_top=raster_band_bottom;
  }
  if(failed)break; /* Checkpoint retention forbids even a budget-clock read. */
  if(raster_band_top>=raster_band_limit){raster_cursor=raster_cursor->next;if(raster_cursor)raster_command_span(raster_cursor);}
  if((unsigned)(millis_now()-start)>=PORTABLE_RASTER_WORK_BUDGET_MS || raster_pixels-pixels>=65536)break;
 }
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(!native_custody_retained)
#endif
 surface=prior_surface;
#ifdef PORTABLE_RASTER_COOPERATIVE_INPUT
 if(!failed)input_service();
#endif
 if(failed || raster_cursor)goto restore;
 raster_ready_to_submit=true;
ready:
 /* Software pixels can finish while the previously submitted frame is busy.
  * Do not acquire or drain a provider lease until that token has completed. */
 if(paper_token)goto restore;
 if(!acquire_surface_mode(true))goto restore;
#if defined(PORTABLE_RASTER_UNDERLAY_EXCHANGE) && defined(PORTABLE_ALARM_CLIENT)
 raster_copy_forget();
#endif
 for(unsigned y=0;y<info.height;y++) {
  uint8_t *to=(uint8_t*)surface.pixels+(size_t)y*surface.stride_bytes;
  const uint8_t *from=(const uint8_t*)raster_offscreen+(size_t)y*row_bytes;
  bool preserve_edge=surface_format==RISC_DISPLAY_FORMAT_MONO1&&(info.width&7u)&&raster_commands&&raster_commands->kind!=RS_BITMAP;
  uint8_t padding=preserve_edge?to[row_bytes-1]:0;
  memcpy(to,from,row_bytes);
  if(preserve_edge) {
   uint8_t mask=(uint8_t)(0xffu<<(8u-(info.width&7u)));
   to[row_bytes-1]=(to[row_bytes-1]&mask)|(padding&~mask);
  }
  if(!(y&7u)&&!raster_checkpoint())goto restore;
 }
#if defined(PORTABLE_RASTER_UNDERLAY_EXCHANGE) && defined(PORTABLE_ALARM_CLIENT)
 /* Every physical row copied successfully with all checkpoint fences live. */
 if(surface_format==RISC_DISPLAY_FORMAT_RGB565 && !failed && !raster_terminal()) {
  raster_copied_source=raster_offscreen;raster_copied_frame=surface.frame;
 }
#endif
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
