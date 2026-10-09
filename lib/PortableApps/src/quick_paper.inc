#include <stdio.h>
#ifdef ALARM_SERVICE_TAGGED_V2
#define quick_alarm_output_modes() alarm_output_modes()
#else
#define quick_alarm_output_modes() alarm_service_output_modes(alarm_sleep_api())
#endif
/* NOVA-7 paper sheet: large two-column tiles, static 1-bit presentation.
 * Uses existing licensed quick-control masks and shared paper text. */
static int qp_x(int x){return x*width()/480;}
#ifdef PORTABLE_PAPER_TRANSITIONS
static int qp_offset;
static bool quick_copy_background(void);
static int qp_y(int y){return (y+qp_offset)*height()/800;}
#else
static int qp_y(int y){return y*height()/800;}
#endif
static void qp_text(int x,int y,int w,const char *s,bool heading,bool black) {
 pp_text(qp_x(x),qp_y(y),qp_x(w),s,heading?2:1,heading,black);
}
static void qp_rect(int x,int y,int w,int h,bool black){fill(qp_x(x),qp_y(y),qp_x(w),h*height()/800,black?0:0xffff);}
static void qp_icon(unsigned icon,int x,int y,bool black){pqa_paper_icon(icon,qp_x(x),qp_y(y),qp_x(34),black,np_pixel);}
static void quick_paper_capabilities(void) {
 if(!quick.ui.paper)return;
 quick.ui.torch_valid=!!(info.flags&RISC_DISPLAY_INFO_BRIGHTNESS) && display->set_brightness;
 quick.ui.brightness_valid&=quick.ui.torch_valid;
 quick.ui.volume_valid&=!!(quick_alarm_output_modes()&ALARM_MODE_SOUND);
#ifndef PORTABLE_QUICK_RADIOS
 /* An ungranted radio is unavailable, never implicitly enabled or launched. */
 quick.ui.radio_controls=true;quick.ui.radios_valid=false;
#endif
}
static void qp_slider(int y,const char *label,unsigned icon,unsigned value,bool valid) {
 char percent[8];if(valid)snprintf(percent,sizeof(percent),"%u%%",value);else strcpy(percent,"--%");
 qp_text(32,y-30,400,label,false,true);qp_icon(icon,35,y+5,true);
 for(unsigned i=0;i<10;i++){qp_rect(88+(int)i*28,y,24,44,true);if(!valid||i*10>=value)qp_rect(91+(int)i*28,y+3,18,38,false);}
 qp_text(382,y+8,70,percent,false,true);
}
static bool quick_paper_changed(const pqa_state *a,const pqa_state *b) {
 return
#ifdef PORTABLE_PAPER_TRANSITIONS
  a->position_q8!=b->position_q8 || a->target_q8!=b->target_q8 ||
#endif
  a->brightness!=b->brightness || a->volume!=b->volume || a->torch!=b->torch ||
  a->brightness_valid!=b->brightness_valid || a->volume_valid!=b->volume_valid ||
  a->dnd_valid!=b->dnd_valid || a->dnd_enabled!=b->dnd_enabled || a->radios_valid!=b->radios_valid ||
  a->wifi_enabled!=b->wifi_enabled || a->bluetooth_enabled!=b->bluetooth_enabled || a->airplane!=b->airplane || a->error_flags!=b->error_flags;
}
static void quick_paper_render(const char *time,bool battery_valid,unsigned battery) {
#ifdef PORTABLE_PAPER_TRANSITIONS
 qp_offset=quick.ui.torch?0:(quick.ui.position_q8*800/PQA_OPEN_Q8)-800;
 if(!quick_copy_background()){failed=true;return;}
 fill(0,qp_y(0),width(),height(),0xffff);
#else
 pp_begin();
#endif
 if(quick.ui.torch){qp_text(74,620,360,"TAP TO TURN OFF",true,true);return;}
 qp_text(32,24,300,"QUICK ACTIONS",true,true);qp_text(342,30,112,time,true,true);
 qp_rect(32,78,416,4,true);
 qp_slider(126,"FRONTLIGHT",6,quick.ui.brightness,quick.ui.brightness_valid);
 qp_slider(220,(quick_alarm_output_modes()&ALARM_MODE_SOUND)?"NOTIFICATIONS":"SOUND UNAVAILABLE",7,quick.ui.volume,quick.ui.volume_valid);qp_rect(32,284,416,3,true);
 static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI","BLUETOOTH","TORCH"};
 for(unsigned i=0;i<6;i++) {
  int x=32+(int)(i%2)*212,y=302+(int)(i/2)*104;
  bool enabled=i==0?quick.ui.volume_valid:i==1?quick.ui.dnd_valid:i==5?quick.ui.torch_valid:quick.ui.radios_valid;
  bool on=enabled&&(i==0?!quick.ui.volume:i==1?quick.ui.dnd_enabled:i==2?quick.ui.airplane:i==3?quick.ui.wifi_enabled:i==4?quick.ui.bluetooth_enabled:quick.ui.torch);
  qp_rect(x,y,204,92,true);if(!on)qp_rect(x+4,y+4,196,84,false);
  qp_icon(i,x+20,y+28,!on);qp_text(x+70,y+(enabled?32:19),126,labels[i],false,!on);
  if(!enabled)qp_text(x+70,y+46,126,"UNAVAILABLE",false,true);
 }
 qp_rect(32,640,416,3,true);qp_text(74,665,360,"SWIPE UP TO CLOSE",false,true);
 if(quick.ui.error_flags)qp_text(52,708,400,"CHANGE UNCONFIRMED",false,true);
 qp_text(32,756,70,"BATT",false,true);
 for(unsigned i=0;i<10;i++){qp_rect(104+(int)i*23,752,19,16,true);if(!battery_valid||i*10>=battery)qp_rect(107+(int)i*23,755,13,10,false);}
 char percent[8];if(battery_valid)snprintf(percent,sizeof(percent),"%u%%",battery);else strcpy(percent,"--%");
 qp_text(384,750,70,percent,false,true);
}
