/* Main clock for retaining monochrome displays. Shared Runtime capability
 * client only. Watch's clock source/UI remains in its existing deployment. */
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableAppSleep.h"
#include "PaperPresentation.h"
#include "PaperFrame.h"
#include "PortablePerformance.h"
#include "PortableStageLog.h"
#ifdef PORTABLE_RESIDENT_SHELL_HOST
#include "PortableResidentShell.h"
#endif
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
#ifndef PORTABLE_HOME_POINTS_NATIVE_UTC
static void text(int x,int y,int w,const char*s,bool heading){paper->text(xscale(x),yscale(y),xscale(w),s,heading?2:1,heading,true);}
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#include "paper_home_points.inc"
#endif
#ifndef PORTABLE_HOME_POINTS_NATIVE_UTC
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
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#define CLOCK_DRAW_OR_RETURN(...) do {if(!draw_clock(__VA_ARGS__))return;} while(0)
#define CLOCK_DRAW_RESULT bool
#else
#define CLOCK_DRAW_OR_RETURN(...) draw_clock(__VA_ARGS__)
#define CLOCK_DRAW_RESULT void
#endif
#include "PortableRasterLayer.h"
static portable_raster_layer *home_scene_layer;
static bool clock_dirty,clock_clean;
static CLOCK_DRAW_RESULT draw_clock(const twatch_rtc_time_v1*time,bool known,const char*notice,bool initial){
 bool rebuild=clock_dirty||initial;clock_clean|=initial;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 if(home_refresh_pending){if(!home_points_refresh(known))return false;home_refresh_pending=false;rebuild=true;}
#endif
 clock_dirty|=rebuild;
 if(!paper_frame_ready()) {
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
  return true;
#else
  return;
#endif
 }
 paper->begin();
 bool capture=false;
 if(!rebuild&&portable_layer_valid(home_scene_layer))goto home_composite;
 capture=portable_layer_begin(&home_scene_layer);
 if(!capture)portable_layer_release(&home_scene_layer);
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 home_press_begin();
#endif
#ifndef PORTABLE_HOME_POINTS_NATIVE_UTC
 char buf[64];
 static const char*const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
 static const char*const months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 home_reference_header(time,known);
#else
 if(known)snprintf(buf,sizeof(buf),"%s %02u %s",days[time->weekday%7],time->day,months[time->month-1]);else strcpy(buf,"TIME UNAVAILABLE");text(32,24,300,buf,true);
 paper_battery_draw(app,paper,336,32,battery_status);
 for(unsigned i=0;i<60;i++){int inner=i%5?185:174;thick_line(xscale(240+clock_ring[i][0]*inner/1000),yscale(264+clock_ring[i][1]*inner/1000),xscale(240+clock_ring[i][0]*197/1000),yscale(264+clock_ring[i][1]*197/1000),xscale(i%5?4:7));}
 if(known)snprintf(buf,sizeof(buf),"%u:%02u",format==PORTABLE_TIME_FORMAT_24?time->hour:time->hour%12?time->hour%12:12,time->minute);else strcpy(buf,"--:--");
 int width=paper->measure(buf,true)*3;paper->text((app->screen_width()-width)/2,yscale(210),app->screen_width()-xscale(64),buf,1|PAPER_TEXT_CLOCK,true,true);
 text(174,330,170,"N O V A - 7",false);
 if(known&&format==PORTABLE_TIME_FORMAT_12)text(224,370,60,time->hour<12?"AM":"PM",false);
 app->fill_rect(xscale(32),yscale(474),xscale(416),3,true);
#endif
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
 if(capture)portable_raster_layer_end();
home_composite:
 if(portable_layer_valid(home_scene_layer))portable_raster_layer_blit(home_scene_layer,0,0,app->screen_width(),app->screen_height(),0,0);
 app->present(clock_clean);clock_dirty=clock_clean=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 return true;
#endif
}
#if defined(PORTABLE_DESK_CLOCK) && !defined(PORTABLE_DESK_CLOCK_SPARSE_START)
#include "paper_desk_clock.inc"
#endif
static void home_main(void){
 portable_layer_release(&home_scene_layer);
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
#ifdef PORTABLE_RESIDENT_LEGACY_HANDOFF
 /* Resume a saved resident/file-open caller before the first Home scene.
  * Sparse TIMER exits within sparse_boot and never reaches this boundary. */
 close_clock();
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
 if(desk_retained||portable_app_sleep_retained())return;
#endif
 int resumed=portable_resident_run_foreground(NULL);
 if(resumed<0 || resumed==PORTABLE_RESIDENT_HANDOFF)return;
 open_clock();
#endif
 twatch_rtc_time_v1 time={0};bool known=read_clock(&time),down=false,neutral=false;int start_x=0,start_y=0;char notice[48]={0};
#ifdef PORTABLE_DESK_CLOCK
 if(desk_returned){desk_returned=false;strcpy(notice,desk_notice());}
#endif

#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(desk_retained)return;
#endif
 battery_status=paper_battery_read(paper);
 clock_dirty=clock_clean=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 home_pressed=home_pending=HOME_NONE;home_refresh_pending=true;
#endif
 paper_transition_begin();
 uint32_t checked=app->millis();CLOCK_DRAW_OR_RETURN(&time,known,notice,true);
 for(;;){
 t5_app_input_t input={0};if(!app->poll(&input,20)){
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
 clock_dirty=clock_clean=true;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 home_pressed=home_pending=HOME_NONE;home_refresh_pending=true;
#endif
}
#endif
  if(input.buttons&PAPER_BUTTON_SLEEP_UNAVAILABLE){strcpy(notice,"SLEEP NOT AVAILABLE");clock_dirty=true;}
  springboard_contact c={0};paper->contact(&c);bool launch=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
  const char *home_launch=NULL;unsigned tapped=HOME_NONE,previous_press=home_pressed;
#ifdef PORTABLE_QUICK_ACTIONS
  bool open_quick=false;
#endif
#endif
  if(!c.valid||c.cancelled){down=false;neutral=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   if(!home_pending)home_pressed=HOME_NONE;
#endif
  }
  else if(down&&(c.down||c.released)&&
      (abs(c.x-start_x)>=xscale(40)||abs(c.y-start_y)>=yscale(67))) {
   /* A release can carry the final movement even when no intermediate move
    * was delivered. Once a swipe wins, it cannot become a block tap. */
   launch=true;down=neutral=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   home_pending=home_pressed=HOME_NONE;
#ifdef PORTABLE_QUICK_ACTIONS
   open_quick=c.y-start_y>abs(c.x-start_x);
   launch=!open_quick;
#endif
#endif
  }
  else if(!c.down){
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   /* A reserved top-strip tap is replayed as one began+released sample. Its
    * valid tap flag and the existing neutral gate preserve contact custody. */
   if(neutral&&c.began&&c.released&&c.tap_eligible&&home_top_hit(c.x,c.y)) {
    down=true;start_x=c.x;start_y=c.y;
   }
   if(down&&c.released&&c.tap_eligible&&abs(c.x-start_x)<xscale(16)&&abs(c.y-start_y)<yscale(16)) {
    unsigned target=home_target(start_x,start_y);
    if(target==home_target(c.x,c.y))tapped=target;
#ifndef PORTABLE_QUICK_ACTIONS
    if(tapped==HOME_TOP)tapped=HOME_NONE;
#endif
   }
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   if(!home_pending)home_pressed=HOME_NONE;
#endif
   down=false;neutral=true;}
  /* The raw reducer already requires neutral at ownership entry. Its fresh
   * eligible DOWN may be the first logical pass after capture-only work. */
  else if(!down&&(neutral||(c.began&&c.tap_eligible))){down=true;neutral=true;start_x=c.x;start_y=c.y;}
  if(input.buttons&T5_APP_BUTTON_CONFIRM){launch=true;down=neutral=false;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   tapped=HOME_NONE;
#ifdef PORTABLE_QUICK_ACTIONS
   open_quick=false;
#endif
#endif
  }
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
#ifdef PORTABLE_QUICK_ACTIONS
  if(open_quick) {
   paper_transition_cancel();
   clock_dirty|=previous_press!=home_pressed;
   if(portable_paper_quick_open())continue;
   strcpy(notice,"QUICK ACTIONS UNAVAILABLE");clock_dirty=true;
  }
#endif
  /* Contact-down is ambiguous: a swipe often starts on a Home block. Only a
   * completed, movement-eligible tap earns block feedback. Navigation keeps
   * the sharp Home image and drains its existing frame before handoff. */
  if(tapped) {
   if(paper_transition_active()){paper_transition_cancel();clock_dirty=true;}
   home_pending=tapped;home_pressed=tapped;
  }
  if(previous_press!=home_pressed)clock_dirty=true;
  if(home_pending) {
   unsigned chosen=home_pending;home_pending=HOME_NONE;
#ifdef PORTABLE_QUICK_ACTIONS
   if(chosen==HOME_TOP) {
    home_pressed=HOME_NONE;clock_dirty=true;
    if(portable_paper_quick_open())continue;
    strcpy(notice,"QUICK ACTIONS UNAVAILABLE");
   } else
#endif
   {launch=true;home_launch=home_launch_target(chosen);}
  }
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  const char *shell_target=portable_resident_take_launch();
  if(shell_target)launch=true;
#endif
  if(launch){paper_transition_cancel();portable_stage_log(rt,"action","name=clock-launch");portable_perf_action(PORTABLE_PERF_CLOCK_LAUNCH,true);if(!paper_frame_drain())return;close_clock();
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
   if(desk_retained||portable_app_sleep_retained())return;
#endif
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   const char *target=home_launch?home_launch:PAPER_CLOCK_LAUNCHER;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
   int shell_result=portable_resident_run_foreground(shell_target?shell_target:target);
   if(shell_result<0 || shell_result==PORTABLE_RESIDENT_HANDOFF)return;
#else
   if(rt->request_launch(target))break;
#endif
#else
#ifdef PORTABLE_RESIDENT_SHELL_HOST
   int shell_result=portable_resident_run_foreground(shell_target?shell_target:PAPER_CLOCK_LAUNCHER);
   if(shell_result<0 || shell_result==PORTABLE_RESIDENT_HANDOFF)return;
#else
   if(rt->request_launch(PAPER_CLOCK_LAUNCHER))break;
#endif
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
   if(shell_result==0)
#endif
   portable_perf_action(PORTABLE_PERF_LAUNCH_FAILED,false);
   open_clock();
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   home_pressed=HOME_NONE;
   strcpy(notice,"UNABLE TO OPEN APP. RETRY.");
#else
   strcpy(notice,"UNABLE TO OPEN APPS. RETRY.");
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
   if(shell_result>0)notice[0]=0;
   down=neutral=false;known=read_clock(&time);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
   if(desk_retained || portable_app_sleep_retained())return;
#endif
   checked=app->millis();
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
   home_refresh_pending=true;
#endif
   if(portable_resident_take_sleep()) {
    if(!portable_desk_adapter_sleep())return;
   }
   clock_clean=true;
#endif
   clock_dirty=true;}
  uint32_t now=app->millis();if((uint32_t)(now-checked)>=1000){twatch_rtc_time_v1 next={0};bool valid=read_clock(&next);
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 if(desk_retained)return;
#endif
 paper_battery next_battery=paper_battery_read(paper);bool battery_dirty=paper_battery_changed(battery_status,next_battery);battery_status=next_battery;checked=now;if(battery_dirty||valid!=known||(valid&&(next.minute!=time.minute||next.hour!=time.hour||next.day!=time.day||next.month!=time.month||next.year!=time.year))){known=valid;time=next;clock_dirty=true;
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 home_refresh_pending=true;
#endif
 }}
#if defined(PORTABLE_HOME_POINTS_NATIVE_UTC) && defined(PORTABLE_DESK_POINTS_SNAPSHOT)
 bool points_changed=false;
 if(!home_points_poll(known,now,&points_changed))return;
 clock_dirty|=points_changed;
#endif
 if(clock_dirty||paper_transition_active())CLOCK_DRAW_OR_RETURN(&time,known,notice,false);
 }
 if(!paper_frame_drain())return;
 close_clock();
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
 if(desk_retained||portable_app_sleep_retained())return;
#endif
 portable_layer_release(&home_scene_layer);
 app->set_back_exits_app(true);
}

void app_main(void){home_main();portable_layer_release(&home_scene_layer);}
