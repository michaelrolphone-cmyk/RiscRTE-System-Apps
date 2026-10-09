/* Paper presentation only: existing Settings controllers own all changes,
 * persistence, RTC gap/fold verification and explicit Save semantics. */
#define SP_TOP 112
#define SP_ROW 88
static unsigned sp_first,sp_field_first;
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
#define SP_MORE_CAPTION (sp_first+rows<SETTINGS_ROW_COUNT?"MORE >":"FIRST PAGE")
#define SP_MORE_NEXT (sp_first+sp_rows()<SETTINGS_ROW_COUNT?sp_first+sp_rows():0)
#else
#define SP_MORE_CAPTION (sp_first?"PREVIOUS":"MORE >")
#define SP_MORE_NEXT (sp_first?0:sp_rows())
#endif
static unsigned sp_rows(void) {
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  unsigned reserve=sv_page==SV_ROOT || sv_page==SV_FIELDS?252u:144u;
  unsigned n=(unsigned)(height()-SP_TOP-(int)reserve)/SP_ROW;return n?n:1;
#else
  unsigned n=(unsigned)(height()-SP_TOP-144)/SP_ROW;return n?n:1;
#endif
}
static void sp_text(int x,int y,int w,const char *s,bool heading,bool black){pp_text(x,y,w,s,heading?2:1,heading,black);}
static void sp_header(const char *title) {sp_text(32,30,width()-64,title,true,true);fill(32,88,width()-64,3,0);}
static void sp_button(int x,int y,int w,int h,const char *caption,bool selected) {
  fill(x,y,w,h,0);if(!selected)fill(x+3,y+3,w-6,h-6,0xffff);
  const char *glyph=NULL,*text=caption;
  if(!strcmp(caption,"< BACK")){glyph="solid:f053";text="BACK";}
  else if(!strcmp(caption,"MORE >")){glyph="solid:f054";text="MORE";}
  else if(!strcmp(caption,"+")){glyph="solid:f067";text="";}
  else if(!strcmp(caption,"-")){glyph="solid:f068";text="";}
  int text_width=pp_measure(text,false),total=text_width+(glyph?(text[0]?36:24):0),left=x+(w-total)/2;
  if(glyph)(void)icon(left,y+(h-24)/2,glyph,24,!selected);
  if(text[0])sp_text(left+(glyph?36:0),y+(h-26)/2,w-12,text,false,!selected);
}
static void sp_footer(bool save) {
  if(save){sp_button(32,height()-112,(width()-80)/2,88,"CANCEL",false);sp_button(width()/2+8,height()-112,(width()-80)/2,88,"SAVE",true);}
  else sp_button(32,height()-112,width()-64,88,"< BACK",false);
}
static void sp_row(int y,const char *title,const char *value,bool selected,bool action) {
  if(selected)fill(32,y,width()-64,SP_ROW,0);
  sp_text(48,y+10,width()-112,title,false,!selected);
  if(value&&*value)sp_text(48,y+42,width()-112,value,false,!selected);
  if(action)(void)icon(width()-72,y+30,"solid:f054",24,!selected);
  fill(32,y+SP_ROW-3,width()-64,3,0);
}
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
static int sp_sleep_top(void) {return height()>=640?144:112;}
static int sp_sleep_step(void) {int step=(height()-sp_sleep_top()-160)/(int)settings_sleep_count();return step<112?step:112;}
static unsigned sp_face_columns(void) {return height()>=720?1u:2u;}
static int sp_face_step(void) {int step=(height()-280)/(int)(PORTABLE_DESK_FACE_COUNT/sp_face_columns());return step<84?step:84;}
static int sp_face_width(void) {return sp_face_columns()==1?width()-64:(width()-80)/2;}
#endif
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
static void sp_native_notice(bool fields) {
  char line[96];int y=height()-234;
  if(snt_result_available) {
    snprintf(line,sizeof(line),"RTC: %s",snt_outcome(snt_last_result.rtc_outcome));sp_text(32,y,width()-64,line,false,true);
    snprintf(line,sizeof(line),"Native: %s",snt_last_result.native.verified?"verified":snt_last_result.native.attempted?"unconfirmed":"not attempted");sp_text(32,y+34,width()-64,line,false,true);
    snprintf(line,sizeof(line),"Basis: %s",snt_outcome(snt_last_result.metadata_outcome));sp_text(32,y+68,width()-64,line,false,true);
  } else if(fields) {
    snprintf(line,sizeof(line),"Zone: %s",snt_zone_status==PORTABLE_TIMEZONE_MISSING?"UTC (default)":snt_zone_status==PORTABLE_TIMEZONE_LOADED?snt_zone_id:"Unavailable - choose zone");sp_text(32,y,width()-64,line,false,true);
    snprintf(line,sizeof(line),"RTC: %s",snt_basis_label());sp_text(32,y+34,width()-64,line,false,true);
    sp_text(32,y+68,width()-64,editor_message,false,true);
  } else sp_text(32,height()-142,width()-64,settings_message,false,true);
}
#endif
static void sp_redraw(void) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
  if(failed)return;
