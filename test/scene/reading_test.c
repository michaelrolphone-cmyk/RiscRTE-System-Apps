#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneReadingV1.h"
static void expect_tap(unsigned x,unsigned y,unsigned node,unsigned action,int value){
 risc_scene_event_v1 e;touch_tap(x,y);assert(tick(&e)==RISC_SCENE_OK);
 assert(e.node==node&&e.action==action);if(value>=0)assert(e.value==value);settle();
}
int main(void){
 w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};startup();
 const risc_scene_reading_api_v1* r=risc_scene_reading_get_v1(api);assert(r);
 risc_scene_checkpoint_api_v1 old=r->checkpoint;old.presentation.page.components.lifecycle.base.struct_size=sizeof(old);
 assert(!risc_scene_reading_get_v1(&old.presentation.page.components.lifecycle.base));
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};assert(!r->reading_geometry(NULL,&g)&&g.width==480&&g.height==680);
 size_t length=(size_t)g.stride*g.height;uint8_t* bitmap=malloc(length);memset(bitmap,255,length);bitmap[100*g.stride+10]=0;
 risc_scene_reading_document_v1 d={.struct_size=sizeof(d),.base={.struct_size=sizeof(d.base),.revision=2,.previous_action=71,.menu_action=72,.next_action=73,.back_action=74},.bookmark_action=75,.layout_action=76,.contents_action=77,.scrub_action=78,.scroll_action=79,.progress=450};strcpy(d.base.title,"IMMERSIVE DOCUMENT");strcpy(d.base.footer,"CH 1 - 1 / 20");strcpy(d.progress_text,"45% - ~20 MIN LEFT");
 assert(r->present_reading(NULL,session,&d,bitmap,length-1,0)==RISC_SCENE_INVALID);
 assert(!r->present_reading(NULL,session,&d,bitmap,length,1));expected_intent=RISC_DISPLAY_PRESENT_CLEAN;settle();
 memset(bitmap,0,length);assert(pixels[16+148*60+10]==255);assert(!pixels[16+300*60+10]);
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 expect_tap(40,400,1,71,-1);expect_tap(440,400,3,73,-1);expect_tap(240,400,2,72,-1);
 d.flags=RISC_SCENE_READING_CHROME;d.base.revision++;assert(!r->present_reading(NULL,session,&d,NULL,0,0));settle();
 expect_tap(354,56,4,75,-1);expect_tap(424,56,5,76,-1);expect_tap(120,740,6,77,-1);expect_tap(360,740,5,76,-1);
 for(unsigned i=0;i<10;i++){unsigned x=28+(370*i)/10+i*6+18;expect_tap(x,644,7,78,(int)i*100+50);}
 d.base.revision++;d.flags=RISC_SCENE_READING_SCROLL;assert(!r->present_reading(NULL,session,&d,NULL,0,0));settle();
 event_at=0;event_count=3;events[0]=(risc_touch_event_v1){++seq,ms,1,0,240,450};events[1]=(risc_touch_event_v1){++seq,ms+20,2,0,240,360};events[2]=(risc_touch_event_v1){++seq,ms+40,3,0,240,300};risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.node==8&&e.action==79&&e.value==1174);settle();
 unsigned count=submits;for(unsigned i=0;i<100;i++)assert(tick(&e)==RISC_SCENE_IDLE);assert(submits==count);
 int rc;while((rc=r->checkpoint.pause(NULL,session))==RISC_SCENE_AGAIN){ms+=10;}assert(!rc);assert(!r->checkpoint.resume(NULL,session,0));settle();assert(submits==count);
 d.base.revision++;d.flags|=RISC_SCENE_READING_INVERT;assert(!r->present_reading(NULL,session,&d,NULL,0,0));expected_intent=RISC_DISPLAY_PRESENT_CLEAN;settle();assert(pixels[16+300*60+10]==255);
 /* A preview is one copied component transaction, not an app-owned canvas. */
 risc_scene_page_geometry_v1 pg={.struct_size=sizeof(pg)};assert(!r->preview_geometry(NULL,&pg));assert(pg.width==368&&pg.height==120);
 risc_components_document_v1 c={.api_version=1,.struct_size=sizeof(c),.revision=++d.base.revision,.root=1,.route_count=1,.node_count=1,.screen_key=77};c.routes[0]=(risc_scene_route_v1){1,99,"TEXT & LAYOUT"};c.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_TEXT_PREVIEW};c.details[0].marker=6;
 size_t plen=(size_t)pg.stride*pg.height;memset(bitmap,255,plen);memset(bitmap,0,pg.stride*10);
 assert(r->present_preview(NULL,session,&c,99,bitmap,plen)==RISC_SCENE_INVALID);assert(r->present_preview(NULL,session,&c,1,bitmap,plen-1)==RISC_SCENE_INVALID);
 assert(!r->present_preview(NULL,session,&c,1,bitmap,plen));memset(bitmap,255,plen);expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;settle();
 assert(r->present_preview(NULL,session,&c,1,bitmap,plen)==RISC_SCENE_STALE);
 unsigned ink=0;for(unsigned y=110;y<300;y++)for(unsigned x=10;x<50;x++)ink+=pixels[16+y*60+x]!=0;assert(ink>200);
 free(bitmap);finish();puts("Immersive page: copied pixels, edge/center tap, chrome targets, ten scrub positions, scroll, idle checkpoint, inversion and preview PASS");
}
