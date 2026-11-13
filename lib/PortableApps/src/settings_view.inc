#include "PortableRgb565Blend.h"
/* Compact app-local presentation, following the supplied 240x240 Settings
 * reference. All functional values come from the shared Settings model. */
#include "../settings_fonts/text.inc"
#include "settings_native_geometry.inc"
#include "../../../Apps/PaperFrame.h"
#define SV_CYAN 0x19e3ffu
#define SV_DIM 0x0e4f5cu
#define SV_LINE 0x12262bu
#define SV_CAP 0x6b8288u
#define SV_TEXT 0xcfe9eeu
enum {
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 SV_STORAGE=30,SV_DISCARD=31,SV_ROOT=0,
#else
 SV_ROOT,
#endif
 SV_FIELDS, SV_VALUE, SV_FOLD, SV_ABOUT, SV_SLEEP, SV_ALERT, SV_TIME_FORMAT, SV_DESK_FACE, SV_FLIP, SV_LANGUAGE
#ifdef PORTABLE_SETTINGS_TIME_ZONE
  ,SV_TIMEZONE_REGIONS,SV_TIMEZONE_CITIES
#endif
#ifdef PORTABLE_SETTINGS_DESK_DIRECTION
  ,SV_DESK_DIRECTION
#endif
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
  ,SV_TIMER
#endif
};
/* Shared drawn/hit geometry in the established 240px logical content space.
 * Capability-native positioning and safe-inset transforms happen once outside. */
enum {SV_CANCEL_X=24,SV_SAVE_X=130,SV_ACTION_Y=200,SV_ACTION_W=86,SV_ACTION_H=26,
      SV_TIMER_DEC_X=20,SV_TIMER_INC_X=156,SV_TIMER_Y=88,SV_TIMER_W=64,SV_TIMER_H=44};
static unsigned sv_page, sv_selected, sv_fold_choice;
#ifdef PORTABLE_TOUCH_SCROLL
static portable_touch_scroll stz_scroll;
static unsigned stz_submitted_page,stz_completed_page;
static int stz_submitted_q8,stz_completed_q8,stz_hit_q8,stz_touch_row;
static bool stz_hit_ready;
#ifdef PORTABLE_SETTINGS_LIST_SCROLL
static portable_touch_scroll sp_scroll[2];
static unsigned sp_submitted_page,sp_completed_page;
static int sp_submitted_q8,sp_completed_q8,sp_hit_q8,sp_touch_row,sp_hit_x,sp_hit_y;
static bool sp_hit_ready,sp_root_touch_selection;
static void sp_scroll_visible(void);
static void sp_scroll_configure(unsigned page);
#endif
static void settings_view_interrupt(void) {
  portable_scroll_cancel(&stz_scroll);stz_hit_ready=false;stz_touch_row=-1;
#ifdef PORTABLE_SETTINGS_LIST_SCROLL
  portable_scroll_cancel(&sp_scroll[0]);portable_scroll_cancel(&sp_scroll[1]);
  sp_hit_ready=false;sp_touch_row=-1;
#endif
}
#endif
static int sv_scroll[2];
static bool sv_active, sv_need_neutral, sv_gesture, sv_eligible;
static bool sp_dirty;
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
/* Hit identity belongs to completed pixels, not a newer queued render. */
typedef struct {uint64_t receipt;unsigned page,epoch;int scroll[2];unsigned timezone_first,timezone_region;} sv_frame_map;
static sv_frame_map sv_submitted_map,sv_visible_map,sv_gesture_map,sv_action_map;
static uint64_t sv_submitted_receipt;
static unsigned sv_map_epoch;
static bool sv_image_pending,sv_visible_valid;
static void sv_frame_completed(void) {
 if(sv_image_pending && portable_paper_frame_idle()) {
  if(sv_submitted_receipt && portable_frame_completed_id()==sv_submitted_receipt) {
   sv_visible_map=sv_submitted_map;sv_visible_valid=true;
  } else sp_dirty=true;
  sv_image_pending=false;
 }
}
static bool sv_frame_current(void) {
 return sv_visible_valid && sv_visible_map.page==sv_page && sv_visible_map.epoch==sv_map_epoch;
}
static void sv_frame_submitted(void) {
 sv_submitted_map=(sv_frame_map){.page=sv_page,.epoch=sv_map_epoch,
  .scroll={sv_scroll[0],sv_scroll[1]}
#ifdef PORTABLE_SETTINGS_TIME_ZONE
  ,.timezone_first=stz_first,.timezone_region=stz_region
#endif
 };
 sv_submitted_receipt=portable_frame_requested_id();sv_submitted_map.receipt=sv_submitted_receipt;sv_image_pending=sv_submitted_receipt!=0;
 sv_frame_completed();
}
static int sv_action_scroll(unsigned page){return sv_action_map.scroll[page];}
#ifdef PORTABLE_SETTINGS_TIME_ZONE
static unsigned sv_action_timezone_first(void){return sv_action_map.timezone_first;}
#endif
#else
static int sv_action_scroll(unsigned page){return sv_scroll[page];}
#ifdef PORTABLE_SETTINGS_TIME_ZONE
static unsigned sv_action_timezone_first(void){return stz_first;}
#endif
#endif

static inline bool sv_save_frame_ready(void) {
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
 sv_frame_completed();
 if(sv_image_pending || !sv_frame_current() || sv_submitted_receipt!=portable_frame_completed_id())return false;
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_modal || pqa_visible(&quick.ui))return false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
 if(alarm_modal)return false;
