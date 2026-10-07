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
#ifndef PAPER_CLOCK_LAUNCHER
#define PAPER_CLOCK_LAUNCHER "springboard.elf"
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *rt;
static const paper_presentation *paper;
static risc_runtime_capability_v1 rtc_grant,battery_grant;
static const twatch_rtc_api_v1 *rtc;
static const risc_battery_gauge_api_v1 *battery;
static unsigned format;
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
static int xscale(int x){return x*app->screen_width()/480;}
static int yscale(int y){return y*app->screen_height()/800;}
static void text(int x,int y,int w,const char*s,bool heading){paper->text(xscale(x),yscale(y),xscale(w),s,heading?2:1,heading,true);}
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
static void draw_clock(const twatch_rtc_time_v1*time,bool known,const char*notice,bool initial){
 paper->begin();char buf[64];
 static const char*const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
 static const char*const months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
 if(known)snprintf(buf,sizeof(buf),"%s %02u %s",days[time->weekday%7],time->day,months[time->month-1]);else strcpy(buf,"TIME UNAVAILABLE");text(32,24,300,buf,true);
 risc_battery_sample_v1 sample={0};
 if(battery&&battery->read(battery->context,&sample)&&!(sample.flags&RISC_BATTERY_PROFILE_MISSING)&&sample.percent<=100){snprintf(buf,sizeof(buf),"%u%%",sample.percent);text(336,32,110,buf,false);}else text(336,32,110,"--%",false);
 for(unsigned i=0;i<60;i++){int inner=i%5?185:174;thick_line(xscale(240+clock_ring[i][0]*inner/1000),yscale(264+clock_ring[i][1]*inner/1000),xscale(240+clock_ring[i][0]*197/1000),yscale(264+clock_ring[i][1]*197/1000),xscale(i%5?4:7));}
 if(known)snprintf(buf,sizeof(buf),"%02u:%02u",format==PORTABLE_TIME_FORMAT_24?time->hour:time->hour%12?time->hour%12:12,time->minute);else strcpy(buf,"--:--");
 int width=paper->measure(buf,true)*3;paper->text((app->screen_width()-width)/2,yscale(210),app->screen_width()-xscale(64),buf,1|PAPER_TEXT_CLOCK,true,true);
 text(174,330,170,"N O V A - 7",false);
 if(known&&format==PORTABLE_TIME_FORMAT_12)text(224,370,60,time->hour<12?"AM":"PM",false);
 app->fill_rect(xscale(32),yscale(474),xscale(416),3,true);
 (void)app->draw_icon(xscale(212),yscale(518),"solid:f00a",56,true);
 text(72,610,370,"SWIPE TO OPEN APPS",true);
 text(70,659,370,notice&&*notice?notice:"ANY DIRECTION",false);
 text(50,757,400,"UPDATES EVERY MINUTE",false);app->present(initial);
}
void app_main(void){
 app=t5_app_get_api(1);rt=risc_runtime_get_api(1);
 if(!app||app->abi_version!=1||app->struct_size<sizeof(*app)||!app->poll||!app->millis||!app->fill_rect||!app->draw_icon||!app->set_back_exits_app||!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!rt->request_launch||!rt->yield_ms||!rt->diagnostic)return;
 paper=paper_presentation_get();if(!paper||paper->struct_size<sizeof(*paper)||!paper->begin||!paper->text||!paper->measure||!paper->contact)return;
 rtc=NULL;battery=NULL;open_clock();app->set_back_exits_app(false);
 twatch_rtc_time_v1 time={0};bool known=read_clock(&time),down=false,neutral=false;int start_x=0,start_y=0;char notice[48]={0};
 uint32_t checked=app->millis();draw_clock(&time,known,notice,true);
 for(;;){t5_app_input_t input={0};if(!app->poll(&input,20)){
#ifdef PORTABLE_ALARM_CLIENT
 if(portable_app_sleep_retained())return;
#endif
break;}
  springboard_contact c={0};paper->contact(&c);bool launch=false;
  if(!c.valid||c.cancelled){down=false;neutral=false;}
  else if(!c.down){down=false;neutral=true;}
  else if(neutral){if(!down){down=true;start_x=c.x;start_y=c.y;}else if(abs(c.x-start_x)>=xscale(40)||abs(c.y-start_y)>=yscale(67)){launch=true;down=neutral=false;}}
  if(input.buttons&T5_APP_BUTTON_CONFIRM){launch=true;down=neutral=false;}
  if(launch){close_clock();if(rt->request_launch(PAPER_CLOCK_LAUNCHER))break;open_clock();strcpy(notice,"UNABLE TO OPEN APPS. RETRY.");draw_clock(&time,known,notice,false);}
  uint32_t now=app->millis();if((uint32_t)(now-checked)>=1000){twatch_rtc_time_v1 next={0};bool valid=read_clock(&next);checked=now;if(valid!=known||(valid&&(next.minute!=time.minute||next.hour!=time.hour||next.day!=time.day||next.month!=time.month||next.year!=time.year))){known=valid;time=next;draw_clock(&time,known,notice,false);}}
 }
 close_clock();app->set_back_exits_app(true);
}
