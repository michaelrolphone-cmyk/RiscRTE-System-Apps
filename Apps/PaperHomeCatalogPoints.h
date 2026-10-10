#pragma once
/* Presentation-only conversion of the service's copied catalog window. */
#include "PortablePointsCatalogView.h"
#include "PortablePointsState.h"
#include "PointsUtcSchedule.h"
typedef struct {
 uint32_t event_id,type_id,revision,at_rtc,parent_day,color;
 uint8_t hour,minute,edge,mode;int16_t day_offset;char label[32];
} paper_catalog_event;
typedef struct {
 uint32_t revision,now_rtc;unsigned next_count;bool previous_valid;
 nova_points_status status;paper_catalog_event previous,next[POINTS_CATALOG_NEXT_COUNT];
} paper_catalog_state;
static bool paper_catalog_event_project(const portable_timezone_rule *rule,
 const portable_points_catalog_event *event,uint32_t today,paper_catalog_event *out) {
 int64_t epoch;portable_timezone_civil local;uint32_t day;
 if(!points_utc_to_unix(event->deadline,&epoch) ||
    portable_timezone_utc_to_local(rule,epoch,&local,NULL)!=PORTABLE_TIMEZONE_OK ||
    !points_utc_local_day(rule,event->deadline,&day))return false;
 *out=(paper_catalog_event){.event_id=event->event_id,.type_id=event->type_id,.revision=event->revision,
  .at_rtc=event->deadline,.parent_day=event->parent_day,.color=event->color,.hour=local.hour,
  .minute=local.minute,.edge=event->edge,.mode=event->mode,.day_offset=(int16_t)((int32_t)day-(int32_t)today)};
 memcpy(out->label,event->label,sizeof(out->label));return true;
}
static bool paper_catalog_project(const portable_points_catalog_view *view,
 const portable_timezone_rule *rule,int64_t epoch,paper_catalog_state *out) {
 paper_catalog_state state={.status=NOVA_POINTS_ERROR};portable_points_catalog_view projected;uint32_t today;
 if(!points_utc_from_unix(epoch,&state.now_rtc) ||
    !portable_points_catalog_view_at(view,state.now_rtc,&projected) ||
    !points_utc_local_day(rule,state.now_rtc,&today))goto failed;
 state.revision=projected.catalog_revision;state.next_count=projected.count;
 state.previous_valid=projected.has_previous;state.status=state.next_count?NOVA_POINTS_READY:NOVA_POINTS_EMPTY;
 if(state.previous_valid&&!paper_catalog_event_project(rule,&projected.previous,today,&state.previous))goto failed;
 for(unsigned i=0;i<state.next_count;i++)
  if(!paper_catalog_event_project(rule,&projected.next[i],today,&state.next[i]))goto failed;
 *out=state;return true;
failed:
 *out=(paper_catalog_state){.status=NOVA_POINTS_ERROR};return false;
}
static unsigned paper_catalog_progress(const paper_catalog_state *state,unsigned width) {
 if(!state->previous_valid||!state->next_count)return 0;
 uint32_t start=state->previous.at_rtc,end=state->next[0].at_rtc,now=state->now_rtc;
 if(end<=start||now<=start)return 0;
 if(now>=end)return width;
 uint32_t elapsed=now-start,span=end-start,remainder=0;unsigned filled=0;
 for(unsigned i=0;i<width;i++) {
  if(remainder>=span-elapsed){remainder-=span-elapsed;filled++;}else remainder+=elapsed;
 }
 return filled;
}