#endif
#endif
 return !sp_dirty && portable_paper_frame_idle();
}
static unsigned sv_axis; /* 1 vertical, 2 horizontal, 3 numeric slider */
static int sv_start_x, sv_start_y, sv_last_x, sv_last_y, sv_start_scroll;
static bool sv_dragged;
static void settings_view_redraw(void);
static bool sp_clean,sp_dirty;
static void sp_redraw(void);
static bool sp_poll(t5_app_input_t *out);
static int sv_abs(int n) { return n < 0 ? -n : n; }
#if defined(PORTABLE_SETTINGS_ALPHA_GEOMETRY) && defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_RASTER_COOPERATIVE_INPUT) && !defined(PORTABLE_FILE_SETUP)
/* A transparent sample still consumes the existing pixel budget. Count a
 * no-write run without coordinate/lease/blend work; every original 512-pixel
 * checkpoint remains at the same counter value. Callers may aggregate a mapped
 * footprint only while replay explicitly forbids callbacks inside that row. */
static bool sv_transparent_samples(unsigned count) {
  if(failed)return false;
  while(count){
    unsigned step=512u-(raster_pixels&511u);
    if(step>count)step=count;
    raster_pixels+=step;count-=step;
    if(!(raster_pixels&511u)&&!raster_checkpoint())return false;
  }
  return !failed;
}
#endif
static void sv_native_pixel(int x, int y, uint32_t rgb, unsigned alpha) {
  if(failed || (!(++raster_pixels&511u)&&!raster_checkpoint()))return;
#if defined(PORTABLE_SETTINGS_ALPHA_GEOMETRY)
  /* Zero coverage cannot read/write pixels, even when no lease is bound.
   * The counter/checkpoint above remains authoritative before this rejection. */
  if(!alpha)return;
#endif
  int side=sv_native_side(),left=(width()-side)/2,top=(height()-side)/2;
  if (!raster_surface_writable() || x < left || x >= left+side || y < top || y >= top+side || !alpha) return;
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_replaying&&(y<raster_band_top||y>=raster_band_bottom))return;
#endif
  if(surface_format==RISC_DISPLAY_FORMAT_MONO1){np_pixel(x,y,rgb,alpha);return;}
  native_point(&x,&y);
  uint8_t *p = (uint8_t *)surface.pixels + (size_t)y * surface.stride_bytes + (size_t)x * 2;
  /* Opaque coverage neither reads the destination nor blends it. All
   * geometry, lease, clip and cancellation checks above remain mandatory. */
  uint16_t c;
  if(alpha>=255)c=portable_rgb565_pack(rgb);
  else {
    uint16_t before=(uint16_t)(p[0]|p[1]<<8);
    c=portable_rgb565_over(before,rgb,alpha);
  }
  p[0] = (uint8_t)c; p[1] = (uint8_t)(c >> 8);
}
static void sv_pixel(int x,int y,uint32_t rgb,unsigned alpha) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_SV_PIXEL,(int32_t[8]){x,y,(int32_t)rgb,(int32_t)alpha},NULL))return;
#endif
  if(x<0||x>=240||y<0||y>=240)return;
  int x0=sv_native_x(x),x1=sv_native_x(x+1),y0=sv_native_y(y),y1=sv_native_y(y+1);
  for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)sv_native_pixel(xx,yy,rgb,alpha);
}
/* Opaque logical rectangles cover contiguous native cells. Map their clipped
 * endpoints once, not four scale divisions and a transform per logical pixel.
 * This path has no provider calls except the existing bounded checkpoints. */
static bool sv_native_rect(int x,int y,int w,int h,uint32_t rgb) {
  if(surface_format!=RISC_DISPLAY_FORMAT_RGB565)return false;
  if(failed || !raster_surface_writable())return true;
  int64_t right=(int64_t)x+w,bottom=(int64_t)y+h;
  if(w<=0||h<=0||x>=240||y>=240||right<=0||bottom<=0)return true;
  if(x<0)x=0;
  if(y<0)y=0;
  if(right>240)right=240;
  if(bottom>240)bottom=240;
  int left=sv_native_x(x),top=sv_native_y(y);
  int end=sv_native_x((int)right),last=sv_native_y((int)bottom);
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_replaying){if(top<raster_band_top)top=raster_band_top;if(last>raster_band_bottom)last=raster_band_bottom;}
#endif
  if(left>=end||top>=last)return true;
  int ax=left,ay=top,bx=left+1,by=top,cx=left,cy=top+1;
  native_point(&ax,&ay);native_point(&bx,&by);native_point(&cx,&cy);
  ptrdiff_t origin=(ptrdiff_t)ay*surface.stride_bytes+(ptrdiff_t)ax*2;
  ptrdiff_t column=(ptrdiff_t)(by-ay)*surface.stride_bytes+(ptrdiff_t)(bx-ax)*2;
  ptrdiff_t row=(ptrdiff_t)(cy-ay)*surface.stride_bytes+(ptrdiff_t)(cx-ax)*2;
  uint16_t color=portable_rgb565_pack(rgb);uint8_t lo=(uint8_t)color,hi=(uint8_t)(color>>8);
  for(int j=top;j<last;++j)for(int i=left;i<end;){
    unsigned n=511u-(raster_pixels&511u);
    if(!n){++raster_pixels;if(!raster_checkpoint()||!raster_surface_writable())return true;n=1;}
    else {if(n>(unsigned)(end-i))n=(unsigned)(end-i);raster_pixels+=n;}
    uint8_t *to=(uint8_t*)surface.pixels+origin+(ptrdiff_t)(j-top)*row+(ptrdiff_t)(i-left)*column;
    if(lo==hi&&(column==2||column==-2)){
      if(column<0)to-=(ptrdiff_t)(n-1u)*2;
      memset(to,lo,(size_t)n*2);
    }else for(unsigned k=0;k<n;++k){uint8_t*p=to+(ptrdiff_t)k*column;p[0]=lo;p[1]=hi;}
    i+=(int)n;
  }
  return true;
}
static void sv_rect(int x, int y, int w, int h, uint32_t color) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_SV_RECT,(int32_t[8]){x,y,w,h,(int32_t)color},NULL))return;
#endif
  if(sv_round()&&color){portable_round_span band;
    if(portable_round_band(width(),height(),sv_native_y(y),sv_native_y(y+h),8,&band)){
      int left=sv_layout_x(band.left),right=sv_layout_x(band.right-1)+1;
      while(sv_native_x(left)<band.left)++left;
      while(sv_native_x(right)>band.right)--right;
      int end=x+w;if(x<left){end+=left-x;x=left;}if(end>right)end=right;w=end-x;
    }
  }
  if(sv_native_rect(x,y,w,h,color))return;
  int row_first=y,row_end=y+h;
