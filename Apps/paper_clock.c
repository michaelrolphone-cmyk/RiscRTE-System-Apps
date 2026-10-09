/* Main clock for retaining monochrome displays. Shared Runtime capability
 * client only. Watch's clock source/UI remains in its existing deployment. */
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableAppSleep.h"
#include "PaperPresentation.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "PaperBattery.h"
#ifndef PAPER_CLOCK_LAUNCHER
#define PAPER_CLOCK_LAUNCHER "springboard.elf"
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *rt;
static const paper_presentation *paper;
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
static unsigned format;
static paper_battery battery_status;
#include "paper_sparse_clock.inc"
#else
static risc_runtime_capability_v1 rtc_grant,battery_grant;
static const twatch_rtc_api_v1 *rtc;
static const risc_battery_gauge_api_v1 *battery;
static unsigned format;
static paper_battery battery_status;
static void retain(void){rt->diagnostic("PAPER_CLOCK cleanup-unconfirmed; invocation retained");for(;;)rt->yield_ms(50);}
static void close_clock(void){
 if(battery_grant.api&&!rt->release(&battery_grant))retain();
 battery=NULL;memset(&battery_grant,0,sizeof(battery_grant));
 if(rtc_grant.api&&!rt->release(&rtc_grant))retain();
 rtc=NULL;memset(&rtc_grant,0,sizeof(rtc_grant));
}
static bool read_clock(twatch_rtc_time_v1*out){twatch_rtc_time_v1 raw;return rtc&&rtc->read(rtc->context,&raw)&&portable_time_forward(&raw,out);}
static void open_clock(void){
 rtc_grant=(risc_runtime_capability_v1){.struct_size=sizeof(rtc_grant)};
 if(rt->acquire("rtc.clock",2,0,&rtc_grant)){const twatch_rtc_api_v1*v=rtc_grant.api;if(v&&v->api_version==2&&v->struct_size>=sizeof(*v)&&v->read)rtc=v;}
 battery_grant=(risc_runtime_capability_v1){.struct_size=sizeof(battery_grant)};
 if(rt->acquire("board.battery",1,0,&battery_grant)){const risc_battery_gauge_api_v1*v=battery_grant.api;if(v&&v->api_version==1&&v->struct_size>=sizeof(*v)&&v->read)battery=v;}
 risc_runtime_capability_v1 pref={.struct_size=sizeof(pref)};format=PORTABLE_TIME_FORMAT_12;
 if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&pref)){(void)portable_time_format_load(pref.api,&format);if(!rt->release(&pref))retain();}
}
#endif
static int xscale(int x){return x*app->screen_width()/480;}
static int yscale(int y){return y*app->screen_height()/800;}
static void text(int x,int y,int w,const char*s,bool heading){paper->text(xscale(x),yscale(y),xscale(w),s,heading?2:1,heading,true);}
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#include "paper_home_points.inc"
#endif
static void thick_line(int x0,int y0,int x1,int y1,int thickness){
 int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,error=dx+dy;
 for(;;){app->fill_rect(x0-thickness/2,y0-thickness/2,thickness,thickness,true);if(x0==x1&&y0==y1)break;int e=2*error;if(e>=dy){error+=dy;x0+=sx;}if(e<=dx){error+=dx;y0+=sy;}}
}
static const int16_t clock_ring[60][2]={
{0,-1000},
{105,-995},
{208,-978},
{309,-951},
{407,-914},
{500,-866},
{588,-809},
{669,-743},
{743,-669},
{809,-588},
{866,-500},
{914,-407},
{951,-309},
{978,-208},
{995,-105},
{1000,0},
{995,105},
{978,208},
{951,309},
{914,407},
{866,500},
{809,588},
{743,669},
{669,743},
{588,809},
{500,866},
{407,914},
{309,951},
{208,978},
{105,995},
{0,1000},
{-105,995},
{-208,978},
{-309,951},
{-407,914},
{-500,866},
{-588,809},
{-669,743},
{-743,669},
{-809,588},
{-866,500},
{-914,407},
{-951,309},
{-978,208},
{-995,105},
{-1000,0},
{-995,-105},
{-978,-208},
{-951,-309},
{-914,-407},
{-866,-500},
{-809,-588},
{-743,-669},
{-669,-743},
{-588,-809},
{-500,-866},
{-407,-914},
{-309,-951},
{-208,-978},
{-105,-995}};
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#define CLOCK_DRAW_OR_RETURN(...) do {if(!draw_clock(__VA_ARGS__))return;} while(0)
#define CLOCK_DRAW_RESULT bool
#else
#define CLOCK_DRAW_OR_RETURN(...) draw_clock(__VA_ARGS__)
#define CLOCK_DRAW_RESULT void
#endif
static CLOCK_DRAW_RESULT draw_clock(const twatch_rtc_time_v1*time,bool known,const char*notice,bool initial){
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(!home_points_refresh(known))return false;
#endif
 paper->begin();char buf[64];
 static const char*const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
 static const char*const months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
 if(known)snprintf(buf,sizeof(buf),"%s %02u %s",days[time->weekday%7],time->day,months[time->month-1]);else strcpy(buf,"TIME UNAVAILABLE");text(32,24,300,buf,true);
 paper_battery_draw(app,paper,336,32,battery_status);
 for(unsigned i=0;i<60;i++){int inner=i%5?185:174;thick_line(xscale(240+clock_ring[i][0]*inner/1000),yscale(264+clock_ring[i][1]*inner/1000),xscale(240+clock_ring[i][0]*197/1000),yscale(264+clock_ring[i][1]*197/1000),xscale(i%5?4:7));}
 if(known)snprintf(buf,sizeof(buf),"%02u:%02u",format==PORTABLE_TIME_FORMAT_24?time->hour:time->hour%12?time->hour%12:12,time->minute);else strcpy(buf,"--:--");
 int width=paper->measure(buf,true)*3;paper->text((app->screen_width()-width)/2,yscale(210),app->screen_width()-xscale(64),buf,1|PAPER_TEXT_CLOCK,true,true);
 text(174,330,170,"N O V A - 7",false);
 if(known&&format==PORTABLE_TIME_FORMAT_12)text(224,370,60,time->hour<12?"AM":"PM",false);
 app->fill_rect(xscale(32),yscale(474),xscale(416),3,true);
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 home_points_draw(notice);
#else
 (void)app->draw_icon(xscale(212),yscale(518),"solid:f00a",56,true);
 text(72,610,370,"SWIPE TO OPEN APPS",true);
#ifdef PORTABLE_QUICK_ACTIONS
 text(70,659,370,notice&&*notice?notice:"TOP EDGE: QUICK ACTIONS",false);
#else
 text(70,659,370,notice&&*notice?notice:"ANY DIRECTION",false);
#endif
 text(50,757,400,"UPDATES EVERY MINUTE",false);
#endif
 app->present(initial);
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 return true;
#endif
}
#if defined(PORTABLE_DESK_CLOCK) && !defined(PORTABLE_DESK_CLOCK_SPARSE_START)
#include "paper_desk_clock.inc"
#endif
void app_main(void){
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(!sparse_boot())return;
#else
 app=t5_app_get_api(1);rt=risc_runtime_get_api(1);
 if(!app||app->abi_version!=1||app->struct_size<sizeof(*app)||!app->poll||!app->millis||!app->fill_rect||!app->draw_icon||!app->set_back_exits_app||!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!rt->request_launch||!rt->yield_ms||!rt->diagnostic)return;
 paper=paper_presentation_get();if(!paper||paper->struct_size<sizeof(*paper)||!paper->begin||!paper->text||!paper->measure||!paper->contact)return;
 rtc=NULL;battery=NULL;open_clock();app->set_back_exits_app(false);
#ifdef PORTABLE_DESK_CLOCK
 int boot=desk_boot();
 if(boot==-2){portable_desk_adapter_retain();return;}
 if(boot<0){close_clock();return;}
 if(!portable_desk_adapter_foreground()){if(!portable_app_sleep_retained())close_clock();return;}
#endif
#endif
 twatch_rtc_time_v1 time={0};bool known=read_clock(&time),down=false,neutral=false;int start_x=0,start_y=0;char notice[48]={0};
#ifdef PORTABLE_DESK_CLOCK
 if(desk_returned){desk_returned=false;strcpy(notice,desk_notice());}
#endif

#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(desk_retained)return;
#endif
 battery_status=paper_battery_read(paper);
 uint32_t checked=app->millis();CLOCK_DRAW_OR_RETURN(&time,known,notice,true);
 for(;;){t5_app_input_t input={0};if(!app->poll(&input,20)){
#ifdef PORTABLE_ALARM_CLIENT
 if(portable_app_sleep_retained())return;
#endif
break;}
  if(input.exit_requested)break;
#ifdef PORTABLE_DESK_CLOCK
  if(desk_returned){desk_returned=false;down=neutral=false;strcpy(notice,desk_notice());known=read_clock(&time);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(desk_retained)return;
#endif
 CLOCK_DRAW_OR_RETURN(&time,known,notice,true);}
#endif
  if(input.buttons&PAPER_BUTTON_SLEEP_UNAVAILABLE){strcpy(notice,"SLEEP NOT AVAILABLE");CLOCK_DRAW_OR_RETURN(&time,known,notice,false);}
  springboard_contact c={0};paper->contact(&c);bool launch=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
  bool points_launch=false;
#endif
  if(!c.valid||c.cancelled){down=false;neutral=false;}
  else if(!c.down){
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   if(down&&c.released&&c.tap_eligible&&home_points_hit(start_x,start_y)&&home_points_hit(c.x,c.y)&&
      abs(c.x-start_x)<xscale(20)&&abs(c.y-start_y)<yscale(28)){launch=true;points_launch=true;}
#endif
   down=false;neutral=true;}
  else if(neutral){if(!down){down=true;start_x=c.x;start_y=c.y;}else if(abs(c.x-start_x)>=xscale(40)||abs(c.y-start_y)>=yscale(67)){launch=true;down=neutral=false;}}
  if(input.buttons&T5_APP_BUTTON_CONFIRM){launch=true;down=neutral=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   points_launch=false;
#endif
  }
  if(launch){close_clock();
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   const char *target=points_launch?PAPER_POINTS_APP:PAPER_CLOCK_LAUNCHER;
   if(rt->request_launch(target))break;
#else
   if(rt->request_launch(PAPER_CLOCK_LAUNCHER))break;
#endif
   open_clock();
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   strcpy(notice,points_launch?"UNABLE TO OPEN POINTS. RETRY.":"UNABLE TO OPEN APPS. RETRY.");
#else
   strcpy(notice,"UNABLE TO OPEN APPS. RETRY.");
#endif
   CLOCK_DRAW_OR_RETURN(&time,known,notice,false);}
  uint32_t now=app->millis();if((uint32_t)(now-checked)>=1000){twatch_rtc_time_v1 next={0};bool valid=read_clock(&next);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(desk_retained)return;
#endif
 paper_battery next_battery=paper_battery_read(paper);bool battery_dirty=paper_battery_changed(battery_status,next_battery);battery_status=next_battery;checked=now;if(battery_dirty||valid!=known||(valid&&(next.minute!=time.minute||next.hour!=time.hour||next.day!=time.day||next.month!=time.month||next.year!=time.year))){known=valid;time=next;CLOCK_DRAW_OR_RETURN(&time,known,notice,false);}}
 }
 close_clock();app->set_back_exits_app(true);
}
