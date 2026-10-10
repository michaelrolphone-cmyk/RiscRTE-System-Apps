/* Foreground Home only. The retained landscape Points scene keeps its own
 * geometry and never draws this dock or acquires another capability. */
#include "../lib/PortableApps/home_fonts/reference.inc"

static const char *home_launch_target(unsigned target) {
 switch(target) {
 case HOME_FILES:return "file_browser.elf";
 case HOME_POINTS:case HOME_DOCK_POINTS:return PAPER_POINTS_APP;
 case HOME_CONTEXTS:return "contexts.elf";
 case HOME_SETTINGS:return "settings.elf";
 default:return PAPER_CLOCK_LAUNCHER;
 }
}
static void home_reference_icon(unsigned index,int x,int y) {
 const uint8_t *bits=home_ref_icons[index];
 home_tick_draw draw={{x,y,80,80,bits},home_ink_snapshot(),app->screen_width(),app->screen_height()};
 if(home_can_defer()&&portable_raster_record_draw(home_tick_rows,&draw,sizeof(draw),yscale(y),yscale(y+80)+1))return;
 for(unsigned row=0;row<80;row++)for(unsigned col=0;col<80;) {
  unsigned bit=row*80+col;
  if(!(bits[bit/8]&(128u>>(bit%8)))){col++;continue;}
  unsigned begin=col++;
  while(col<80){bit=row*80+col;if(!(bits[bit/8]&(128u>>(bit%8))))break;col++;}
  home_fill(xscale(x+(int)begin),yscale(y+(int)row),xscale((int)(col-begin)),yscale(1),true);
 }
}
static void home_reference_dock(void) {
 static const char *const names[]={"FILES","POINTS","CONTEXTS","SETTINGS"};
 home_fill(xscale(32),yscale(625),xscale(416),yscale(3),true);
 for(unsigned i=0;i<4;i++) {
  int x=84+(int)i*104;
  home_reference_icon(i,x-40,650);
  home_type(&HOME_REF_DOCK,x,756,names[i],1,2,100,0);
 }
}
/* Complete dynamic labels wrap at measured glyph boundaries, including long
 * unbroken names. Never replace them with an ellipsis or a numeric type ID. */
