#pragma once
#include "PortableTouchScroll.h"
#include <stdlib.h>

/* App-local horizontal pages. Input updates the latest position even while a
 * display token is BUSY. The owner renders that position when its lease is
 * ready; there is no animation frame queue or runtime UI policy. */
typedef struct {
    portable_scroll_viewport view;
    int width,limit,position_q8,velocity_q8;
    int start_x,start_y,last_x,last_y,start_q8,origin_page,from_q8,to_q8;
    uint32_t sample_ms,moved_ms,animation_ms;
    bool contact,eligible,dragged,horizontal,blocked,stop_tap,animating;
} springboard_pages;
#define SBS_PAGE_MS 180u
static inline int sbs_pages_offset(const springboard_pages *s) {return s->position_q8/256;}
static inline int sbs_pages_nearest(const springboard_pages *s) {
    return s->width?(s->position_q8+s->width*128)/(s->width*256):0;
}
static inline void sbs_pages_configure(springboard_pages *s,portable_scroll_viewport view,int width,unsigned pages) {
    if(width<1||width>4096||!pages||pages>128){width=1;pages=1;view=(portable_scroll_viewport){0};}
    s->view=view;s->width=width;s->limit=(int)(pages-1)*width;
    s->position_q8=portable_scroll_clamp(s->position_q8,s->limit*256);
    s->to_q8=portable_scroll_clamp(s->to_q8,s->limit*256);
}
static inline void sbs_pages_cancel(springboard_pages *s) {
    s->position_q8=portable_scroll_clamp(sbs_pages_nearest(s)*s->width,s->limit)*256;
    s->velocity_q8=0;s->contact=s->eligible=s->dragged=s->horizontal=s->animating=false;
    s->stop_tap=false;s->blocked=true;
}
static inline void sbs_pages_snap(springboard_pages *s,int page,uint32_t now) {
    s->from_q8=s->position_q8;s->to_q8=portable_scroll_clamp(page*s->width,s->limit)*256;
    s->animation_ms=now;s->velocity_q8=0;s->animating=s->from_q8!=s->to_q8;
}
static inline void sbs_pages_step(springboard_pages *s,uint32_t now) {
    if(!s->animating)return;
    uint32_t elapsed=now-s->animation_ms;
    if(elapsed>=SBS_PAGE_MS){s->position_q8=s->to_q8;s->animating=false;return;}
    int remain=(int)(SBS_PAGE_MS-elapsed)*1024/(int)SBS_PAGE_MS;
    int eased=1024-(remain*remain/1024)*remain/1024;
    /* Difference is at most one bounded page; use widened multiplication. */
    s->position_q8=s->from_q8+(int)((int64_t)(s->to_q8-s->from_q8)*eased/1024);
}
static inline unsigned sbs_pages_touch(springboard_pages *s,const portable_touch_sample *sample,uint32_t now) {
    int before=sbs_pages_offset(s);unsigned result=0;
    if(!sample->valid||sample->cancelled) {
        if(s->contact&&s->dragged)sbs_pages_snap(s,s->origin_page,now);
        s->contact=s->eligible=false;s->velocity_q8=0;s->blocked=true;
        sbs_pages_step(s,now);
        return sbs_pages_offset(s)!=before?PORTABLE_SCROLL_CHANGED:0;
    }
    if(s->blocked) {
        if(!sample->down||(sample->began&&sample->tap_eligible))s->blocked=false;
        else return 0;
    }
    bool snapshot_release=sample->released&&!sample->release_position_valid;
    int x=snapshot_release?s->last_x:sample->x,y=snapshot_release?s->last_y:sample->y;
    if(sample->began) {
        s->stop_tap=s->animating;s->animating=false;s->velocity_q8=0;
        s->contact=true;s->eligible=sample->tap_eligible;
        s->dragged=s->horizontal=false;s->start_x=s->last_x=x;s->start_y=s->last_y=y;
        s->start_q8=s->position_q8;s->origin_page=sbs_pages_nearest(s);
        s->sample_ms=s->moved_ms=now;
    }
    if(s->contact&&(sample->down||sample->released)) {
        int dx=x-s->start_x,dy=y-s->start_y;
        /* Raw MOVE delivery may set moved at 6 px. Wait for our 8 px axis
         * threshold before locking; otherwise fine-grained horizontal input
         * would be permanently classified as non-horizontal at 6 px. */
        if(!s->dragged&&abs(dx)+abs(dy)>=8) {
            s->dragged=true;
            s->horizontal=abs(dx)>=8&&abs(dx)>2*abs(dy)&&
                portable_scroll_inside(&s->view,s->start_x,s->start_y);
        }
        if(s->horizontal) {
            int target=s->start_q8-dx*256;
            s->position_q8=portable_scroll_clamp(target,s->limit*256);
            uint32_t dt=now-s->sample_ms;
            if(x!=s->last_x&&dt) {
                int velocity=(s->last_x-x)*256/(int)(dt>160?160:dt);
                s->velocity_q8=velocity>768?768:velocity<-768?-768:velocity;
                s->moved_ms=now;
            }
            if(s->position_q8!=target)s->velocity_q8=0;
        }
        s->last_x=x;s->last_y=y;s->sample_ms=now;
        if(sample->released) {
            s->contact=false;
            if(s->eligible&&!s->dragged&&!sample->moved&&sample->tap_eligible&&!s->stop_tap)
                result|=PORTABLE_SCROLL_TAP;
            int page=sbs_pages_nearest(s);
            if(s->horizontal&&s->eligible&&sample->tap_eligible) {
                /* A recent flick can finish the adjacent page. Stale velocity,
                 * a short drag or a reversed final movement cannot replay it. */
                if(abs(dx)>=44&&now-s->moved_ms<=80&&abs(s->velocity_q8)>=128)
                    page=(s->position_q8+s->velocity_q8*180+s->width*128)/(s->width*256);
                if(page<s->origin_page-1)page=s->origin_page-1;
                if(page>s->origin_page+1)page=s->origin_page+1;
            } else if(s->dragged)page=s->origin_page;
            sbs_pages_snap(s,page,now);
        }
    } else if(!sample->down)sbs_pages_step(s,now);
    if(sbs_pages_offset(s)!=before)result|=PORTABLE_SCROLL_CHANGED;
    return result;
}