#ifdef PORTABLE_RASTER_SNAPSHOT
  sv_layout_rows(0,&row_first,&row_end);
#endif
  for (int j = row_first; j < row_end; ++j) for (int i = x; i < x+w; ++i) sv_pixel(i,j,color,255);
}
static int sv_measure(unsigned face, const char *s, int spacing) {
  int length=0;if(!s||face>5)return 0;
  for(unsigned i=0;i<96&&s[i];++i){unsigned ch=(unsigned char)s[i];if(ch<32||ch>126)ch='?';
    length+=sv_glyph(face,ch)->advance_q4+(i?sv_native_spacing(spacing):0);}
  return ((length+15)/16*240+sv_native_side()-1)/sv_native_side();
}
#if defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
/* One immutable replay command owns this setup. Reinitialized at command
 * boundaries, never keyed only by an allocation address across frames. */
static bool sv_text_setup_valid;
static const void *sv_text_setup_active,*sv_text_setup_owner;
static int sv_text_setup_width,sv_text_setup_height;
static unsigned sv_text_setup_format;
static bool sv_text_setup_round;
static int sv_text_setup_pen,sv_text_setup_top,sv_text_setup_side;
#endif
static void sv_text_aligned(unsigned face,int x,int y,const char*s,uint32_t color,int spacing,unsigned align) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_SV_TEXT,(int32_t[8]){(int32_t)face,x,y,(int32_t)color,spacing,(int32_t)align},s))return;
#endif
  if(!s||face>5)return;
  int pen,top,font_side;
#if defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
  if(raster_replaying && sv_text_setup_active && sv_text_setup_owner==sv_text_setup_active && sv_text_setup_valid &&
     sv_text_setup_width==width() && sv_text_setup_height==height() &&
     sv_text_setup_format==surface_format && sv_text_setup_round==sv_round()){pen=sv_text_setup_pen;top=sv_text_setup_top;font_side=sv_text_setup_side;}
  else
#endif
  {
  int original_x=x;
  if(align==1)x-=sv_measure(face,s,spacing)/2;
  if(align==2)x-=sv_measure(face,s,spacing);
  pen=sv_native_x(x)*16;top=sv_native_y(y);font_side=sv_native_side();
  if(sv_round()){
    portable_round_span band={0,width()};int advance=0;
    for(;;){int above=999,below=-999;advance=0;
      for(unsigned k=0;k<96&&s[k];++k){unsigned ch=(unsigned char)s[k];if(ch<32||ch>126)ch='?';
        const rps_glyph*g=sv_glyph_at(face,ch,font_side);
        if(g->height){if(g->top<above)above=g->top;if(g->top+g->height>below)below=g->top+g->height;}
        advance+=g->advance_q4+(k?sv_native_spacing(spacing):0);
      }
      bool fits=below>above&&portable_round_band(width(),height(),top+above,top+below,8,&band)&&advance<=(band.right-band.left)*16;
      if(fits||font_side<=240)break;
      int next=240;
#ifdef PORTABLE_SETTINGS_NATIVE_GEOMETRY
      for(unsigned b=0;b<sizeof(rps_native_sides)/sizeof(*rps_native_sides);++b)if(rps_native_sides[b]<font_side)next=rps_native_sides[b];
#endif
      font_side=next;
    }
    if(align==1)pen=sv_native_x(original_x)*16-advance/2;
    if(align==2)pen=sv_native_x(original_x)*16-advance;
    if(pen+advance>band.right*16)pen=band.right*16-advance;
    if(pen<band.left*16)pen=band.left*16;
  }
#if defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_SETTINGS_PREPARED_TEXT)
    if(raster_replaying && sv_text_setup_active){
      sv_text_setup_owner=sv_text_setup_active;sv_text_setup_width=width();sv_text_setup_height=height();
      sv_text_setup_format=surface_format;sv_text_setup_round=sv_round();sv_text_setup_pen=pen;sv_text_setup_top=top;sv_text_setup_side=font_side;sv_text_setup_valid=true;}
#endif
  }
  for(unsigned k=0;k<96&&s[k];++k){unsigned ch=(unsigned char)s[k];if(ch<32||ch>126)ch='?';
    const rps_glyph*g=sv_glyph_at(face,ch,font_side);int first=0,end=g->height;
#ifdef PORTABLE_RASTER_SNAPSHOT
    raster_rows((int64_t)top+g->top,&first,&end);
#endif
    for(int j=first;j<end;++j)for(unsigned i=0;i<g->width;++i){unsigned n=(unsigned)j*g->width+i;
      unsigned coverage=(g->bits[n/4]>>(6-2*(n%4)))&3;
      sv_native_pixel(pen/16+g->left+(int)i,top+g->top+j,color,coverage*85);}
    pen+=g->advance_q4+sv_native_spacing(spacing);
  }
}
static void sv_text(unsigned face,int x,int y,const char*s,uint32_t color,int spacing){sv_text_aligned(face,x,y,s,color,spacing,0);}
static void sv_center(unsigned face,int y,const char *s,uint32_t color,int spacing) {
  if(sv_round())sv_text_aligned(face,120,y,s,color,spacing,1);
  else sv_text(face,(240-sv_measure(face,s,spacing))/2,y,s,color,spacing);
}
static void sv_right(unsigned face,int x,int y,const char *s,uint32_t color,int spacing) {
  if(sv_round())sv_text_aligned(face,x,y,s,color,spacing,2);
  else sv_text(face,x-sv_measure(face,s,spacing),y,s,color,spacing);
}
#ifdef PORTABLE_NOVA_UI
#define SV_ROW_HEIGHT 44
#define SV_MESSAGE_Y 172
#define SV_FIELD_BOTTOM 166
static void sv_geometry(int *x,int *y,int *w,int *h) {
  if(*h==26 && (*y==192 || *y==200)){*y=188;*h=44;}
  else if(*h==36 && *y==66){*x=*x<100?20:176;*y=74;*w=*h=44;}
  else if(*h==30 && (*y==66 || *y==104 || *y==142)) {
    bool first=*y==66,last=*y==142;*x=first || last?20:124;*y=last?120:64;*w=last?200:96;*h=44;
  } else if(*h==32){*y=*y==76?64:*y==124?118:*y==100?82:*y==142?132:*y;*h=44;}
}
#else
#define SV_ROW_HEIGHT (sv_round_compact_rows()?36:40)
#define SV_MESSAGE_Y 177
#define SV_FIELD_BOTTOM (sv_round()?60+3*SV_ROW_HEIGHT:176)
#endif
static void sv_button(int x,int y,int w,int h,const char *caption,bool selected) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_SV_BUTTON,(int32_t[8]){x,y,w,h,selected},caption))return;
#endif
#ifdef PORTABLE_NOVA_UI
  sv_geometry(&x,&y,&w,&h);
