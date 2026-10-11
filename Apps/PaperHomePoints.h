#pragma once
/* The Watch's copied, read-only nova_points_state is the view contract. The
 * selected X4 profile projects Utilities' UTC records through its frozen zone
 * rule. Only Home's previous/next fields are needed; no service authority. */
#include <stdio.h>
#include "PortablePointsState.h"
#include "PointsUtcSchedule.h"
static bool paper_points_event(const portable_timezone_rule *rule,const points_event *event,
                               const points_meta *meta,uint32_t today,nova_point_event *out) {
 int64_t epoch;portable_timezone_civil local;uint32_t day;
 if(!points_utc_to_unix(event->deadline,&epoch) ||
    portable_timezone_utc_to_local(rule,epoch,&local,NULL)!=PORTABLE_TIMEZONE_OK ||
    !points_utc_local_day(rule,event->deadline,&day))return false;
 *out=(nova_point_event){.at_rtc=event->deadline,.hour=local.hour,.minute=local.minute,
   .kind=event->kind,.source_slot=event->slot,.is_end=event->edge!=0,
   .day_offset=(int16_t)((int32_t)day-(int32_t)today),.color_index=6};
 if(event->kind==POINTS_CUSTOM_1 || event->kind==POINTS_CUSTOM_2) {
  unsigned index=event->kind-POINTS_CUSTOM_1;
  const char *label=meta->custom[index].name[0]?meta->custom[index].name:index?"CUSTOM 2":"CUSTOM 1";
  snprintf(out->label,sizeof(out->label),"%s",label);out->color_index=meta->custom[index].color;
 }
 return true;
}
static bool paper_points_project(const points_config *config,const points_meta *meta,
                                 const portable_timezone_rule *rule,int64_t epoch,nova_points_state *out) {
 nova_points_state state={.status=NOVA_POINTS_ERROR};points_projection projected;uint32_t today;
 if(!points_utc_from_unix(epoch,&state.now_rtc)||!points_utc_local_day(rule,state.now_rtc,&today)||
    !points_utc_project(rule,config,state.now_rtc,&projected))goto failed;
 state.next_count=(uint8_t)projected.count;state.previous_valid=projected.has_previous;
 state.status=state.next_count?NOVA_POINTS_READY:NOVA_POINTS_EMPTY;
 for(unsigned i=0;i<state.next_count;i++)
  if(!paper_points_event(rule,&projected.next[i],meta,today,&state.next[i]))goto failed;
 if(state.previous_valid&&!paper_points_event(rule,&projected.previous,meta,today,&state.previous))goto failed;
 state.revision=config->revision^(meta->revision*2166136261u)^(today*16777619u)^
   (state.previous_valid?state.previous.at_rtc:0)^(state.next_count?state.next[0].at_rtc:0)^(uint32_t)state.status;
 *out=state;return true;
failed:
 *out=(nova_points_state){.status=NOVA_POINTS_ERROR};return false;
}
static const char *paper_points_label(const nova_point_event *event) {
 if(event->is_end)return "BACK";
 switch(event->kind) {
  case POINTS_WORK_START:return "START WORK";
  case POINTS_WORK_END:return "END WORK";
  case POINTS_LUNCH:return "LUNCH";
  case POINTS_BREAK:return "BREAK";
  case POINTS_BEDTIME:return "WIND DOWN";
  case POINTS_CUSTOM_1:case POINTS_CUSTOM_2:return event->label;
  default:return "POINT ERROR";
 }
}
/* Elapsed UTC seconds, never subtraction of displayed civil hours. */
static unsigned paper_points_progress(const nova_points_state *state,unsigned width) {
 if(!state->previous_valid||!state->next_count)return 0;
 uint32_t start=state->previous.at_rtc,end=state->next[0].at_rtc,now=state->now_rtc;
 if(end<=start||now<=start)return 0;
 if(now>=end)return width;
 /* Bounded pixel accumulation avoids target libgcc's unsupported 64-bit
  * division relocations and cannot overflow even at the UTC domain limit. */
 uint32_t elapsed=now-start,span=end-start,remainder=0;unsigned filled=0;
 for(unsigned i=0;i<width;i++) {
  if(remainder>=span-elapsed){remainder-=span-elapsed;filled++;}
  else remainder+=elapsed;
 }
 return filled;
}
