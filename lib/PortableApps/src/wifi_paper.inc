#include <stdio.h>
/* Capability-selected paper presentation. Shared PaperPresentation typography,
 * MONO1 drawing and licensed FontAwesome masks; no radio or storage policy. */
#define WPV_TOP 156
#define WPV_ROW 80
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
#include "wifi_scroll.inc"
#endif
static unsigned wpv_rows(void) {return (unsigned)(height()-WPV_TOP-176)/WPV_ROW;}
static bool wpv_inside(int x,int y,int left,int top,int w,int h) {
  return x>=left && x<left+w && y>=top && y<top+h;
}
static void wpv_text(int x,int y,int w,const char *s,bool black) {
  pp_text(x,y,w,s,1|PAPER_TEXT_LITERAL,false,black);
}
/* Wrap complete words where possible, preserving SSID case. Truncation remains
 * bounded; the editor shows its tail, never unmasked password bytes. */
static void wpv_wrap(int x,int y,int w,const char *s,unsigned lines,bool black) {
  if(!s)return;
  for(unsigned line=0;line<lines && *s;line++) {
    char part[97];unsigned n=0,space=0;
    while(s[n] && n<96) {
      part[n]=s[n];part[n+1]=0;
      if(pp_measure(part,false)>w)break;
      if(s[n]==' ')space=n;
      n++;
    }
    if(!n)return;
    if(s[n] && space)n=space;
    memcpy(part,s,n);part[n]=0;wpv_text(x,y+(int)line*26,w,part,black);
    s+=n;while(*s==' ')s++;
  }
}
static void wpv_button(int x,int y,int w,int h,const char *label,const char *glyph,bool selected) {
  fill(x,y,w,h,0);if(!selected)fill(x+3,y+3,w-6,h-6,0xffff);
  if(glyph)(void)icon(x+(w-24)/2,y+(h-24)/2,glyph,24,!selected);
  else wpv_text(x+(w-pp_measure(label,false))/2,y+(h-26)/2,w-8,label,!selected);
}
static portable_watch_key_rect wpv_key(unsigned key) {
  int step=(width()-64)/8,kh=(height()-392)/4;if(kh>80)kh=80;
  if(key<PWK_CHARACTERS)return (portable_watch_key_rect){32+(int)(key%8)*step,184+(int)(key/8)*kh,step-4,kh-6};
  int action=(width()-64)/3;
  return (portable_watch_key_rect){32+(int)(key-PWK_PAGE)*action,184+4*kh+16,action-8,56};
}
static int wifi_paper_hit(const portable_wifi_view *v,int x,int y) {
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
  if(wpv_identity!=v->identity || !wpv_hit_ready)return -1;
  if(wpv_inside(x,y,32,height()-88,width()-64,64))return WIFI_HIT_BACK;
  if(!v->keyboard)return wpv_touch_row;
#endif
  if(wpv_inside(x,y,32,height()-88,width()-64,64)) {
    if(v->keyboard || x<width()/2-8)return WIFI_HIT_BACK;
    if(x>=width()/2+8) {
      if(x<width()*3/4-4)return WIFI_HIT_PREVIOUS;
      if(x>=width()*3/4+4)return WIFI_HIT_NEXT;
    }
    return -1;
  }
  if(v->keyboard) {
    for(unsigned i=0;i<PWK_COUNT;i++) {portable_watch_key_rect k=wpv_key(i);if(wpv_inside(x,y,k.x,k.y,k.w,k.h))return (int)i;}
  } else {
    unsigned rows=wpv_rows(),first=v->selected/rows*rows;
    if(wpv_inside(x,y,32,WPV_TOP,width()-64,(int)rows*WPV_ROW)) {
      unsigned i=first+(unsigned)(y-WPV_TOP)/WPV_ROW;
      if(i<v->count)return (int)i;
    }
  }
  return -1;
}
static void wifi_paper_render(const portable_wifi_view *v) {
  if(failed)return;
  pp_begin();
  /* Long scan/privacy titles use two body lines, rather than clipping a title. */
  if(pp_measure(v->title,true)<=width()-112)pp_text(32,30,width()-112,v->title,2,true,true);
  else wpv_wrap(32,24,width()-112,v->title,2,true);
  (void)icon(width()-64,36,"solid:f1eb",32,true);fill(32,88,width()-64,3,0);
  if(v->keyboard) {
    wpv_text(32,112,width()-64,v->entry,true);
    wpv_text(32,146,width()-64,"ABC/# changes the character page",true);
    for(unsigned i=0;i<PWK_COUNT;i++) {
      portable_watch_key_rect k=wpv_key(i);unsigned ch=portable_watch_key_character(v->key_page,i);
      char label[]={(char)(ch?ch:' '),0};
      wpv_button(k.x,k.y,k.w,k.h,i==PWK_PAGE?"ABC/#":i==PWK_DELETE?"DELETE":i==PWK_DONE?"DONE":ch==32?"_":label,NULL,v->selected==i);
    }
    wpv_wrap(32,height()-132,width()-64,v->message,1,true);
    wpv_button(32,height()-88,width()-64,64,"", "solid:f053",false);
  } else {
    wpv_text(32,110,width()-64,v->detail,true);
    unsigned rows=wpv_rows(),first=v->selected/rows*rows;
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    wpv_scroll_visible();
    first=(unsigned)portable_scroll_offset(&wpv_scroll)/WPV_ROW;
    rows=(unsigned)wpv_scroll.view.height/WPV_ROW+2;
    raster_clip=wpv_scroll.view;raster_clip_active=true;
#endif
    for(unsigned j=0;j<rows && first+j<v->count;j++) {
      unsigned i=first+j;int y=WPV_TOP+(int)j*WPV_ROW;bool selected=i==v->selected;
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
      y=portable_scroll_row_y(&wpv_scroll,(int)i);
#endif
      if(selected)fill(32,y,width()-64,WPV_ROW,0);
      wpv_text(48,y+8,width()-96,v->rows[i],!selected);
      wpv_wrap(48,y+34,width()-96,v->values[i],1,!selected);
      fill(32,y+WPV_ROW-3,width()-64,3,0);
    }
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    raster_clip_active=false;
    if(wpv_scroll.limit) {
      int bar=wpv_scroll.view.height*wpv_scroll.view.height/(wpv_scroll.count*WPV_ROW);
      if(bar<24)bar=24;
      int y=WPV_TOP+(wpv_scroll.view.height-bar)*portable_scroll_offset(&wpv_scroll)/wpv_scroll.limit;
      fill(width()-24,y,4,bar,0);
    }
    wpv_wrap(32,height()-156,width()-64,v->message,2,true);
    wpv_button(32,height()-88,width()-64,64,"BACK",NULL,false);
#else
    wpv_wrap(32,height()-172,width()-64,v->message,2,true);
    char position[32];snprintf(position,sizeof(position),"%u / %u",v->selected+1,v->count);
    wpv_text(32,height()-116,width()-64,position,true);
    wpv_button(32,height()-88,width()/2-40,64,"BACK",NULL,false);
    wpv_button(width()/2+8,height()-88,width()/4-12,64,"","solid:f053",false);
    wpv_button(width()*3/4+4,height()-88,width()/4-36,64,"","solid:f054",false);
#endif
  }
  present(false);
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
  wpv_submitted_identity=v->identity;wpv_submitted_selected=v->selected;wpv_submitted_q8=wpv_scroll.position_q8;wpv_scroll_visible();
#endif
}