#endif
  sv_native_button_geometry(&x,&y,&w,&h);
  /* Rounded outline follows the mockup, with a usable complete hit rectangle. */
  int row_first=0,row_end=h;
#ifdef PORTABLE_RASTER_SNAPSHOT
  sv_layout_rows(y,&row_first,&row_end);
#endif
  for(int j=row_first;j<row_end;++j)for(int i=0;i<w;++i) {
    int dx=i<6?6-i:i>=w-6?i-(w-7):0;
    int dy=j<6?6-j:j>=h-6?j-(h-7):0;
    if(dx*dx+dy*dy>36)continue;
    bool edge=i==0 || i==w-1 || j==0 || j==h-1 || (dx && dy && dx*dx+dy*dy>22);
    sv_pixel(x+i,y+j,edge?(selected?SV_CYAN:SV_DIM):0,255);
  }
  sv_text(2,x+(w-sv_measure(2,caption,18))/2,y+(h-12)/2-2,caption,SV_CYAN,18);
}
static void sv_fade(int top,int bottom) {
#ifdef PORTABLE_RASTER_SNAPSHOT
  if(raster_record(RS_SV_FADE,(int32_t[8]){top,bottom},NULL))return;
#endif
  int row_first=top,row_end=bottom;
#ifdef PORTABLE_RASTER_SNAPSHOT
  sv_layout_rows(0,&row_first,&row_end);
#endif
  for(int y=row_first;y<row_end;++y) {
    unsigned opacity=255;
    int a=y-top,b=bottom-1-y;
    int distance=a<b?a:b;
    if(sv_native_side()>240){if(distance<6)opacity=(unsigned)distance*255/6;}
    else if(distance<14)opacity=(unsigned)distance*102/14;
    else if(distance<46)opacity=102+(unsigned)(distance-14)*153/32;
#if defined(PORTABLE_SETTINGS_ALPHA_GEOMETRY) && defined(PORTABLE_RASTER_SNAPSHOT) && defined(PORTABLE_RASTER_COOPERATIVE_INPUT) && !defined(PORTABLE_FILE_SETUP)
    if(opacity==255 && raster_replaying && raster_input_slice){
      if(y>=0 && y<240){
        /* Sum the adjacent logical cells' native widths. Their mapped edges
         * telescope exactly; preserve even the siblings clipped by this band. */
        unsigned columns=(unsigned)(sv_native_x(240)-sv_native_x(0));
        unsigned rows=(unsigned)(sv_native_y(y+1)-sv_native_y(y));
        if(!sv_transparent_samples(columns*rows))return;
      }
      continue;
    }
#endif
    for(int x=0;x<240;++x)sv_pixel(x,y,0,255-opacity);
  }
}
static void sv_begin(void) { clear(); fill(0,0,width(),height(),0); }
static void sv_heading(const char *title) {
  sv_text(2,20,24,title,SV_CAP,28);sv_rect(20,46,200,1,SV_DIM);
}
static void sv_back(void) { sv_button(82,192,76,26,"< BACK",false); }
static int sv_row_y(unsigned index) {
  if(sv_native_side()>240)return index<3?58+(int)index*SV_ROW_HEIGHT:210+(int)(index-3)*SV_ROW_HEIGHT;
  return index<3?124+(int)index*SV_ROW_HEIGHT:278+(int)(index-3)*SV_ROW_HEIGHT;
}
static int sv_max_scroll(void) {
  if(sv_page==SV_ROOT&&sv_native_side()>240)return sv_row_y(SETTINGS_ROW_COUNT-1)+SV_ROW_HEIGHT-174;
  return sv_page==SV_ROOT?220+(int)(SETTINGS_ROW_COUNT-5u)*SV_ROW_HEIGHT:(sv_round()?3*SV_ROW_HEIGHT:(SV_ROW_HEIGHT==44?168:144));
}
static void sv_clamp_scroll(void) {
  unsigned p=sv_page==SV_ROOT?0:1;
  if(sv_scroll[p]<0)sv_scroll[p]=0;
  if(sv_scroll[p]>sv_max_scroll())sv_scroll[p]=sv_max_scroll();
}
static void sv_root(void) {
  int offset=sv_scroll[0];
  if(sv_native_side()>240){
    if(sv_round())sv_center(0,18-offset,"RiscRTE Settings",SV_CYAN,0);
    else sv_text(0,20,6-offset,"RiscRTE Settings",SV_CYAN,0);
    sv_rect(20,32-offset,200,1,SV_DIM);
    sv_text(3,20,(sv_round()?44:38)-offset,"TIME",0x0e8ea3,38);
    sv_text(3,20,190-offset,"SYSTEM",0x0e8ea3,38);
  } else {
    sv_text(0,20,50-offset,"RiscRTE",SV_CYAN,18);
    sv_text(2,20,72-offset,"Settings",SV_CAP,22);
    sv_rect(20,94-offset,200,1,SV_DIM);
    sv_text(3,20,104-offset,"TIME",0x0e8ea3,38);
    sv_text(3,20,258-offset,"SYSTEM",0x0e8ea3,38);
  }
  for(unsigned i=0;i<SETTINGS_ROW_COUNT;++i) {
    t5_app_setting_t row;bool available=settings_get(0,i,&row);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(failed)return;
#endif
    if(!available)continue;
    int y=sv_row_y(i)-offset;
    sv_rect(20,y,2,SV_ROW_HEIGHT,sv_selected==i+1?SV_CYAN:SV_LINE);
    sv_rect(22,y+SV_ROW_HEIGHT-1,198,1,SV_LINE);
    if(sv_native_side()>240){
      /* Native body/value fonts need separate baselines, not overlapping
       * left/right labels when a localized setting or value is long. */
      sv_text(1,32,y+1,row.label,0xffffff,4);
      sv_text(2,32,y+20,row.value,SV_CYAN,0);
      if(row.type==T5_APP_SETTING_ACTION)sv_text(1,211,y+12,">",SV_DIM,0);
    } else {
      sv_text(1,32,y+10,row.label,0xffffff,4);
      sv_right(2,row.type==T5_APP_SETTING_ACTION?204:216,y+12,row.value,SV_CYAN,0);
      if(row.type==T5_APP_SETTING_ACTION)sv_text(1,211,y+9,">",SV_DIM,0);
    }
  }
#ifdef PORTABLE_NOVA_UI
  sv_fade(0,184);sv_rect(0,184,240,56,0);
#else
  sv_fade(0,192);sv_rect(0,190,240,50,0);
#endif
  if(sv_round()){
    /* A stable round header keeps scrolling rows out of the narrow top cap. */
    sv_rect(0,0,240,40,0);sv_center(0,18,"RiscRTE Settings",SV_CYAN,0);
    sv_rect(20,32,200,1,SV_DIM);
  }
  if(settings_message[0])sv_center(3,173,settings_message,SV_CYAN,0);
  sv_back();
}
#ifndef PORTABLE_NOVA_UI
/* Fit the status glyph bounds between the three-row list and the actual
 * expanded action targets. Use available clearance rather than a fixed origin. */
