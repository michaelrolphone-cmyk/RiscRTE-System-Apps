/* Capability-selected paper view; all operation authority stays in controller. */
#include "PaperPresentation.h"
static const paper_presentation *up;
static unsigned up_first;
static void up_text(int x,int y,int w,const char *s,bool heading,bool black) {
 up->text(x,y,w,s,heading?2:1,heading,black);
}
static const char *up_action(void) {
 if(restart_pending)return "RESTART";
 if(cleanup_pending)return "RETRY";
 if(connecting||(ust.state>=SOFTWARE_UPDATE_CATALOG&&ust.state!=SOFTWARE_UPDATE_LIST&&ust.state<=SOFTWARE_UPDATE_READY))return "CANCEL";
 if(confirming)return confirm_choice?"INSTALL":"CANCEL";
 return selected?"OPEN":"CHECK";
}
static void up_draw(void) {
 int w=ua->screen_width(),h=ua->screen_height();up->begin();
 up_text(32,28,w-64,PORTABLE_UPDATE_FIRMWARE?"FIRMWARE UPDATE":"APP STORE",true,true);
 ua->fill_rect(32,88,w-64,3,true);
 up_first=uv.selected/4*4;
 for(unsigned i=up_first;i<uv.count&&i<up_first+4;i++) {
  int y=108+(int)(i-up_first)*96;bool chosen=i==uv.selected;
  if(chosen)ua->fill_rect(32,y,w-64,96,true);
  up_text(48,y+12,w-128,uv.rows[i],false,!chosen);
  up_text(48,y+46,w-128,uv.values[i],false,!chosen);
  if(ua->draw_icon)ua->draw_icon(w-72,y+34,"solid:f054",24,!chosen);
  ua->fill_rect(32,y+93,w-64,3,true);
 }
 up_text(32,512,w-64,uv.detail?uv.detail:"",false,true);
 /* Wrap status without hiding cleanup or failure behind a clipped line. */
 const char *s=uv.message?uv.message:"";unsigned pos=0;
 for(unsigned row=0;row<4&&s[pos];row++) {char line[97]={0};unsigned n=0;
  while(n<96&&s[pos+n]){line[n]=s[pos+n];line[n+1]=0;if(up->measure(line,false)>w-64){line[n]=0;break;}n++;}
  if(!n)break;up_text(32,548+(int)row*28,w-64,line,false,true);pos+=n;
 }
 const char *labels[]={"BACK","PREV",up_action(),"NEXT"};
 const char *icons[]={"solid:f053","solid:f077",NULL,"solid:f078"};int bw=(w-88)/4;
 for(unsigned i=0;i<4;i++){int x=32+(int)i*(bw+8);ua->fill_rect(x,h-112,bw,88,true);ua->fill_rect(x+3,h-109,bw-6,82,false);
  if(icons[i]&&ua->draw_icon)ua->draw_icon(x+(bw-24)/2,h-98,icons[i],24,true);
  up_text(x+5,h-62,bw-10,labels[i],false,true);
 }
 ua->present(true);
}
#ifndef PORTABLE_UPDATE_TOUCH_SCROLL
static void up_input(t5_app_input_t *input) {
 springboard_contact c={0};up->contact(&c);
 input->tapped=c.valid&&!c.cancelled&&c.released&&c.tap_eligible;
 if(input->tapped){input->touch_x=c.x;input->touch_y=c.y;
  int w=ua->screen_width(),h=ua->screen_height(),bw=(w-88)/4,x=c.x-32;
  if(x>=0&&c.y>=h-112&&c.y<h-24){unsigned at=(unsigned)x/(unsigned)(bw+8);
   if(at<4&&x%(bw+8)<bw){static const unsigned keys[]={T5_APP_BUTTON_BACK,T5_APP_BUTTON_UP,T5_APP_BUTTON_CONFIRM,T5_APP_BUTTON_DOWN};input->buttons|=keys[at];input->tapped=false;}}
 }
}

#endif
