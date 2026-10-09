/* Foreground-only native Points projection. No calls from sparse timer wake. */
#if !defined(PORTABLE_DESK_CLOCK_SPARSE_START) || !defined(ALARM_NATIVE_UTC)
#error "Home Points requires the native sparse Clock profile"
#endif
#include "PaperHomePoints.h"
#include "PaperQuickIntent.h"
#include "paper_home_type.inc"
#ifndef PAPER_POINTS_APP
#define PAPER_POINTS_APP "points_in_time.elf"
#endif
static nova_points_state home_points;
static bool home_points_status(int32_t status) {
 if(desk_retained||portable_app_sleep_retained())return false;
 if(status!=RISC_KEY_VALUE_OK && status!=RISC_KEY_VALUE_NOT_FOUND &&
    status!=RISC_KEY_VALUE_BUFFER_SMALL && status!=RISC_KEY_VALUE_INVALID && status!=RISC_KEY_VALUE_IO) {
  desk_fence();return false;
 }
 return true;
}
static bool home_points_refresh(bool known) {
 home_points=(nova_points_state){.status=NOVA_POINTS_UNAVAILABLE};
 if(!desk_promoted||desk_held||desk_retained||portable_app_sleep_retained())return false;
 if(!known||!desk_home_valid||!desk_zone_known)return true;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,5,&grant)){desk_fence();return false;}
 if(desk_retained||portable_app_sleep_retained())return false;
 if(!grant.api||!grant.slot||!grant.generation){desk_fence();return false;}
 const risc_key_value_v1 *kv=grant.api;points_config config={0};points_meta meta={0};bool loaded=false;
 if(kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get) {
  uint8_t bytes[POINTS_RECORD_SIZE];uint32_t length=0;
  int32_t result=kv->get(kv->context,POINTS_CONFIG_KEY,bytes,sizeof(bytes),&length);
  if(!home_points_status(result))return false;
  bool defaults=result==RISC_KEY_VALUE_NOT_FOUND;
  if(defaults){config=points_default_config();loaded=true;}
  else loaded=result==RISC_KEY_VALUE_OK&&points_config_decode(&config,bytes,length);
  if(loaded) {
   length=0;result=kv->get(kv->context,POINTS_META_KEY,bytes,sizeof(bytes),&length);
   if(!home_points_status(result))return false;
   if(defaults&&result==RISC_KEY_VALUE_NOT_FOUND)meta=points_default_meta();
   else if(result==RISC_KEY_VALUE_OK&&!points_meta_decode(&meta,bytes,length))meta=(points_meta){0};
  }
 }
 if(!rt->release(&grant)||grant.api||grant.slot||grant.generation){desk_fence();return false;}
 if(desk_retained||portable_app_sleep_retained())return false;
 if(loaded)(void)paper_points_project(&config,&meta,&desk_rule,desk_home_epoch,&home_points);
 else home_points.status=NOVA_POINTS_ERROR;
 return true;
}
static void home_duration(char *out,size_t size,uint32_t seconds) {
 uint32_t minutes=(seconds+59u)/60u;
 if(minutes>=1440)snprintf(out,size,"IN %lu D %lu H",(unsigned long)(minutes/1440),(unsigned long)(minutes%1440/60));
 else if(minutes>=60)snprintf(out,size,"IN %lu H %02lu M",(unsigned long)(minutes/60),(unsigned long)(minutes%60));
 else snprintf(out,size,"IN %lu MIN",(unsigned long)minutes);
}
static void home_time(char *out,size_t size,const nova_point_event *event,bool next) {
 unsigned hour=format==PORTABLE_TIME_FORMAT_24?event->hour:event->hour%12?event->hour%12:12;
 snprintf(out,size,"%s%02u:%02u%s",next?"AT ":"",hour,event->minute,
  format==PORTABLE_TIME_FORMAT_24?"":event->hour<12?" AM":" PM");
}
static void home_points_draw(const char *notice) {
 char value[48];
 if(home_points.status!=NOVA_POINTS_READY||!home_points.next_count) {
  home_type(&HOME_O900_52,32,568,home_points.status==NOVA_POINTS_EMPTY?"NO POINTS":"POINTS UNAVAILABLE",0,0,416,0);
  home_type(&HOME_R700_26,32,608,home_points.status==NOVA_POINTS_EMPTY?"TAP TO ADD ONE":"OPEN POINTS TO CHECK",2,0,416,0);
 } else {
  const nova_point_event *next=&home_points.next[0];
  home_type(&HOME_R700_22,32,508,"NEXT",8,0,0,0);
  home_time(value,sizeof(value),next,true);home_type(&HOME_O700_24,448,508,value,0,1,250,0);
  home_type(&HOME_O900_52,32,568,paper_points_label(next),0,0,416,home_svg_fit(paper_points_label(next),52,86,0,416));
  home_duration(value,sizeof(value),next->at_rtc-home_points.now_rtc);
  home_type(&HOME_R700_30,32,608,value,2,0,416,0);
  home_round(30,624,420,28,6,true);home_round(34,628,412,20,2,false);
  unsigned progress=paper_points_progress(&home_points,416);
  if(home_points.previous_valid&&progress<8)progress=8;
  if(progress)home_round(30,624,(int)progress+4,28,6,true);
  for(unsigned i=1;i<home_points.next_count&&i<3;i++) {
   const nova_point_event *event=&home_points.next[i];int baseline=692+(int)(i-1)*38;
   home_time(value,sizeof(value),event,false);
   home_type(&HOME_O700_28,32,baseline,value,0,0,120,0);
   home_type(&HOME_R700_28,164,baseline,paper_points_label(event),2,0,150,home_svg_fit(paper_points_label(event),28,52,2,150));
   home_duration(value,sizeof(value),event->at_rtc-home_points.now_rtc);
   home_type(&HOME_R600_22,448,baseline,value,0,1,130,home_svg_fit(value,22,52,0,130));
  }
 }
 if(notice&&*notice)home_type(&HOME_R600_22,32,778,notice,0,0,416,0);
}
static bool home_points_hit(int x,int y) {
 return x>=xscale(24)&&x<xscale(456)&&y>=yscale(484)&&y<yscale(710);
}
static bool home_dial_hit(int x,int y) {
 return x>=xscale(40)&&x<xscale(440)&&y>=yscale(72)&&y<yscale(462);
}
static inline bool home_top_hit(int x,int y) {
 return x>=0&&x<xscale(480)&&y>=0&&y<yscale(72);
}
static unsigned home_target(int x,int y) {
 if(home_top_hit(x,y))return HOME_TOP;
 if(home_dial_hit(x,y))return HOME_DIAL;
 if(home_points_hit(x,y))return HOME_POINTS;
 return HOME_NONE;
}