static int sv_fields_message_y(const char *message) {
  if(sv_native_side()==240)return 185;
  int top=999,bottom=-999;
  for(unsigned i=0;message&&message[i];++i){unsigned ch=(unsigned char)message[i];if(ch<32||ch>126)ch='?';
    const rps_glyph*g=sv_glyph(3,ch);if(!g->height)continue;
    if(g->top<top)top=g->top;
    if(g->top+g->height>bottom)bottom=g->top+g->height;}
  if(bottom<top)return 177;
  int x=130,y=200,w=86,h=26;sv_native_button_geometry(&x,&y,&w,&h);
  int first=sv_native_y(sv_round()?SV_FIELD_BOTTOM:180),last=sv_native_y(y),best=177,clearance=-999;
  for(int candidate=170;candidate<200;++candidate){
    int above=sv_native_y(candidate)+top-first,below=last-sv_native_y(candidate)-bottom;
    int gap=above<below?above:below;if(gap>clearance){clearance=gap;best=candidate;}}
  return best;
}
#endif
static void sv_fields(void) {
  sv_heading("TIME AND DATE");
  for(unsigned i=0;i<6;++i) {
    int y=60+(int)i*SV_ROW_HEIGHT-sv_scroll[1];
    if(y+SV_ROW_HEIGHT<=50 || y>=184)continue;
    char value[24];editor_value_text(i,value,sizeof(value));
    sv_rect(20,y,2,SV_ROW_HEIGHT,editor_field==i?SV_CYAN:SV_LINE);
    sv_rect(22,y+SV_ROW_HEIGHT-1,198,1,SV_LINE);
    sv_text(1,32,y+10,editor_names[i],0xffffff,4);
    sv_right(1,202,y+10,value,SV_CYAN,0);sv_text(1,211,y+9,">",SV_DIM,0);
  }
  /* Keep the fixed heading/buttons intact while rows move behind them. */
#ifdef PORTABLE_NOVA_UI
  sv_fade(50,166);sv_rect(0,166,240,74,0);
#else
  int footer=sv_round()?SV_FIELD_BOTTOM:sv_native_side()>240?180:188;
  sv_fade(50,sv_native_side()>240?footer+6:footer);sv_rect(0,footer,240,240-footer,0);
#endif
  sv_rect(0,0,240,50,0);sv_heading("TIME AND DATE");
  sv_button(24,200,86,26,"CANCEL",editor_field==7);
  sv_button(130,200,86,26,"SAVE",editor_field==6);
#ifdef PORTABLE_NOVA_UI
  sv_center(3,171,editor_message,SV_CAP,0);
#else
  sv_center(3,sv_fields_message_y(editor_message),editor_message,SV_CAP,0);
#endif
}
static void sv_value(void) {
  sv_heading(editor_names[editor_field]);
  char value[24]; unsigned v=editor_value(editor_field),shown=v;
  if(editor_field==3 && !settings_hour_24())shown=v%12?v%12:12;
  snprintf(value,sizeof(value),"%u",shown);
  sv_center(4,59,value,SV_CYAN,0);
  sv_button(24,66,36,36,"",false);sv_button(180,66,36,36,"",false);
  for(unsigned i=0;i<2;++i) {
    const char *symbol=i?"+":"-";const rps_glyph *g=&rps_text[5*95+(unsigned)symbol[0]-32];
#ifdef PORTABLE_NOVA_UI
    sv_text(5,(i?176:20)+(44-(int)g->advance_q4/16)/2,74+(44-g->height)/2-g->top,symbol,SV_CYAN,0);
#else
    sv_text(5,(i?180:24)+(36-(int)g->advance_q4/16)/2,66+(36-g->height)/2-g->top,symbol,SV_CYAN,0);
#endif
  }
  unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);
  unsigned filled=hi>lo?(v-lo)*10/(hi-lo):0;
  for(unsigned i=0;i<10;++i)sv_rect(72+(int)i*10,124,7,8,i<filled?SV_CYAN:0x032027);
  if(editor_field==3 && !settings_hour_24()) {
#ifdef PORTABLE_NOVA_UI
    sv_button(76,140,88,44,v<12?"AM":"PM",true);
#else
    sv_center(1,145,v<12?"AM":"PM",SV_TEXT,12);
#endif
  }
  else {snprintf(value,sizeof(value),"%u - %u",lo,hi);sv_center(2,145,value,SV_CAP,12);}