static unsigned home_reference_lines(const home_font *font,const char *label,int width,char lines[3][32]) {
 memset(lines,0,96);unsigned offset=0,line=0;
 while(label[offset]&&line<3) {
  unsigned count=0,space=0;
  while(label[offset+count]&&count<31) {
   lines[line][count]=label[offset+count];lines[line][count+1]=0;
   if(home_text_width(font,lines[line],0)>width*64){lines[line][count]=0;break;}
   if(label[offset+count]==' ')space=count;
   count++;
  }
  if(!count)return 0;
  if(label[offset+count]&&space){count=space;lines[line][count]=0;}
  offset+=count;while(label[offset]==' ')offset++;
  line++;
 }
 return label[offset]?0:line;
}
static const char *home_reference_label(const home_point_event *event) {
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
 return event->label;
#else
 return paper_points_label(event);
#endif
}
static void home_reference_title(const char *label) {
 if(home_text_width(&HOME_REF_TITLE,label,0)<=416*64) {
  home_type(&HOME_REF_TITLE,32,366,label,0,0,416,0);return;
 }
 char lines[3][32];unsigned count=home_reference_lines(&HOME_R700_30,label,416,lines);
 if(!count)return;
 for(unsigned i=0;i<count;i++)home_type(&HOME_R700_30,32,count==1?366:342+(int)i*32,lines[i],0,0,416,0);
}
static void home_reference_follow_label(const char *label,int x) {
 char lines[3][32];const home_font *font=&HOME_REF_FOLLOW;
 unsigned count=home_reference_lines(font,label,198,lines);
 if(!count){font=&HOME_REF_DOCK;count=home_reference_lines(font,label,198,lines);}
 for(unsigned i=0;i<count;i++)home_type(font,x,568+(int)i*19,lines[i],count==1?2:0,0,198,0);
}
static void home_reference_points(const home_point_state *state,unsigned time_format,const char *notice) {
 char value[48];
 home_fill(xscale(32),yscale(262),xscale(416),yscale(42),true);
 home_type_ink(&HOME_REF_NEXT,48,291,"NEXT POINT",6,0,0,0,false);
 if(state->status!=NOVA_POINTS_READY||!state->next_count) {
  home_reference_title(state->status==NOVA_POINTS_EMPTY?"NONE SET":state->status==NOVA_POINTS_PENDING?"LOADING POINTS":"POINTS UNAVAILABLE");
  home_type(&HOME_R700_26,32,408,state->status==NOVA_POINTS_EMPTY?"TAP TO ADD ONE":state->status==NOVA_POINTS_PENDING?"PLEASE WAIT":"OPEN POINTS TO CHECK",2,0,416,0);
 } else {
  const home_point_event *next=&state->next[0];
  home_time(value,sizeof(value),next,false,time_format);
  home_type_ink(&HOME_REF_EVENT,432,293,value,0,1,200,0,false);
  home_reference_title(home_reference_label(next));
  home_duration(value,sizeof(value),next->at_rtc-state->now_rtc);
  home_type(&HOME_R700_30,32,408,value,2,0,416,0);
  home_round(30,422,420,28,14,true);home_round(34,426,412,20,10,false);
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
  unsigned progress=paper_catalog_progress(state,416);
#else
  unsigned progress=paper_points_progress(state,416);
#endif
  if(state->previous_valid&&progress<24)progress=24;
  if(progress)home_round(30,422,(int)progress+4,28,14,true);
  home_fill(xscale(32),yscale(477),xscale(416),yscale(3),true);
  home_type(&HOME_REF_THEN,32,504,"THEN",8,0,0,0);
  for(unsigned i=1;i<state->next_count&&i<3;i++) {
   const home_point_event *event=&state->next[i];int x=i==1?32:250;
   home_time(value,sizeof(value),event,false,time_format);
   home_type(&HOME_REF_EVENT,x,540,value,0,0,98,0);
   home_duration(value,sizeof(value),event->at_rtc-state->now_rtc);
   home_type(&HOME_REF_DURATION,x+198,536,value,0,1,90,0);
   home_reference_follow_label(home_reference_label(event),x);
  }
 }
 if(notice&&*notice)home_type(&HOME_REF_DOCK,32,232,notice,0,0,416,0);
 home_reference_dock();
}
static void home_reference_header(const twatch_rtc_time_v1 *time,bool known) {
 char value[64];
 static const char *const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
 static const char *const months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
 if(known)snprintf(value,sizeof(value),"%s %02u %s",days[time->weekday%7],time->day,months[time->month-1]);
 else strcpy(value,"TIME UNAVAILABLE");
 home_type(&HOME_R700_26,32,44,value,3,0,200,0);
 if(battery_status.valid)snprintf(value,sizeof(value),"%u%%%s",battery_status.percent,battery_status.charging?" +":"");else strcpy(value,"--%");
 home_type(&HOME_REF_BATTERY,331,43,value,0,1,94,0);
 unsigned bars=battery_status.valid?(battery_status.percent+5u)/10u:0;
 for(unsigned i=0;i<10;i++) {
  int x=340+(int)i*11;home_round(x,27,10,16,3,true);
  if(i>=bars)home_round(x+3,30,4,10,1,false);
 }
 home_fill(xscale(32),yscale(59),xscale(416),yscale(3),true);
 if(known)snprintf(value,sizeof(value),"%u:%02u",format==PORTABLE_TIME_FORMAT_24?time->hour:time->hour%12?time->hour%12:12,time->minute);
 else strcpy(value,"--:--");
 home_type(&HOME_REF_TIME,240,190,value,0,2,0,416);
 if(known&&format==PORTABLE_TIME_FORMAT_12)home_type(&HOME_REF_DOCK,448,232,time->hour<12?"AM":"PM",0,1,0,0);
}
