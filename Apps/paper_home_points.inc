/* Foreground-only native Points projection. No calls from sparse timer wake. */
#if !defined(PORTABLE_DESK_CLOCK_SPARSE_START) || !defined(ALARM_NATIVE_UTC)
#error "Home Points requires the native sparse Clock profile"
#endif
#include "PaperHomePoints.h"
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
 if(minutes>=60)snprintf(out,size,"IN %lu H %02lu M",(unsigned long)(minutes/60),(unsigned long)(minutes%60));
 else snprintf(out,size,"IN %lu MIN",(unsigned long)minutes);
}
static void home_time(char *out,size_t size,const nova_point_event *event,bool next) {
 unsigned hour=format==PORTABLE_TIME_FORMAT_24?event->hour:event->hour%12?event->hour%12:12;
 snprintf(out,size,"%s%02u:%02u%s",next?"AT ":"",hour,event->minute,
  format==PORTABLE_TIME_FORMAT_24?"":event->hour<12?" AM":" PM");
}
static void home_right(int right,int y,const char *value,bool heading) {
 int width=paper->measure(value,heading);paper->text(xscale(right)-width,yscale(y),width+1,value,1,heading,true);
}
static void home_points_draw(const char *notice) {
 char value[48];
 if(home_points.status!=NOVA_POINTS_READY||!home_points.next_count) {
  text(32,495,416,"POINTS IN TIME",true);
  text(32,553,416,home_points.status==NOVA_POINTS_EMPTY?"NO UPCOMING POINTS":"POINTS UNAVAILABLE",true);
  text(32,614,416,home_points.status==NOVA_POINTS_EMPTY?"OPEN POINTS TO EDIT":"OPEN POINTS TO CHECK",false);
 } else {
  const nova_point_event *next=&home_points.next[0];
  text(32,491,160,"N E X T",false);home_time(value,sizeof(value),next,true);home_right(448,489,value,true);
  const char *label=paper_points_label(next);
  paper->text(xscale(32),yscale(510),xscale(416),label,1|PAPER_TEXT_LARGE|PAPER_TEXT_FIT,true,true);
  home_duration(value,sizeof(value),next->at_rtc-home_points.now_rtc);
  paper->text(xscale(32),yscale(578),xscale(416),value,1|PAPER_TEXT_LARGE|PAPER_TEXT_FIT,false,true);
  app->fill_rect(xscale(32),yscale(626),xscale(416),yscale(24),true);
  app->fill_rect(xscale(36),yscale(630),xscale(408),yscale(16),false);
  unsigned progress=paper_points_progress(&home_points,408);
  if(progress)app->fill_rect(xscale(36),yscale(630),xscale((int)progress),yscale(16),true);
  for(unsigned i=1;i<home_points.next_count&&i<3;i++) {
   const nova_point_event *event=&home_points.next[i];int y=670+(int)(i-1)*38;
   home_time(value,sizeof(value),event,false);
   paper->text(xscale(32),yscale(y-8),xscale(120),value,1|PAPER_TEXT_LARGE|PAPER_TEXT_FIT,false,true);
   paper->text(xscale(166),yscale(y-8),xscale(146),paper_points_label(event),1|PAPER_TEXT_LARGE|PAPER_TEXT_FIT,false,true);
   home_duration(value,sizeof(value),event->at_rtc-home_points.now_rtc);
   int width=paper->measure(value,false)*2;if(width>xscale(130))width=xscale(130);
   paper->text(xscale(448)-width,yscale(y-8),width,value,1|PAPER_TEXT_LARGE|PAPER_TEXT_FIT,false,true);
  }
 }
 if(notice&&*notice)text(32,757,416,notice,false);
}
static bool home_points_hit(int x,int y) {
 return x>=xscale(32)&&x<xscale(448)&&y>=yscale(474)&&y<yscale(744);
}