#ifndef PORTABLE_NOVA_UI
  sv_center(3,163,"Draft only - use SAVE",SV_CAP,2);
#endif
  sv_back();
}
static void sv_fold(void) {
  sv_heading("REPEATED LOCAL TIME");
  sv_center(2,56,"This hour occurs twice",SV_TEXT,0);
  sv_center(2,72,"Choose which time to save",SV_CAP,0);
  sv_button(28,100,184,32,
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  "SAVE EARLIER UTC"
#else
  "SAVE FIRST  MDT"
#endif
,sv_fold_choice==0);
  sv_button(28,142,184,32,
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  "SAVE LATER UTC"
#else
  "SAVE SECOND MST"
#endif
,sv_fold_choice==1);
  sv_back();
}
static void sv_about(void) {
  sv_heading("ABOUT");
  sv_text(1,20,68,"Settings",SV_CAP,0);sv_right(1,220,68,PORTABLE_SETTINGS_VERSION,SV_TEXT,0);
  sv_rect(20,93,200,1,SV_LINE);
  char geometry[24];snprintf(geometry,sizeof(geometry),"%d x %d",(int)(sv_native_side()>240?info.width:(unsigned)width()),(int)(sv_native_side()>240?info.height:(unsigned)height()));
  sv_text(1,20,103,"Display",SV_CAP,0);sv_right(1,220,103,geometry,SV_TEXT,0);
  sv_rect(20,128,200,1,SV_LINE);
  if(sv_native_side()>240){
    snprintf(geometry,sizeof(geometry),"%d x %d",(int)(sv_round()?info.width-info.safe_insets.left-info.safe_insets.right:(unsigned)width()),(int)(sv_round()?info.height-info.safe_insets.top-info.safe_insets.bottom:(unsigned)height()));
    sv_text(1,20,136,"Safe content",SV_CAP,0);sv_right(1,220,136,geometry,SV_TEXT,0);
    sv_center(2,168,"Reported device geometry",SV_CAP,0);
  } else sv_center(2,148,"Only reported device data",SV_CAP,0);
  sv_back();
}
static void sv_time_format(void) {
  sv_heading("TIME FORMAT");
  sv_button(28,76,184,32,"12-HOUR  AM / PM",time_format_choice==PORTABLE_TIME_FORMAT_12);
  sv_button(28,124,184,32,"24-HOUR",time_format_choice==PORTABLE_TIME_FORMAT_24);
  sv_center(3,SV_MESSAGE_Y,time_format_message[0]?time_format_message:"Draft only - use SAVE",
            time_format_message[0]?SV_CYAN:SV_CAP,0);
  sv_button(SV_CANCEL_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"CANCEL",false);
  sv_button(SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"SAVE",false);
}
#ifdef PORTABLE_SLEEP_SETTINGS
static void sv_sleep(void) {
  sv_heading("CLOCK SLEEP MODE");
#if defined(PORTABLE_SETTINGS_CONFIGURED_SLEEP_DEFAULT) || defined(PORTABLE_SETTINGS_IDLE_TIMER) || defined(PORTABLE_SETTINGS_BOTH_TIMERS)
  const char *options[]={"LIGHT SLEEP","DEEP SLEEP","HYBRID SLEEP"};
  const char *description="Clock preference - use SAVE";
#else
  const char *options[]={"LIGHT SLEEP","DEEP SLEEP","LIGHT 5 MIN - DEEP"};
  const char *description="Apps: Light 5 min then Deep";
#endif
  for(unsigned row=0;row<settings_sleep_count();++row){unsigned mode=settings_sleep_at(row);
    sv_button(28,66+(int)row*38,184,30,options[mode],sleep_choice==mode);}
#ifdef PORTABLE_NOVA_UI
  sv_center(3,172,sleep_message[0]?sleep_message:description,SV_CAP,0);
#else
  if(sv_native_side()>240)sv_center(3,177,sleep_message[0]?sleep_message:description,sleep_message[0]?SV_CYAN:SV_CAP,0);
  else {sv_center(3,177,description,SV_CAP,0);sv_center(3,190,sleep_message,SV_CYAN,0);}
#endif
  sv_button(SV_CANCEL_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"CANCEL",false);
  sv_button(SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"SAVE",false);
}
#endif
#ifdef PORTABLE_ALARM_SETTINGS
static void sv_alert(void) {
  sv_heading("ALARM ALERTS");
  if(settings_visual_alerts()) {
    sv_center(1,86,"VISUAL ONLY",SV_CYAN,0);
    sv_center(3,124,"No sound or vibration",SV_CAP,0);
    sv_center(3,160,"Saved preference kept",SV_CAP,0);
    sv_button(24,200,192,26,"BACK",false);return;
  }
  sv_button(28,66,184,30,"VIBRATE",alert_choice==PORTABLE_ALERT_VIBRATE);
  sv_button(28,104,184,30,"SOUND",alert_choice==PORTABLE_ALERT_SOUND);
  sv_button(28,142,184,30,"BOTH",alert_choice==PORTABLE_ALERT_BOTH);
  sv_center(3,SV_MESSAGE_Y,alert_message[0]?alert_message:"Alarms and Countdown",
            alert_message[0]?SV_CYAN:SV_CAP,0);
  sv_button(SV_CANCEL_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"CANCEL",false);
  sv_button(SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"SAVE",false);
}
#endif
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
static void sv_timer(void) {
  sv_heading(timer_deep?"DEEP SLEEP TIMER":"SLEEP TIMER");
  char value[24];snprintf(value,sizeof(value),"%lu SEC",(unsigned long)(timer_choice/1000u));
  sv_center(0,66,value,SV_CYAN,0);
  sv_button(SV_TIMER_DEC_X,SV_TIMER_Y,SV_TIMER_W,SV_TIMER_H,"-",false);sv_button(SV_TIMER_INC_X,SV_TIMER_Y,SV_TIMER_W,SV_TIMER_H,"+",false);
  sv_center(3,150,timer_deep?"Time asleep before Deep":"Idle time before sleep",SV_CAP,0);
  sv_center(3,172,timer_message,SV_CYAN,0);
  sv_button(SV_CANCEL_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"CANCEL",false);sv_button(SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H,"SAVE",false);
}
#endif
static void settings_view_redraw(void) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(failed)return;
#endif
  if(handoff_requested)return;
  if(failed || !sv_active)return;
  sp_dirty=true;
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
  sv_frame_completed();
  if(sv_image_pending || !portable_paper_frame_idle())return;
