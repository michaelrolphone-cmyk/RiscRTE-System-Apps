#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneReadingLinksV1.h"
static void tap_expect(int x,int y,unsigned node,unsigned action,int value){risc_scene_event_v1 e;touch_tap(x,y);assert(tick(&e)==RISC_SCENE_OK);assert(e.node==node&&e.action==action&&e.value==value);settle();}
int main(void){
 w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};startup();
 const risc_scene_reading_links_api_v1* p=risc_scene_reading_links_get_v1(api);assert(p);
 risc_scene_reading_api_v1 old=p->reading;old.checkpoint.presentation.page.components.lifecycle.base.struct_size=sizeof(old);assert(!risc_scene_reading_links_get_v1(&old.checkpoint.presentation.page.components.lifecycle.base));
 risc_scene_reading_document_v1 r={.struct_size=sizeof(r),.base={.struct_size=sizeof(r.base),.revision=2,.previous_action=71,.menu_action=72,.next_action=73,.back_action=74},.bookmark_action=75,.layout_action=76,.contents_action=77,.scrub_action=78,.scroll_action=79};
 risc_scene_reading_links_v1 l={.struct_size=sizeof(l),.action=80,.count=3,.items={{10,100,80,30},{220,100,80,30},{400,100,70,30}}};
 size_t n=480*680/8;uint8_t* b=malloc(n);memset(b,255,n);
 assert(!p->present_linked(NULL,session,&r,&l,b,n,1));expected_intent=RISC_DISPLAY_PRESENT_CLEAN;settle();expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 memset(l.items,0,sizeof(l.items));tap_expect(30,160,9,80,0);tap_expect(250,160,9,80,1);tap_expect(430,160,9,80,2); // copied, not borrowed
 tap_expect(30,300,1,71,0);tap_expect(240,300,2,72,0);tap_expect(440,300,3,73,0);
 unsigned count=submits;int rc;while((rc=p->reading.checkpoint.pause(NULL,session))==RISC_SCENE_AGAIN)ms+=10;assert(!rc);assert(!p->reading.checkpoint.resume(NULL,session,0));settle();assert(submits==count);tap_expect(30,160,9,80,0);
 l.items[0]=(risc_scene_reading_link_v1){65530,0,20,20};l.count=1;r.base.revision++;assert(p->present_linked(NULL,session,&r,&l,NULL,0,0)==RISC_SCENE_INVALID);tap_expect(250,160,9,80,1);
 l.items[0]=(risc_scene_reading_link_v1){10,0,450,680};r.flags=RISC_SCENE_READING_CHROME;
 assert(!p->present_linked(NULL,session,&r,&l,NULL,0,0));settle();tap_expect(424,56,5,76,0);tap_expect(120,740,6,77,0);tap_expect(250,160,9,80,0);tap_expect(200,80,2,72,0); // hidden text cannot steal toolbar taps
 r.base.revision++;r.flags=RISC_SCENE_READING_SCROLL;assert(!p->present_linked(NULL,session,&r,&l,NULL,0,0));settle();
 event_at=0;event_count=3;events[0]=(risc_touch_event_v1){++seq,ms,1,0,240,450};events[1]=(risc_touch_event_v1){++seq,ms+20,2,0,240,360};events[2]=(risc_touch_event_v1){++seq,ms+40,3,0,240,300};risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.node==8&&e.action==79&&e.value==1174);settle();
 // A new revision invalidates a held contact; it cannot select the next page's link.
 event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms,1,0,240,160};assert(tick(&e)==RISC_SCENE_IDLE);
 r.base.revision++;assert(!p->present_linked(NULL,session,&r,&l,NULL,0,0));event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms+20,3,0,240,160};assert(tick(&e)==RISC_SCENE_IDLE);settle();
 r.base.revision++;r.flags=0;assert(!p->reading.present_reading(NULL,session,&r,NULL,0,0));settle();tap_expect(30,160,1,71,0); // legacy presentation clears stale links
 free(b);finish();puts("Reading link suffix: copied bounds, edge override, chrome precedence, swipe arbitration, stale-contact fence, checkpoint retention and legacy compatibility PASS");
}
