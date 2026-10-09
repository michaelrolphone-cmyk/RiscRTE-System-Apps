/* Opt-in Settings-only timezone list. No changes to the shared paper renderer.
 * Each row wraps complete city names, including subordinate paths such as
 * Argentina/Buenos Aires. Page controls never silently wrap at either end. */
static unsigned stz_count(void) {
  return sv_page==SV_TIMEZONE_REGIONS?PORTABLE_TIMEZONE_REGION_COUNT:portable_timezone_region_count(stz_region);
}
static int stz_width(void) {return pp_enabled()?width():240;}
static int stz_height(void) {return pp_enabled()?height():240;}
static int stz_top(void) {return pp_enabled()?160:72;}
static int stz_step(void) {return pp_enabled()?72:34;}
static unsigned stz_rows(void) {
  return pp_enabled()?(unsigned)(height()-stz_top()-208)/72u:3u;
}
static int stz_margin(void) {return pp_enabled()?32:12;}
static void stz_visible(void) {stz_first=stz_selected/stz_rows()*stz_rows();}
static void stz_choose(unsigned row) {
  if(row>=stz_count())return;
  stz_selected=row;
  if(sv_page==SV_TIMEZONE_CITIES) {
    stz_choice=(unsigned)portable_timezone_city_index(stz_region,row);
    if(!stz_unconfirmed && (stz_status==PORTABLE_TIMEZONE_LOADED || stz_status==PORTABLE_TIMEZONE_MISSING))stz_message="";
  }
  stz_visible();
}
static bool stz_page_step(bool forward) {
  unsigned rows=stz_rows(),count=stz_count(),before=stz_first;
  if(forward){if(stz_first+rows<count)stz_choose(stz_first+rows);}
  else if(stz_first)stz_choose(stz_first>=rows?stz_first-rows:0);
  return stz_first!=before;
}
static void stz_name(unsigned row,char out[PORTABLE_TIMEZONE_ID_BYTES]) {
  if(sv_page==SV_TIMEZONE_REGIONS)strcpy(out,portable_timezone_region_name(row));
  else (void)portable_timezone_display_name((unsigned)portable_timezone_city_index(stz_region,row),out,PORTABLE_TIMEZONE_ID_BYTES);
}
static int stz_measure(const char *text) {return pp_enabled()?pp_measure(text,false):sv_measure(2,text,0);}
/* Complete bounded wrapping; callers reserve enough lines for the whole
 * canonical catalog. Tests verify every name fits, including the smallest UI. */