#endif
  if(!portable_paper_frame_ready())return;
  sp_dirty=false;
  if(pp_enabled()) {
#ifdef PORTABLE_SETTINGS_LIST_SCROLL
    sp_scroll_visible();
#endif
  }
#ifdef PORTABLE_SETTINGS_TIME_ZONE
  if(sv_page==SV_TIMEZONE_REGIONS || sv_page==SV_TIMEZONE_CITIES){stz_redraw();
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
    sv_frame_submitted();
#endif
    return;}
#endif
  if(pp_enabled()){sp_redraw();return;}
  sv_begin();
  if(sv_page==SV_ROOT)sv_root();else if(sv_page==SV_FIELDS)sv_fields();
  else if(sv_page==SV_TIME_FORMAT)sv_time_format();
#ifdef PORTABLE_SLEEP_SETTINGS
  else if(sv_page==SV_SLEEP)sv_sleep();
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
  else if(sv_page==SV_TIMER)sv_timer();
#endif
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  else if(sv_page==SV_ALERT)sv_alert();
#endif
  else if(sv_page==SV_VALUE)sv_value();else if(sv_page==SV_FOLD)sv_fold();else sv_about();
  present(false);
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
  sv_frame_submitted();
#endif
}
static void sv_switch(unsigned page) {
  if(failed)return; /* Retained sleep must unwind without provider I/O. */
  if(pp_enabled())sp_clean=true;
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
  if(sv_map_epoch==~0u){failed=true;return;}++sv_map_epoch;
#endif
  sv_page=page;sv_need_neutral=true;sv_gesture=false;sv_axis=0;sv_dragged=false;
#ifdef PORTABLE_TOUCH_SCROLL
  settings_view_interrupt();
  /* A page enum can be revisited while another page is still in flight. */
  stz_submitted_page=stz_completed_page=~0u;
  stz_scroll_reset(page);
#ifdef PORTABLE_SETTINGS_LIST_SCROLL
  sp_submitted_page=sp_completed_page=~0u;
#endif
#endif
  input_pending=false;portable_touch_cancel_gesture(&touch);
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();
#endif
  settings_editing=page==SV_FIELDS || page==SV_VALUE || page==SV_FOLD || page==SV_TIME_FORMAT
#ifdef PORTABLE_SLEEP_SETTINGS
      || page==SV_SLEEP
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
      || page==SV_TIMER
#endif
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
      || page==SV_DESK_FACE || page==SV_FLIP || page==SV_LANGUAGE
#endif
#ifdef PORTABLE_SETTINGS_DESK_DIRECTION
      || page==SV_DESK_DIRECTION
#endif
#ifdef PORTABLE_SETTINGS_TIME_ZONE
      || page==SV_TIMEZONE_REGIONS || page==SV_TIMEZONE_CITIES
#endif
#ifdef PORTABLE_ALARM_SETTINGS
      || page==SV_ALERT
#endif
      ;
  settings_view_redraw();
}
static bool sv_inside(int x,int y,int left,int top,int w,int h) {
#ifdef PORTABLE_NOVA_UI
  sv_geometry(&left,&top,&w,&h);
#endif
  sv_native_button_geometry(&left,&top,&w,&h);
  return x>=left && x<left+w && y>=top && y<top+h;
}
static void sv_back_input(t5_app_input_t *out) {
  out->buttons=T5_APP_BUTTON_BACK;
  if(sv_page==SV_ROOT)out->exit_requested=true;
}
static void sv_slider(int x) {
  unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);
  if(x<64)x=64;
  if(x>176)x=176;
  editor_set_value(lo+(unsigned)(x-64)*(hi-lo)/112);
  settings_view_redraw();
}
#include "settings_paper.inc"
#ifdef PORTABLE_SETTINGS_TIME_ZONE
#include "settings_timezone_view.inc"
#endif
static bool settings_view_poll(t5_app_input_t *out) {
  if(!sv_active)return false;
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
  sv_frame_completed();
#endif
  if(sp_dirty && !input_progressed && !pp_enabled())settings_view_redraw();
  if(pp_enabled()) {
    bool handled;
#ifdef PORTABLE_SETTINGS_TIME_ZONE
    if(sv_page==SV_TIMEZONE_REGIONS || sv_page==SV_TIMEZONE_CITIES)handled=stz_poll(out);
    else
#endif
      handled=sp_poll(out);
    /* The controller consumes emitted actions after poll returns. Its redraw
     * will paint that new state; otherwise a neutral poll retries pending work. */
    if(sp_dirty && !out->buttons && !out->tapped && !out->exit_requested)settings_view_redraw();
    return handled;
  }
#ifdef PORTABLE_SETTINGS_TIME_ZONE
  if(sv_page==SV_TIMEZONE_REGIONS || sv_page==SV_TIMEZONE_CITIES)return stz_poll(out);
#endif
  settings_navigation_poll(out);
  if(out->buttons&T5_APP_BUTTON_BACK){sv_back_input(out);return true;}
  portable_touch_sample sample;
  input_take(&sample);
  if(!sample.valid || sample.cancelled) {
    sv_gesture=false;sv_need_neutral=true;return true;
  }
  int x=sv_layout_x(sample.x),y=sv_layout_y(sample.y);
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
  if(!sv_frame_current()){sv_gesture=false;sv_need_neutral=true;return true;}
#endif
  bool fresh_tap=sample.released && sample.tap_eligible && !sample.moved;
  if(sv_need_neutral) {
    if(!sample.down || (sample.began && sample.tap_eligible))sv_need_neutral=false;
    else return true;
  }
  if(fresh_tap && !sv_gesture) {
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
    sv_gesture_map=sv_visible_map;
#endif
    sv_gesture=sv_eligible=true;sv_start_x=sv_last_x=x;sv_start_y=sv_last_y=y;
    sv_axis=0;sv_dragged=false;
  }
  if(sample.began) {
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
    sv_gesture_map=sv_visible_map;
#endif
    sv_gesture=true;sv_eligible=sample.tap_eligible && x>=0 && x<240 && y>=0 && y<240;
    sv_start_x=sv_last_x=x;sv_start_y=sv_last_y=y;sv_axis=0;sv_dragged=false;
#ifdef PORTABLE_QUICK_ACTIONS
    if(quick_replay_pending){sv_start_x=quick.ui.start_x;sv_start_y=quick.ui.start_y;quick_replay_pending=false;}
#endif
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
    sv_start_scroll=sv_gesture_map.scroll[sv_page==SV_ROOT?0:1];
#else
    sv_start_scroll=sv_scroll[sv_page==SV_ROOT?0:1];
#endif
#ifndef PORTABLE_NOVA_UI
    if(sv_eligible && sv_page==SV_VALUE && y>=116 && y<142 && x>=64 && x<=176) {
      sv_axis=3;sv_slider(x);
    }
#endif
  }
  if(sample.down && sv_gesture && sv_eligible) {
    int dx=x-sv_start_x,dy=y-sv_start_y;
    sv_last_x=x;sv_last_y=y;
    if(sample.moved || sv_abs(dx)+sv_abs(dy)>=6)sv_dragged=true;
    if(!sv_axis && sv_dragged) {
      if(sv_abs(dy)>sv_abs(dx))sv_axis=1;else sv_axis=2;
    }
    if(sv_axis==1 && (sv_page==SV_ROOT || sv_page==SV_FIELDS)) {
      unsigned p=sv_page==SV_ROOT?0:1;int before=sv_scroll[p];
      sv_scroll[p]=sv_start_scroll-dy;sv_clamp_scroll();
      if(before!=sv_scroll[p])settings_view_redraw();
    } else if(sv_axis==3)sv_slider(x);
  }
  if(sample.released && sv_gesture) {
    sv_gesture=false;
    if(!sv_eligible)return true;
    int dx=sv_last_x-sv_start_x,dy=sv_last_y-sv_start_y;
    if(sv_axis==2 && dx>=36 && dx>sv_abs(dy)*2) {sv_back_input(out);return true;}
    if(sv_dragged || sample.moved || sv_axis==3)return true;
    x=sv_start_x;y=sv_start_y;
#ifdef PORTABLE_SETTINGS_SLICED_FRAMES
    if(sv_gesture_map.epoch!=sv_map_epoch || sv_gesture_map.page!=sv_page || sv_gesture_map.receipt!=sv_visible_map.receipt)return true;
    sv_action_map=sv_gesture_map;
#endif
    if(sv_page==SV_FIELDS || sv_page==SV_TIME_FORMAT
#ifdef PORTABLE_SLEEP_SETTINGS
       || sv_page==SV_SLEEP
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
       || sv_page==SV_TIMER
#endif
#endif
#ifdef PORTABLE_ALARM_SETTINGS
       || sv_page==SV_ALERT
#endif
      ) {
      if(sv_inside(x,y,SV_CANCEL_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H)) {sv_back_input(out);return true;}
      if(sv_inside(x,y,SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H)) {editor_field=6;out->buttons=T5_APP_BUTTON_CONFIRM;return true;}
    } else if(sv_inside(x,y,82,192,76,26)) {sv_back_input(out);return true;}
    if(sv_page==SV_VALUE && sv_inside(x,y,24,66,36,36))out->buttons=T5_APP_BUTTON_LEFT;
    else if(sv_page==SV_VALUE && sv_inside(x,y,180,66,36,36))out->buttons=T5_APP_BUTTON_RIGHT;
    else {out->tapped=true;out->touch_x=(int16_t)x;out->touch_y=(int16_t)y;}
    return true;
  }

  if((out->buttons&T5_APP_BUTTON_BACK) && sv_page==SV_ROOT)out->exit_requested=true;
  return true;
}
