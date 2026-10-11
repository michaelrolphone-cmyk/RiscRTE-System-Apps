#include <assert.h>
#include <stdio.h>
#include "../../Apps/springboard_pages.h"
static springboard_pages model(void) {
    springboard_pages s={0};sbs_pages_configure(&s,(portable_scroll_viewport){0,88,480,640},480,2);return s;
}
static unsigned touch(springboard_pages *s,uint32_t ms,int x,int y,bool began,bool down,bool released,bool queued) {
    portable_touch_sample p={.valid=true,.tap_eligible=true,.x=(uint16_t)x,.y=(uint16_t)y,
        .began=began,.down=down,.released=released,.release_position_valid=queued};
    return sbs_pages_touch(s,&p,ms);
}
static void settle(springboard_pages *s,uint32_t ms) {
    touch(s,ms,0,0,false,false,false,false);assert(!s->animating&&!s->velocity_q8);
    assert(s->position_q8%(480*256)==0);
}
int main(void) {
    springboard_pages s=model();
    assert(!(touch(&s,0,400,136,true,true,false,false)&PORTABLE_SCROLL_TAP));
    touch(&s,80,320,136,false,true,false,false);assert(sbs_pages_offset(&s)==80);
    touch(&s,140,260,136,false,true,false,false);assert(sbs_pages_offset(&s)==140);
    assert(!(touch(&s,160,400,136,false,false,true,false)&PORTABLE_SCROLL_TAP));
    assert(sbs_pages_offset(&s)==140&&s.animating&&s.to_q8==480*256);
    touch(&s,200,0,0,false,false,false,false);assert(sbs_pages_offset(&s)>140&&sbs_pages_offset(&s)<480);
    settle(&s,340);assert(sbs_pages_offset(&s)==480);
    /* Bounds neither wrap nor displace the vertical coordinate. */
    touch(&s,400,400,136,true,true,false,false);touch(&s,420,10,136,false,true,false,false);
    assert(sbs_pages_offset(&s)==480);touch(&s,440,10,136,false,false,true,true);settle(&s,620);
    touch(&s,700,100,136,true,true,false,false);touch(&s,780,260,136,false,true,false,false);
    assert(sbs_pages_offset(&s)==320);touch(&s,800,260,136,false,false,true,true);settle(&s,980);assert(!sbs_pages_offset(&s));
    /* Recent reverse velocity chooses the final direction, not the first. */
    touch(&s,1000,400,136,true,true,false,false);touch(&s,1100,180,136,false,true,false,false);
    touch(&s,1160,300,136,false,true,false,false);assert(sbs_pages_offset(&s)==100);
    touch(&s,1180,300,136,false,false,true,true);settle(&s,1360);assert(!sbs_pages_offset(&s));
    /* A stationary pause discards flick velocity; short drags snap back. */
    touch(&s,1400,400,136,true,true,false,false);touch(&s,1420,300,136,false,true,false,false);
    touch(&s,1600,300,136,false,false,true,true);settle(&s,1780);assert(!sbs_pages_offset(&s));
    touch(&s,1800,400,136,true,true,false,false);touch(&s,1820,365,136,false,true,false,false);
    touch(&s,1840,365,136,false,false,true,true);settle(&s,2020);assert(!sbs_pages_offset(&s));
    /* Direction locks, no vertical-grid route, and movement cancels tap. */
    touch(&s,2100,240,300,true,true,false,false);touch(&s,2120,240,280,false,true,false,false);
    touch(&s,2140,100,280,false,true,false,false);assert(!sbs_pages_offset(&s));
    assert(!(touch(&s,2160,100,280,false,false,true,true)&PORTABLE_SCROLL_TAP));
    touch(&s,2200,400,136,true,true,false,false);touch(&s,2220,380,136,false,true,false,false);
    touch(&s,2320,240,500,false,true,false,false);assert(sbs_pages_offset(&s)==160);
    touch(&s,2340,240,500,false,false,true,true);settle(&s,2520);assert(sbs_pages_offset(&s)==480);
    /* Cancellation returns to the gesture's origin and suppresses inherited contacts. */
    s=model();touch(&s,0,400,136,true,true,false,false);touch(&s,80,200,136,false,true,false,false);
    portable_touch_sample cancel={.cancelled=true};sbs_pages_touch(&s,&cancel,100);settle(&s,300);assert(!sbs_pages_offset(&s));
    /* Re-grab while settling keeps the newest logical position, without a tap. */
    touch(&s,400,400,136,true,true,false,false);touch(&s,500,200,136,false,true,false,false);
    touch(&s,520,200,136,false,false,true,true);touch(&s,560,0,0,false,false,false,false);
    int latest=sbs_pages_offset(&s);assert(latest>200&&latest<480);
    touch(&s,580,100,136,true,true,false,false);assert(sbs_pages_offset(&s)==latest);
    assert(!(touch(&s,600,100,136,false,false,true,true)&PORTABLE_SCROLL_TAP));settle(&s,800);
    /* A queued UP supplies final coordinates; snapshots use the last held position. */
    s=model();touch(&s,0,400,136,true,true,false,false);touch(&s,20,380,136,false,true,false,false);
    touch(&s,40,220,136,false,false,true,true);assert(sbs_pages_offset(&s)==180);settle(&s,220);assert(sbs_pages_offset(&s)==480);
    s=model();touch(&s,UINT32_MAX-50,400,136,true,true,false,false);touch(&s,20,240,136,false,true,false,false);
    touch(&s,40,400,136,false,false,true,false);settle(&s,220);assert(sbs_pages_offset(&s)==480);
    /* A simple tap remains a tap; an empty/one-page catalog stays bounded. */
    s=model();touch(&s,0,110,136,true,true,false,false);
    assert(touch(&s,20,110,136,false,false,true,true)&PORTABLE_SCROLL_TAP);
    sbs_pages_configure(&s,s.view,480,1);touch(&s,40,400,136,true,true,false,false);
    touch(&s,60,100,136,false,true,false,false);touch(&s,80,100,136,false,false,true,true);settle(&s,260);assert(!sbs_pages_offset(&s));
    puts("Springboard horizontal-page model: drag, snap, bounds, reverse, cancellation and wraparound time passed");
}
