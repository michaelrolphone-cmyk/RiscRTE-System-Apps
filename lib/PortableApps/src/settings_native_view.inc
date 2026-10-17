#ifdef PORTABLE_SETTINGS_NATIVE_UI
static void sn_text(unsigned face,int x,int y,int available,const char *text,uint32_t color) {
  if(!text||available<=0)return;
  char label[100];size_t n=strlen(text);if(n>96)n=96;
  memcpy(label,text,n);label[n]=0;
  if(portable_nova_measure(face,label)>available) {
    while(n&&portable_nova_measure(face,label)>available- portable_nova_measure(face,"..."))label[--n]=0;
    if(available>=portable_nova_measure(face,"...")){memcpy(label+n,"...",4);}
    else label[0]=0;
  }
  portable_nova_native_text(face,x,y,available,label,color);
}
static void sn_center(unsigned face,int y,const char *text,uint32_t color) {
  sn_layout g=sn_geometry();int n=portable_nova_measure(face,text),available=g.w-2*g.margin;
  sn_text(face,n<available?(g.w-n)/2:g.margin,y,available,text,color);
}
static void sn_button(sn_rect r,const char *caption,bool selected) {
  if(r.w<=0||r.h<=0)return;
  portable_nova_native_round(r.x,r.y,r.w,r.h,5,selected?SV_CYAN:SV_DIM);
  portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,4,selected?0x062d35u:0);
  const char *glyph=NULL;
  if(!strcmp(caption,"+"))glyph="solid:f067";
  else if(!strcmp(caption,"-"))glyph="solid:f068";
  if(glyph){(void)portable_nova_native_icon(r.x+(r.w-16)/2,r.y+(r.h-16)/2,16,glyph,SV_CYAN);return;}
  int n=portable_nova_measure(2,caption),available=r.w-6;
  sn_text(2,n<available?r.x+(r.w-n)/2:r.x+3,r.y+(r.h-12)/2-2,available,caption,SV_CYAN);
}
static void sn_control_button(int x,int y,int w,int h,const char *s,bool selected) {
  sn_button(sn_control(x,y,w,h),s,selected);
}
static void sn_notice(const char *s) {
  sn_layout g=sn_geometry();sn_text(2,g.margin,g.message,g.w-2*g.margin,s,SV_CAP);
}
static void sn_list_row(sn_rect r,const char *title,const char *value,bool selected,bool action) {
  if(r.w<=0)return;
  portable_nova_native_fill(r.x,r.y,2,r.h,selected?SV_CYAN:SV_LINE);
  portable_nova_native_fill(r.x+2,r.y+r.h-1,r.w-2,1,SV_LINE);
  sn_text(2,r.x+10,r.y+2,r.w-26,title,0xffffff);
  sn_text(2,r.x+10,r.y+(r.h<40?16:23),r.w-26,value,SV_CYAN);
  if(action)(void)portable_nova_native_icon(r.x+r.w-17,r.y+(r.h-12)/2,12,"solid:f054",SV_DIM);
}
static void sn_footer_draw(bool save) {
  if(save){sn_button(sn_footer(0,2),"CANCEL",false);sn_button(sn_footer(1,2),"SAVE",true);}
  else sn_button(sn_footer(0,1),"< BACK",false);
}
static void sn_redraw(void) {
  sn_layout g=sn_geometry();sv_begin();
  if(sn_choices()) {
    unsigned chosen=sv_page==SV_FOLD?sv_fold_choice:sv_page==SV_TIME_FORMAT?time_format_choice:0;
#ifdef PORTABLE_SLEEP_SETTINGS
    if(sv_page==SV_SLEEP)for(unsigned i=0;i<settings_sleep_count();i++)if(settings_sleep_at(i)==sleep_choice)chosen=i;
#endif
#ifdef PORTABLE_ALARM_SETTINGS
    if(sv_page==SV_ALERT)chosen=alert_choice?alert_choice-1:0;
#endif
    if(sn_last_choice!=chosen){sn_choice_scroll=sn_reveal(chosen,sn_choice_scroll);sn_last_choice=chosen;}
  }
  const char *title=sv_page==SV_ROOT?"SETTINGS":sv_page==SV_FIELDS?"TIME AND DATE":
    sv_page==SV_VALUE?editor_names[editor_field]:sv_page==SV_TIME_FORMAT?"TIME FORMAT":
    sv_page==SV_FOLD?"REPEATED LOCAL TIME":sv_page==SV_SLEEP?"CLOCK SLEEP MODE":
    sv_page==SV_ALERT?"ALARM ALERTS":"ABOUT";
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
  if(sv_page==SV_TIMER)title=timer_deep?"DEEP SLEEP TIMER":"SLEEP TIMER";
#endif
  sv_heading(title);
  bool save=sv_page==SV_FIELDS||sv_page==SV_TIME_FORMAT||sv_page==SV_SLEEP||sv_page==SV_ALERT;
  const char *notice="";
  if(sv_page==SV_ROOT) {
    for(unsigned i=0;i<SETTINGS_ROW_COUNT;i++){
      sn_rect r=sn_row(i,sv_scroll[0]);if(r.w<=0)continue;
      t5_app_setting_t row;if(!settings_get(0,i,&row))continue;
      if(failed)return;
      sn_list_row(r,row.label,row.value,sv_selected==i+1,row.type==T5_APP_SETTING_ACTION);
    }
    notice=settings_message[0]?settings_message:sn_limit(SETTINGS_ROW_COUNT)?"Swipe for more":"";
  } else if(sv_page==SV_FIELDS) {
    for(unsigned i=0;i<6;i++){
      char value[24];editor_value_text(i,value,sizeof(value));
      sn_list_row(sn_row(i,sv_scroll[1]),editor_names[i],value,editor_field==i,true);
    }
    notice=editor_message[0]?editor_message:"Draft only - Save";
  } else if(sv_page==SV_VALUE) {
    char value[24];unsigned v=editor_value(editor_field),shown=v;
    if(editor_field==3&&!settings_hour_24())shown=v%12?v%12:12;
    snprintf(value,sizeof(value),"%u",shown);
    int side=g.w<200?32:44,available=g.w-2*g.margin-2*side-8;
    unsigned face=g.h<160?2:portable_nova_measure(4,value)<=available?4:1;
    sn_center(face,g.top+(g.h<200?4:3),value,SV_CYAN);
    sn_control_button(24,66,36,36,"-",false);sn_control_button(180,66,36,36,"+",false);
    unsigned lo,hi;editor_bounds(editor_field,&lo,&hi);
    unsigned filled=hi>lo?(v-lo)*10/(hi-lo):0;sn_rect slider=sn_slider_rect();
    for(unsigned i=0;i<10;i++){
      int x=slider.x+(int)i*slider.w/10,end=slider.x+(int)(i+1)*slider.w/10;
      portable_nova_native_fill(x,slider.y,end-x-2,slider.h,i<filled?SV_CYAN:0x032027u);
    }
    if(editor_field==3&&!settings_hour_24())sn_control_button(76,140,88,44,v<12?"AM":"PM",true);
    else {snprintf(value,sizeof(value),"%u - %u",lo,hi);sn_center(2,g.h<160?g.top+20:g.bottom-14,value,SV_CAP);}
    notice="Draft only - Save";
  } else if(sv_page==SV_FOLD) {
    sn_control_button(28,100,184,32,"SAVE EARLIER UTC",sv_fold_choice==0);
    sn_control_button(28,142,184,32,"SAVE LATER UTC",sv_fold_choice==1);
    notice=sn_limit(2)?"Swipe for more":"Choose repeated hour";
  } else if(sv_page==SV_TIME_FORMAT) {
    sn_control_button(28,76,184,32,"12-HOUR AM / PM",time_format_choice==PORTABLE_TIME_FORMAT_12);
    sn_control_button(28,124,184,32,"24-HOUR",time_format_choice==PORTABLE_TIME_FORMAT_24);
    notice=time_format_message[0]?time_format_message:sn_limit(2)?"Swipe for more":"Draft only - Save";
#ifdef PORTABLE_SLEEP_SETTINGS
  } else if(sv_page==SV_SLEEP) {
    const char *names[]={"LIGHT SLEEP","DEEP SLEEP","LIGHT THEN DEEP"};
    for(unsigned i=0;i<settings_sleep_count();i++){
      unsigned m=settings_sleep_at(i);sn_control_button(28,66+(int)i*38,184,30,names[m],sleep_choice==m);
    }
    notice=sleep_message[0]?sleep_message:sn_limit(settings_sleep_count())?"Swipe for more":"Draft only - Save";
#endif
#ifdef PORTABLE_ALARM_SETTINGS
  } else if(sv_page==SV_ALERT) {
    if(settings_visual_alerts()){sn_center(2,g.top,"VISUAL ONLY",SV_CYAN);notice="Saved preference kept";save=false;}
    else {
      sn_control_button(28,66,184,30,"VIBRATE",alert_choice==PORTABLE_ALERT_VIBRATE);
      sn_control_button(28,104,184,30,"SOUND",alert_choice==PORTABLE_ALERT_SOUND);
      sn_control_button(28,142,184,30,"BOTH",alert_choice==PORTABLE_ALERT_BOTH);
      notice=alert_message[0]?alert_message:sn_limit(3)?"Swipe for more":"Alarms and Countdown";
    }
#endif
#ifdef PORTABLE_SETTINGS_SLEEP_TIMER
  } else if(sv_page==SV_TIMER) {
    char value[24];snprintf(value,sizeof(value),"%lu SEC",(unsigned long)(timer_choice/1000u));
    sn_center(2,g.top+4,value,SV_CYAN);
    sn_control_button(20,88,64,44,"-",false);sn_control_button(156,88,64,44,"+",false);
    notice=timer_message[0]?timer_message:"Draft only - Save";save=true;
#endif
  } else {
    char dimensions[24];snprintf(dimensions,sizeof(dimensions),"%d x %d",g.w,g.h);
    sn_text(2,g.margin,g.top,g.w-2*g.margin,PORTABLE_SETTINGS_VERSION,SV_TEXT);
    sn_text(2,g.margin,g.top+20,g.w-2*g.margin,dimensions,SV_CYAN);
    notice="Reported device data";
  }
  sn_notice(notice);sn_footer_draw(save);present(false);
}
#endif
