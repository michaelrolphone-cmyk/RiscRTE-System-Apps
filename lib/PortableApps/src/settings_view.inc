/* Compact app-local presentation, following the supplied 240x240 Settings
 * reference. All functional values come from the shared Settings model. */
#include "../settings_fonts/text.inc"
#define SV_CYAN 0x19e3ffu
#define SV_DIM 0x0e4f5cu
#define SV_LINE 0x12262bu
#define SV_CAP 0x6b8288u
#define SV_TEXT 0xcfe9eeu
enum { SV_ROOT, SV_FIELDS, SV_VALUE, SV_FOLD, SV_ABOUT, SV_SLEEP };
static unsigned sv_page, sv_selected, sv_fold_choice;
static int sv_scroll[2];
static bool sv_active, sv_need_neutral, sv_gesture, sv_eligible;
static unsigned sv_axis; /* 1 vertical, 2 horizontal, 3 numeric slider */
static int sv_start_x, sv_start_y, sv_last_x, sv_last_y, sv_start_scroll;
static bool sv_dragged;
static void settings_view_redraw(void);
static int sv_abs(int n) { return n < 0 ? -n : n; }
static void sv_pixel(int x, int y, uint32_t rgb, unsigned alpha) {
  if (!surface.frame || x < 0 || x >= 240 || y < 0 || y >= 240 || !alpha) return;
  x += (width() - 240) / 2; y += (height() - 240) / 2;
  uint8_t *p = (uint8_t *)surface.pixels + (size_t)y * surface.stride_bytes + (size_t)x * 2;
  uint16_t before = (uint16_t)(p[0] | p[1] << 8);
  if (alpha > 255) alpha = 255;
  unsigned r = (((before >> 11) & 31) * 255 / 31 * (255-alpha) + ((rgb >> 16) & 255) * alpha) / 255;
  unsigned g = (((before >> 5) & 63) * 255 / 63 * (255-alpha) + ((rgb >> 8) & 255) * alpha) / 255;
  unsigned b = ((before & 31) * 255 / 31 * (255-alpha) + (rgb & 255) * alpha) / 255;
  uint16_t c = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
  p[0] = (uint8_t)c; p[1] = (uint8_t)(c >> 8);
}
static void sv_rect(int x, int y, int w, int h, uint32_t color) {
  for (int j = y; j < y+h; ++j) for (int i = x; i < x+w; ++i) sv_pixel(i,j,color,255);
}
static int sv_measure(unsigned face, const char *s, int spacing) {
  int length = 0;
  if (!s) return 0;
  for (unsigned i=0; i<96 && s[i]; ++i) {
    unsigned ch=(unsigned char)s[i]; if(ch<32 || ch>126) ch='?';
    length += rps_text[face*95+ch-32].advance_q4 + (i?spacing:0);
  }
  return (length+15)/16;
}
static void sv_text(unsigned face, int x, int y, const char *s, uint32_t color, int spacing) {
  if (!s || face > 5) return;
  int pen=x*16;
  for (unsigned k=0;k<96 && s[k];++k) {
    unsigned ch=(unsigned char)s[k]; if(ch<32 || ch>126) ch='?';
    const rps_glyph *g=&rps_text[face*95+ch-32];
    for(unsigned j=0;j<g->height;++j)for(unsigned i=0;i<g->width;++i) {
      unsigned n=j*g->width+i,coverage=(g->bits[n/4]>>(6-2*(n%4)))&3;
      sv_pixel(pen/16+g->left+(int)i,y+g->top+(int)j,color,coverage*85);
    }
    pen += g->advance_q4 + spacing;
  }
}
static void sv_center(unsigned face,int y,const char *s,uint32_t color,int spacing) {
  sv_text(face,(240-sv_measure(face,s,spacing))/2,y,s,color,spacing);
}
static void sv_right(unsigned face,int x,int y,const char *s,uint32_t color,int spacing) {
  sv_text(face,x-sv_measure(face,s,spacing),y,s,color,spacing);
}
static void sv_button(int x,int y,int w,int h,const char *caption,bool selected) {
  /* Rounded outline follows the mockup, with a usable complete hit rectangle. */
  for(int j=0;j<h;++j)for(int i=0;i<w;++i) {
    int dx=i<6?6-i:i>=w-6?i-(w-7):0;
    int dy=j<6?6-j:j>=h-6?j-(h-7):0;
    if(dx*dx+dy*dy>36)continue;
    bool edge=i==0 || i==w-1 || j==0 || j==h-1 || (dx && dy && dx*dx+dy*dy>22);
    sv_pixel(x+i,y+j,edge?(selected?SV_CYAN:SV_DIM):0,255);
  }
  sv_text(2,x+(w-sv_measure(2,caption,18))/2,y+(h-12)/2-2,caption,SV_CYAN,18);
}
static void sv_fade(int top,int bottom) {
  for(int y=top;y<bottom;++y) {
    unsigned opacity=255;
    int a=y-top,b=bottom-1-y;
    int distance=a<b?a:b;
    if(distance<14)opacity=(unsigned)distance*102/14;
    else if(distance<46)opacity=102+(unsigned)(distance-14)*153/32;
    for(int x=0;x<240;++x)sv_pixel(x,y,0,255-opacity);
  }
}
static void sv_begin(void) { clear(); fill(0,0,width(),height(),0); }
static void sv_heading(const char *title) {
  sv_text(2,20,24,title,SV_CAP,28);sv_rect(20,46,200,1,SV_DIM);
}
static void sv_back(void) { sv_button(82,192,76,26,"< BACK",false); }
static int sv_row_y(unsigned index) { return index<3?124+(int)index*40:278+(int)(index-3)*40; }
static int sv_max_scroll(void) { return sv_page==SV_ROOT?220+(int)(SETTINGS_ROW_COUNT-5u)*40:144; }
static void sv_clamp_scroll(void) {
  unsigned p=sv_page==SV_ROOT?0:1;
  if(sv_scroll[p]<0)sv_scroll[p]=0;
  if(sv_scroll[p]>sv_max_scroll())sv_scroll[p]=sv_max_scroll();
}
static void sv_root(void) {
  int offset=sv_scroll[0];
  sv_text(0,20,50-offset,"RiscRTE",SV_CYAN,18);
  sv_text(2,20,72-offset,"Settings",SV_CAP,22);
  sv_rect(20,94-offset,200,1,SV_DIM);
  sv_text(3,20,104-offset,"TIME",0x0e8ea3,38);
  sv_text(3,20,258-offset,"SYSTEM",0x0e8ea3,38);
  for(unsigned i=0;i<SETTINGS_ROW_COUNT;++i) {
    t5_app_setting_t row; if(!settings_get(0,i,&row))continue;
    int y=sv_row_y(i)-offset;
    sv_rect(20,y,2,40,sv_selected==i+1?SV_CYAN:SV_LINE);
    sv_rect(22,y+39,198,1,SV_LINE);
    sv_text(1,32,y+10,row.label,0xffffff,4);
    sv_right(2,row.type==T5_APP_SETTING_ACTION?204:216,y+12,row.value,SV_CYAN,0);
    if(row.type==T5_APP_SETTING_ACTION)sv_text(1,211,y+9,">",SV_DIM,0);
  }
  sv_fade(0,192);sv_rect(0,190,240,50,0);
  if(settings_message[0])sv_center(3,173,settings_message,SV_CYAN,0);
  sv_back();
}
static void sv_fields(void) {
  sv_heading("TIME AND DATE");
  for(unsigned i=0;i<6;++i) {
    int y=60+(int)i*40-sv_scroll[1];
    if(y+40<=50 || y>=184)continue;
    char value[24];editor_value_text(i,value,sizeof(value));
    sv_rect(20,y,2,40,editor_field==i?SV_CYAN:SV_LINE);
    sv_rect(22,y+39,198,1,SV_LINE);
    sv_text(1,32,y+10,editor_names[i],0xffffff,4);
    sv_right(1,202,y+10,value,SV_CYAN,0);sv_text(1,211,y+9,">",SV_DIM,0);
  }
  /* Keep the fixed heading/buttons intact while rows move behind them. */
  sv_fade(50,188);sv_rect(0,0,240,50,0);sv_heading("TIME AND DATE");
  sv_rect(0,188,240,52,0);
  sv_button(24,200,86,26,"CANCEL",editor_field==7);
  sv_button(130,200,86,26,"SAVE",editor_field==6);
  sv_center(3,185,editor_message,SV_CAP,0);
}
static void sv_value(void) {
  sv_heading(editor_names[editor_field]);
  char value[24]; unsigned v=editor_value(editor_field),shown=v;
  if(editor_field==3)shown=v%12?v%12:12;
  snprintf(value,sizeof(value),"%u",shown);
  sv_center(4,59,value,SV_CYAN,0);
  sv_button(24,66,36,36,"",false);sv_button(180,66,36,36,"",false);
  for(unsigned i=0;i<2;++i) {
    const char *symbol=i?"+":"-";const rps_glyph *g=&rps_text[5*95+(unsigned)symbol[0]-32];
    sv_text(5,(i?180:24)+(36-(int)g->advance_q4/16)/2,66+(36-g->height)/2-g->top,symbol,SV_CYAN,0);
  }
  unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);
  unsigned filled=hi>lo?(v-lo)*10/(hi-lo):0;
  for(unsigned i=0;i<10;++i)sv_rect(72+(int)i*10,124,7,8,i<filled?SV_CYAN:0x032027);
  if(editor_field==3)sv_center(1,145,v<12?"AM":"PM",SV_TEXT,12);
  else {snprintf(value,sizeof(value),"%u - %u",lo,hi);sv_center(2,145,value,SV_CAP,12);}
  sv_center(3,163,"Draft only - use SAVE",SV_CAP,2);sv_back();
}
static void sv_fold(void) {
  sv_heading("REPEATED LOCAL TIME");
  sv_center(2,56,"This hour occurs twice",SV_TEXT,0);
  sv_center(2,72,"Choose which time to save",SV_CAP,0);
  sv_button(28,100,184,32,"SAVE FIRST  MDT",sv_fold_choice==0);
  sv_button(28,142,184,32,"SAVE SECOND MST",sv_fold_choice==1);
  sv_back();
}
static void sv_about(void) {
  sv_heading("ABOUT");
  sv_text(1,20,68,"Settings",SV_CAP,0);sv_right(1,220,68,PORTABLE_SETTINGS_VERSION,SV_TEXT,0);
  sv_rect(20,93,200,1,SV_LINE);
  char geometry[24];snprintf(geometry,sizeof(geometry),"%d x %d",(int)width(),(int)height());
  sv_text(1,20,103,"Display",SV_CAP,0);sv_right(1,220,103,geometry,SV_TEXT,0);
  sv_rect(20,128,200,1,SV_LINE);
  sv_center(2,148,"Only reported device data",SV_CAP,0);sv_back();
}
#ifdef PORTABLE_SLEEP_SETTINGS
static void sv_sleep(void) {
  sv_heading("SLEEP MODE");
  sv_button(28,66,184,34,"LIGHT SLEEP",sleep_choice==PORTABLE_SLEEP_LIGHT);
  sv_button(28,110,184,34,"DEEP SLEEP",sleep_choice==PORTABLE_SLEEP_DEEP);
  sv_center(3,151,"Deep restarts Clock on wake",SV_CAP,0);
  sv_center(3,166,"Crown wake - 60 second idle",SV_CAP,0);
  sv_center(3,184,sleep_message,SV_CYAN,0);
  sv_button(24,200,86,26,"CANCEL",false);
  sv_button(130,200,86,26,"SAVE",false);
}
#endif
static void settings_view_redraw(void) {
  if(!sv_active)return;
  sv_begin();
  if(sv_page==SV_ROOT)sv_root();else if(sv_page==SV_FIELDS)sv_fields();
  #ifdef PORTABLE_SLEEP_SETTINGS
  else if(sv_page==SV_SLEEP)sv_sleep();
#endif
  else if(sv_page==SV_VALUE)sv_value();else if(sv_page==SV_FOLD)sv_fold();else sv_about();
  present(false);
}
static void sv_switch(unsigned page) {
  sv_page=page;sv_need_neutral=true;sv_gesture=false;sv_axis=0;sv_dragged=false;
  input_pending=false;touch.neutral=touch.down=false;
#ifdef PORTABLE_INPUT_NAVIGATION
  input_navigation_reset();
#endif
  settings_editing=page==SV_FIELDS || page==SV_VALUE || page==SV_FOLD
#ifdef PORTABLE_SLEEP_SETTINGS
      || page==SV_SLEEP
#endif
      ;
  settings_view_redraw();
}
static bool sv_inside(int x,int y,int left,int top,int w,int h) {
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
static bool settings_view_poll(t5_app_input_t *out) {
  if(!sv_active)return false;
  settings_navigation_poll(out);
  if(out->buttons&T5_APP_BUTTON_BACK){sv_back_input(out);return true;}
  portable_touch_sample sample;
  input_take(&sample);
  if(!sample.valid || sample.cancelled) {
    sv_gesture=false;sv_need_neutral=true;return true;
  }
  int x=(int)sample.x-(width()-240)/2,y=(int)sample.y-(height()-240)/2;
  bool fresh_tap=sample.released && sample.tap_eligible && !sample.moved;
  if(sv_need_neutral) {
    if(!sample.down || (sample.began && sample.tap_eligible))sv_need_neutral=false;
    else return true;
  }
  if(fresh_tap && !sv_gesture) {
    sv_gesture=sv_eligible=true;sv_start_x=sv_last_x=x;sv_start_y=sv_last_y=y;
    sv_axis=0;sv_dragged=false;
  }
  if(sample.began) {
    sv_gesture=true;sv_eligible=sample.tap_eligible && x>=0 && x<240 && y>=0 && y<240;
    sv_start_x=sv_last_x=x;sv_start_y=sv_last_y=y;sv_axis=0;sv_dragged=false;
    sv_start_scroll=sv_scroll[sv_page==SV_ROOT?0:1];
    if(sv_eligible && sv_page==SV_VALUE && y>=116 && y<142 && x>=64 && x<=176) {
      sv_axis=3;sv_slider(x);
    }
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
    if(sv_page==SV_FIELDS
#ifdef PORTABLE_SLEEP_SETTINGS
       || sv_page==SV_SLEEP
#endif
      ) {
      if(sv_inside(x,y,24,200,86,26)) {sv_back_input(out);return true;}
      if(sv_inside(x,y,130,200,86,26)) {editor_field=6;out->buttons=T5_APP_BUTTON_CONFIRM;return true;}
    } else if(sv_inside(x,y,82,192,76,26)) {sv_back_input(out);return true;}
    if(sv_page==SV_VALUE && sv_inside(x,y,24,66,36,36))out->buttons=T5_APP_BUTTON_LEFT;
    else if(sv_page==SV_VALUE && sv_inside(x,y,180,66,36,36))out->buttons=T5_APP_BUTTON_RIGHT;
    else {out->tapped=true;out->touch_x=(int16_t)x;out->touch_y=(int16_t)y;}
    return true;
  }

  if((out->buttons&T5_APP_BUTTON_BACK) && sv_page==SV_ROOT)out->exit_requested=true;
  return true;
}
