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
static void qp_rect(int x,int y,int w,int h,bool black){fill(qp_x(x),qp_y(y),qp_x(w),h*height()/800,black?0:0xffff);}
#ifndef PORTABLE_RESIDENT_SHELL_HOST
static void qp_text(int x,int y,int w,const char *s,bool heading,bool black) {
 pp_text(qp_x(x),qp_y(y),qp_x(w),s,heading?2:1,heading,black);
}
#define qp_tile qp_rect
static void qp_icon(unsigned icon,int x,int y,bool black){
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(raster_record(RS_QP_ICON,(int32_t[8]){(int32_t)icon,x,y,black},NULL))return;
#endif
 pqa_paper_icon(icon,qp_x(x),qp_y(y),qp_x(34),black,np_pixel);}
#endif
static void quick_paper_capabilities(void) {
 if(!quick.ui.paper)return;
 quick.ui.torch_valid=!!(info.flags&RISC_DISPLAY_INFO_BRIGHTNESS) && display->set_brightness;
 quick.ui.brightness_valid&=quick.ui.torch_valid;
 quick.ui.volume_valid&=!!(quick_alarm_output_modes()&ALARM_MODE_SOUND);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 quick.ui.audio_controls=!!(quick_alarm_output_modes()&ALARM_MODE_SOUND);
 quick.ui.clean_refresh_valid=!!(info.flags&RISC_DISPLAY_INFO_CLEAN_PRESENT);
#endif
#ifndef PORTABLE_QUICK_RADIOS
 /* An ungranted radio is unavailable, never implicitly enabled or launched. */
 quick.ui.radio_controls=true;quick.ui.radios_valid=false;
#endif
}
#ifndef PORTABLE_RESIDENT_SHELL_HOST
static void qp_slider(int y,const char *label,unsigned icon,unsigned value,bool valid) {
 char percent[8];if(valid)snprintf(percent,sizeof(percent),"%u%%",value);else strcpy(percent,"--%");
 int label_width=400;
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(y==126)label_width=280;
#endif
 qp_text(32,y-30,label_width,label,false,true);qp_icon(icon,35,y+5,true);
 for(unsigned i=0;i<10;i++){qp_rect(88+(int)i*28,y,24,44,true);if(!valid||i*10>=value)qp_rect(91+(int)i*28,y+3,18,38,false);}
 qp_text(382,y+8,70,percent,false,true);
}
#endif
static bool quick_paper_changed(const pqa_state *a,const pqa_state *b) {
 return
#ifdef PORTABLE_FRONTLIGHT_TONE
  a->tone!=b->tone || a->applied_tone!=b->applied_tone || a->tone_controls!=b->tone_controls ||
  a->tone_valid!=b->tone_valid || a->applied_tone_valid!=b->applied_tone_valid ||
#endif
#ifdef PORTABLE_PAPER_TRANSITIONS
  a->position_q8!=b->position_q8 || a->target_q8!=b->target_q8 ||
  a->applied_brightness!=b->applied_brightness || a->applied_brightness_valid!=b->applied_brightness_valid ||
#endif
  a->brightness!=b->brightness || a->volume!=b->volume || a->torch!=b->torch ||
  a->brightness_valid!=b->brightness_valid || a->volume_valid!=b->volume_valid ||
  a->dnd_valid!=b->dnd_valid || a->dnd_enabled!=b->dnd_enabled || a->radios_valid!=b->radios_valid ||
  a->wifi_enabled!=b->wifi_enabled || a->bluetooth_enabled!=b->bluetooth_enabled || a->airplane!=b->airplane || a->error_flags!=b->error_flags;
}
#ifdef PORTABLE_RESIDENT_SHELL_HOST
#include "quick_reference.inc"
#else
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
#ifdef PORTABLE_PAPER_TRANSITIONS
 qp_slider(126,"BACKLIGHT",6,quick.ui.applied_brightness,quick.ui.brightness_valid&&quick.ui.applied_brightness_valid);
 bool light_known=quick.ui.brightness_valid&&quick.ui.applied_brightness_valid;
 bool light_on=light_known&&quick.ui.applied_brightness;
 qp_rect(326,84,122,36,true);if(!light_on)qp_rect(329,87,116,30,false);
 qp_text(346,92,90,light_known?(light_on?"ON":"OFF"):"--",false,!light_on);
#else
 qp_slider(126,"FRONTLIGHT",6,quick.ui.brightness,quick.ui.brightness_valid);
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 if(quick.ui.audio_controls)
#endif
 qp_slider(220,(quick_alarm_output_modes()&ALARM_MODE_SOUND)?"NOTIFICATIONS":"SOUND UNAVAILABLE",7,quick.ui.volume,quick.ui.volume_valid);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 qp_rect(32,quick.ui.audio_controls?284:188,416,3,true);
#else
 qp_rect(32,284,416,3,true);
#endif
 static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI","BLUETOOTH","TORCH"};
 for(unsigned i=0;i<6;i++) {
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  int x,y;if(!pqa_paper_tile(&quick.ui,i,&x,&y))continue;
#else
  int x=32+(int)(i%2)*212,y=302+(int)(i/2)*104;
#endif
  bool enabled=i==0?quick.ui.volume_valid:i==1?quick.ui.dnd_valid:i==5?quick.ui.torch_valid:quick.ui.radios_valid;
  bool on=enabled&&(i==0?!quick.ui.volume:i==1?quick.ui.dnd_enabled:i==2?quick.ui.airplane:i==3?quick.ui.wifi_enabled:i==4?quick.ui.bluetooth_enabled:quick.ui.torch);
  qp_tile(x,y,204,92,true);if(!on)qp_tile(x+4,y+4,196,84,false);
  qp_icon(i,x+20,y+28,!on);qp_text(x+70,y+(enabled?32:19),126,labels[i],false,!on);
  if(!enabled)qp_text(x+70,y+46,126,"UNAVAILABLE",false,true);
 }
#ifdef PORTABLE_QUICK_USB_TRANSFER
 qp_tile(32,612,416,64,true);qp_tile(36,616,408,56,false);
 qp_text(74,632,360,"USB SD TRANSFER",false,true);
 qp_text(74,695,360,"SWIPE UP TO CLOSE",false,true);
 if(quick.ui.error_flags)qp_text(52,726,400,"CHANGE UNCONFIRMED",false,true);
#else
 qp_rect(32,640,416,3,true);qp_text(74,665,360,"SWIPE UP TO CLOSE",false,true);
 if(quick.ui.error_flags)qp_text(52,708,400,"CHANGE UNCONFIRMED",false,true);
#endif
 qp_text(32,756,70,"BATT",false,true);
 for(unsigned i=0;i<10;i++){qp_rect(104+(int)i*23,752,19,16,true);if(!battery_valid||i*10>=battery)qp_rect(107+(int)i*23,755,13,10,false);}
 char percent[8];if(battery_valid)snprintf(percent,sizeof(percent),"%u%%",battery);else strcpy(percent,"--%");
 qp_text(384,750,70,percent,false,true);
}

#endif