#endif
  pp_begin();
  const char *title=sv_page==SV_ROOT?"SETTINGS":sv_page==SV_FIELDS?"TIME AND DATE":sv_page==SV_VALUE?editor_names[editor_field]:

#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
    sv_page==SV_DESK_FACE?"CLOCK FACE":sv_page==SV_FLIP?"FLIP UI 180°":sv_page==SV_LANGUAGE?"LANGUAGE":
#endif
    sv_page==SV_TIME_FORMAT?"TIME FORMAT":sv_page==SV_FOLD?"REPEATED TIME":sv_page==SV_SLEEP?"SLEEP MODE":sv_page==SV_ALERT?"ALARM ALERTS":"ABOUT";
  sp_header(title);
  if(sv_page==SV_ROOT) {
    unsigned rows=sp_rows();if(sp_first>=SETTINGS_ROW_COUNT)sp_first=0;
    for(unsigned i=0;i<rows&&sp_first+i<SETTINGS_ROW_COUNT;i++) {
      t5_app_setting_t item;unsigned index=sp_first+i;
      bool available=settings_get(0,index,&item);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
      if(failed)return;
#endif
      if(available)sp_row(SP_TOP+(int)i*SP_ROW,item.label,item.value,sv_selected==index+1,item.type==T5_APP_SETTING_ACTION);
    }
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
    sp_native_notice(false);
#else
    sp_text(32,height()-142,width()-64,settings_message,false,true);
#endif
    if(SETTINGS_ROW_COUNT>rows) {
      sp_button(32,height()-112,(width()-80)/2,88,"< BACK",false);
      sp_button(width()/2+8,height()-112,(width()-80)/2,88,SP_MORE_CAPTION,false);
    } else sp_footer(false);
  } else if(sv_page==SV_FIELDS) {
    for(unsigned at=0;at<sp_rows()&&sp_field_first+at<6;at++){unsigned i=sp_field_first+at;char value[24];editor_value_text(i,value,sizeof(value));sp_row(SP_TOP+(int)at*SP_ROW,editor_names[i],value,editor_field==i,true);}
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
    sp_native_notice(true);
#else
    sp_text(32,height()-142,width()-64,editor_message,false,true);
#endif
    sp_footer(true);
  } else if(sv_page==SV_VALUE) {
    char value[32];editor_value_text(editor_field,value,sizeof(value));
    int w=pp_measure(value,true);sp_text((width()-w)/2,150,width()-64,value,true,true);
    sp_button(32,238,112,112,"-",false);sp_button(width()-144,238,112,112,"+",false);
    if(editor_field==3&&!settings_hour_24())sp_button(32,392,width()-64,88,settings_draft.hour<12?"AM":"PM",false);
    sp_text(32,520,width()-64,"DRAFT ONLY - USE SAVE",false,true);sp_footer(false);
  } else if(sv_page==SV_FOLD) {
    sp_text(32,120,width()-64,"THIS LOCAL HOUR OCCURS TWICE",false,true);
    sp_button(32,192,width()-64,88,
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  "SAVE EARLIER UTC"
#else
  "SAVE FIRST MDT"
#endif
,sv_fold_choice==0);
    sp_button(32,304,width()-64,88,
#ifdef PORTABLE_SETTINGS_NATIVE_TIME
  "SAVE LATER UTC"
#else
  "SAVE SECOND MST"
#endif
,sv_fold_choice==1);sp_footer(false);
  } else if(sv_page==SV_TIME_FORMAT) {
    sp_button(32,144,width()-64,88,"12-HOUR AM / PM",time_format_choice==PORTABLE_TIME_FORMAT_12);
    sp_button(32,256,width()-64,88,"24-HOUR",time_format_choice==PORTABLE_TIME_FORMAT_24);
    sp_text(32,height()-142,width()-64,time_format_message[0]?time_format_message:"DRAFT ONLY - USE SAVE",false,true);sp_footer(true);
#ifdef PORTABLE_SLEEP_SETTINGS
  } else if(sv_page==SV_SLEEP) {
    const char *options[]={"LIGHT SLEEP","DEEP SLEEP","LIGHT 5 MIN - DEEP"};
    for(unsigned row=0;row<settings_sleep_count();++row){unsigned mode=settings_sleep_at(row);
      const char *name=options[mode];
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
      if(mode==PORTABLE_SLEEP_DEEP)name="DEEP DESK CLOCK";
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
      sp_button(32,sp_sleep_top()+(int)row*sp_sleep_step(),width()-64,sp_sleep_step()-24,name,sleep_choice==mode);
#else
      sp_button(32,144+(int)row*112,width()-64,88,name,sleep_choice==mode);
#endif
    }
    sp_text(32,height()-142,width()-64,sleep_message,false,true);sp_footer(true);
#endif
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  } else if(sv_page==SV_FLIP || sv_page==SV_LANGUAGE) {
    bool language=sv_page==SV_LANGUAGE;char choice[64];
    if(language)snprintf(choice,sizeof(choice),"%s (%s)",portable_reader_language_name(reader_choice),portable_reader_language_code(reader_choice));
    else strcpy(choice,reader_choice?"ON":"OFF");
    int measure=pp_measure(choice,false);
    sp_text((width()-measure)/2,144,width()-64,choice,false,true);
    if(language) {
      snprintf(choice,sizeof(choice),"%u / %u",reader_choice+1u,PORTABLE_READER_LANGUAGE_COUNT);
      measure=pp_measure(choice,false);sp_text((width()-measure)/2,196,width()-64,choice,false,true);
    }
    sp_button(32,260,(width()-80)/2,88,"-",false);
    sp_button(width()/2+8,260,(width()-80)/2,88,"+",false);
    if(language)sp_text(32,388,width()-64,"CLOCK TEXT USES ENGLISH FALLBACK",false,true);
    sp_text(32,height()-142,width()-64,reader_message[0]?reader_message:"DRAFT ONLY - USE SAVE",false,true);sp_footer(true);
  } else if(sv_page==SV_DESK_FACE) {
    unsigned columns=sp_face_columns();int step=sp_face_step(),w=sp_face_width();
    for(unsigned i=0;i<PORTABLE_DESK_FACE_COUNT;++i)
      sp_button(32+(int)(i%columns)*(w+16),120+(int)(i/columns)*step,w,step-12,portable_desk_face_name(i),face_choice==i);
    sp_text(32,height()-142,width()-64,face_message[0]?face_message:"DRAFT ONLY - USE SAVE",false,true);sp_footer(true);
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  } else if(sv_page==SV_ALERT) {
    if(settings_visual_alerts()) {
      sp_text(32,144,width()-64,"VISUAL ONLY",true,true);
      sp_text(32,208,width()-64,"NO SOUND OR VIBRATION",false,true);
      sp_text(32,264,width()-64,"SAVED PREFERENCE KEPT",false,true);sp_footer(false);
      present(sp_clean);sp_clean=false;return;
    }
    const char *options[]={"VIBRATE","SOUND","BOTH"};
    for(unsigned i=0;i<3;i++)sp_button(32,144+(int)i*112,width()-64,88,options[i],alert_choice==i+1);
    sp_text(32,height()-142,width()-64,alert_message,false,true);sp_footer(true);
#endif
  } else {
    char geometry[32];snprintf(geometry,sizeof(geometry),"%d X %d",(int)width(),(int)height());
    sp_row(SP_TOP,"SETTINGS",PORTABLE_SETTINGS_VERSION,false,false);sp_row(SP_TOP+SP_ROW,"LOGICAL DISPLAY",geometry,false,false);
    sp_text(32,340,width()-64,"REPORTED DEVICE CAPABILITIES",false,true);sp_footer(false);
  }
  present(sp_clean);sp_clean=false;
}
static void sp_emit_hit(t5_app_input_t *out,int x,int y,int w,int h) {
#ifdef PORTABLE_NOVA_UI
  sv_geometry(&x,&y,&w,&h);
#endif
  out->tapped=true;out->touch_x=(int16_t)(x+w/2);out->touch_y=(int16_t)(y+h/2);
}
static bool sp_poll(t5_app_input_t *out) {
  settings_navigation_poll(out);
  if(out->buttons&T5_APP_BUTTON_BACK){sv_back_input(out);return true;}
  portable_touch_sample s;input_take(&s);
  if(!s.valid||s.cancelled){sv_gesture=false;sv_need_neutral=true;return true;}
  if(sv_need_neutral){if(!s.down||(s.began&&s.tap_eligible))sv_need_neutral=false;else return true;}
  if(s.began){sv_gesture=true;sv_start_x=sv_last_x=s.x;sv_start_y=sv_last_y=s.y;}
  if(s.down&&sv_gesture){sv_last_x=s.x;sv_last_y=s.y;}
  if(!s.released)return true;
  bool tap=s.tap_eligible&&!s.moved;
  if(sv_gesture&&s.moved&&(sv_page==SV_ROOT||sv_page==SV_FIELDS)) {
    int dy=sv_last_y-sv_start_y;
    if(abs(dy)>=44&&abs(dy)>abs(sv_last_x-sv_start_x)*2) {
      unsigned rows=sp_rows(),limit=sv_page==SV_ROOT?SETTINGS_ROW_COUNT:6;unsigned *first=sv_page==SV_ROOT?&sp_first:&sp_field_first;
      *first=dy<0&&*first+rows<limit?*first+rows:dy>0?0:*first;settings_view_redraw();
    }
  }
  sv_gesture=false;if(!tap||s.x>=width()||s.y>=height())return true;
  int x=s.x,y=s.y;
  bool editable=sv_page==SV_FIELDS||sv_page==SV_TIME_FORMAT||sv_page==SV_SLEEP||sv_page==SV_ALERT;
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  if(sv_page==SV_DESK_FACE || sv_page==SV_FLIP || sv_page==SV_LANGUAGE)editable=true;
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  if(sv_page==SV_ALERT&&settings_visual_alerts())editable=false;
#endif
  if(y>=height()-112&&y<height()-24) {
    if((editable||(sv_page==SV_ROOT&&SETTINGS_ROW_COUNT>sp_rows()))&&x>=width()/2-8&&x<width()/2+8)return true;
    if(editable&&x>=width()/2){editor_field=6;out->buttons=T5_APP_BUTTON_CONFIRM;}
    else if(sv_page==SV_ROOT&&SETTINGS_ROW_COUNT>sp_rows()&&x>=width()/2){sp_first=SP_MORE_NEXT;settings_view_redraw();}
    else sv_back_input(out);
    return true;
  }
  if(x<32||x>=width()-32)return true;
  if(sv_page==SV_ROOT){if(y>=SP_TOP&&y<SP_TOP+(int)sp_rows()*SP_ROW){out->tapped=true;out->touch_x=x;out->touch_y=y;}return true;}
  if(sv_page==SV_FIELDS&&y>=SP_TOP&&y<SP_TOP+(int)sp_rows()*SP_ROW&&sp_field_first+(unsigned)(y-SP_TOP)/SP_ROW<6){editor_field=sp_field_first+(unsigned)(y-SP_TOP)/SP_ROW;out->buttons=T5_APP_BUTTON_CONFIRM;return true;}
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  if(sv_page==SV_FLIP || sv_page==SV_LANGUAGE) {
    if(y>=260 && y<348) {
      if(x<width()/2-8)out->buttons=T5_APP_BUTTON_LEFT;
      else if(x>=width()/2+8)out->buttons=T5_APP_BUTTON_RIGHT;
    }
    return true;
  }
#endif
  if(sv_page==SV_VALUE) {
    if(y>=238&&y<350){if(x<144)out->buttons=T5_APP_BUTTON_LEFT;else if(x>=width()-144)out->buttons=T5_APP_BUTTON_RIGHT;}
    else if(y>=392&&y<480&&editor_field==3&&!settings_hour_24()){editor_set_value((settings_draft.hour+12)%24);settings_view_redraw();}
  } else if(sv_page==SV_FOLD) {
    if(y>=192&&y<280)sp_emit_hit(out,28,100,184,32);
    else if(y>=304&&y<392)sp_emit_hit(out,28,142,184,32);
  } else if(sv_page==SV_TIME_FORMAT) {
    if(y>=144&&y<232)sp_emit_hit(out,28,76,184,32);
    else if(y>=256&&y<344)sp_emit_hit(out,28,124,184,32);
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
  } else if(sv_page==SV_FLIP || sv_page==SV_LANGUAGE) {
    bool language=sv_page==SV_LANGUAGE;char choice[64];
    if(language)snprintf(choice,sizeof(choice),"%s (%s)",portable_reader_language_name(reader_choice),portable_reader_language_code(reader_choice));
    else strcpy(choice,reader_choice?"ON":"OFF");
    int measure=pp_measure(choice,false);
    sp_text((width()-measure)/2,144,width()-64,choice,false,true);
    if(language) {
      snprintf(choice,sizeof(choice),"%u / %u",reader_choice+1u,PORTABLE_READER_LANGUAGE_COUNT);
      measure=pp_measure(choice,false);sp_text((width()-measure)/2,196,width()-64,choice,false,true);
    }
    sp_button(32,260,(width()-80)/2,88,"-",false);
    sp_button(width()/2+8,260,(width()-80)/2,88,"+",false);
    if(language)sp_text(32,388,width()-64,"CLOCK TEXT USES ENGLISH FALLBACK",false,true);
    sp_text(32,height()-142,width()-64,reader_message[0]?reader_message:"DRAFT ONLY - USE SAVE",false,true);sp_footer(true);
  } else if(sv_page==SV_DESK_FACE) {
    unsigned columns=sp_face_columns();int step=sp_face_step(),w=sp_face_width();
    for(unsigned i=0;i<PORTABLE_DESK_FACE_COUNT;++i){int left=32+(int)(i%columns)*(w+16),top=120+(int)(i/columns)*step;
      if(x>=left&&x<left+w&&y>=top&&y<top+step-12)
        sp_emit_hit(out,20+(int)(i%2)*104,62+(int)(i/2)*36,96,30);
    }
#endif
  } else if(sv_page==SV_SLEEP||sv_page==SV_ALERT) {
    unsigned count=3;
#ifdef PORTABLE_SLEEP_SETTINGS
    if(sv_page==SV_SLEEP)count=settings_sleep_count();
#endif
    int top=144,step=112,h=88;
#ifdef PORTABLE_SETTINGS_X4_DESK_CLOCK
    if(sv_page==SV_SLEEP){top=sp_sleep_top();step=sp_sleep_step();h=step-24;}
#endif
    for(unsigned i=0;i<count;i++)if(y>=top+(int)i*step&&y<top+(int)i*step+h)sp_emit_hit(out,28,66+(int)i*38,184,30);
  }
  return true;
}