static unsigned stz_lines(const char *text,int available,char lines[3][PORTABLE_TIMEZONE_ID_BYTES]) {
  unsigned at=0,line=0;
  memset(lines,0,3*PORTABLE_TIMEZONE_ID_BYTES);
  while(text[at] && line<3) {
    unsigned n=0,last_break=0;
    while(text[at+n] && n+1<PORTABLE_TIMEZONE_ID_BYTES) {
      lines[line][n]=text[at+n];lines[line][n+1]=0;
      if(stz_measure(lines[line])>available){lines[line][n]=0;break;}
      if(text[at+n]==' ' || text[at+n]=='/')last_break=n+1;
      ++n;
    }
    if(!n)break;
    if(text[at+n] && last_break)n=last_break;
    lines[line][n]=0;at+=n;++line;
  }
  return text[at]?0:line;
}
static void stz_text(int x,int y,int available,const char *text,bool black) {
  if(pp_enabled())sp_text(x,y,available,text,false,black);
  else sv_text(2,x,y,text,black?SV_TEXT:0,0);
}
static void stz_wrapped(int x,int y,int available,const char *text,bool black) {
  char lines[3][PORTABLE_TIMEZONE_ID_BYTES];unsigned n=stz_lines(text,available,lines);
  for(unsigned i=0;i<n;++i)stz_text(x,y+(int)i*(pp_enabled()?28:13),available,lines[i],black);
}
static void stz_button(int x,int y,int w,int h,const char *text,bool selected) {
  if(pp_enabled()){sp_button(x,y,w,h,text,selected);return;}
  sv_rect(x,y,w,h,selected?SV_CYAN:SV_DIM);sv_rect(x+1,y+1,w-2,h-2,0);
  sv_text(2,x+(w-sv_measure(2,text,0))/2,y+(h-12)/2-1,text,SV_CYAN,0);
}
static void stz_redraw(void) {
  bool paper=pp_enabled(),cities=sv_page==SV_TIMEZONE_CITIES;
  int w=stz_width(),h=stz_height(),margin=stz_margin(),inside=w-2*margin;
  if(paper){pp_begin();sp_header(cities?portable_timezone_region_name(stz_region):"TIME ZONE: REGION");}
  else {sv_begin();sv_heading(cities?portable_timezone_region_name(stz_region):"TIME ZONE: REGION");}
  stz_wrapped(margin,paper?104:48,inside,stz_message[0]?stz_message:
      cities?"Draft only - Save confirms":"Choose region - then city",true);
  unsigned count=stz_count();
  for(unsigned i=0;i<stz_rows() && stz_first+i<count;++i) {
    unsigned row=stz_first+i;int y=stz_top()+(int)i*stz_step();char name[PORTABLE_TIMEZONE_ID_BYTES];stz_name(row,name);
    if(paper){
      if(row==stz_selected)fill(margin,y,inside,stz_step(),0);
      fill(margin,y+stz_step()-2,inside,2,0);
    }else {
      sv_rect(margin,y,2,stz_step(),row==stz_selected?SV_CYAN:SV_LINE);
      sv_rect(margin,y+stz_step()-1,inside,1,SV_LINE);
    }
    stz_wrapped(margin+12,y+(paper?7:3),inside-24,name,!paper||row!=stz_selected);
  }
  int page_y=paper?h-192:178,button_h=paper?56:24;
  int page_w=paper?112:60;char position[24];
  snprintf(position,sizeof(position),"%u / %u",stz_first/stz_rows()+1,(count+stz_rows()-1)/stz_rows());
  stz_button(margin,page_y,page_w,button_h,"PREV",false);
  stz_button(w-margin-page_w,page_y,page_w,button_h,"NEXT",false);
  stz_text((w-stz_measure(position))/2,page_y+(paper?14:5),inside,position,true);
  int foot_y=paper?h-112:210,foot_h=paper?88:28,gap=paper?12:6;
  int columns=cities?3:2,foot_w=(inside-(columns-1)*gap)/columns;
  if(cities)stz_button(margin,foot_y,foot_w,foot_h,"BACK",false);
  stz_button(margin+(cities?foot_w+gap:0),foot_y,foot_w,foot_h,"CANCEL",false);
  stz_button(margin+(columns-1)*(foot_w+gap),foot_y,foot_w,foot_h,cities?"SAVE":"OPEN",true);
  present(paper?sp_clean:false);sp_clean=false;
}
/* Keep touch handling local: cancelled/lost samples destroy the entire gesture,
 * so an interrupted swipe cannot select a row or activate a footer on release. */
static bool stz_poll(t5_app_input_t *out) {
  settings_navigation_poll(out);
  portable_touch_sample sample;input_take(&sample);
  if(!sample.valid || sample.cancelled){sv_gesture=false;sv_need_neutral=true;return true;}
  int x=sample.x-(pp_enabled()?0:(width()-240)/2),y=sample.y-(pp_enabled()?0:(height()-240)/2);
  if(sv_need_neutral){if(!sample.down || (sample.began&&sample.tap_eligible))sv_need_neutral=false;else return true;}
  if(sample.began){sv_gesture=true;sv_eligible=sample.tap_eligible;sv_start_x=sv_last_x=x;sv_start_y=sv_last_y=y;sv_dragged=false;}
  if(sample.down&&sv_gesture){sv_last_x=x;sv_last_y=y;sv_dragged|=sample.moved;}
  if(!sample.released)return true;
  bool eligible=sv_gesture&&sv_eligible;
  int dx=sv_last_x-sv_start_x,dy=sv_last_y-sv_start_y;sv_gesture=false;
  if(!eligible)return true;
  if(sv_dragged||sample.moved) {
    int threshold=pp_enabled()?44:28;
    if(sv_abs(dy)>=threshold && sv_abs(dy)>sv_abs(dx)*2) {if(stz_page_step(dy<0))settings_view_redraw();}
    else if(dx>=threshold && dx>sv_abs(dy)*2)out->buttons=T5_APP_BUTTON_BACK;
    return true;
  }
  if(sample.tap_eligible){out->tapped=true;out->touch_x=(int16_t)x;out->touch_y=(int16_t)y;}
  return true;
}
