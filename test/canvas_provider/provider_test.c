#include <assert.h>
#include <stdio.h>
#include "canvas_provider.h"
static uint32_t physical_width=8;
static bool info(void*x,risc_display_info_v1*out){(void)x;*out=(risc_display_info_v1){.width=physical_width,.height=8,.supported_formats=1};return true;}
static bool acquire(void*x,uint32_t f,risc_display_surface_v1*out){(void)x;(void)f;(void)out;return false;}
static void release(void*x,uint64_t f){(void)x;(void)f;}
static bool submit(void*x,uint64_t f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)x;(void)f;(void)r;(void)n;(void)o;(void)t;return false;}
static bool status(void*x,uint64_t t,risc_display_present_status_v1*out){(void)x;(void)t;(void)out;return false;}
static risc_touch_snapshot_v1 touch_snapshot={.width=8,.height=8};
static risc_touch_event_v1 queued;
static int queued_result;
static uint64_t subscribe(void*x){(void)x;return 1;}
static bool unsubscribe(void*x,uint64_t id){(void)x;return id==1;}
static bool touch_poll(void*x,size_t count){(void)x;(void)count;return true;}
static int32_t touch_next(void*x,uint64_t sub,risc_touch_event_v1*out){(void)x;assert(sub==1);*out=queued;int rc=queued_result;queued_result=0;return rc;}
static bool snapshot(void*x,risc_touch_snapshot_v1*out){(void)x;*out=touch_snapshot;return true;}
static unsigned claims,resets;
static risc_input_navigation_frame_v1 nav_sample;
static bool nav_poll(void*x,risc_input_navigation_frame_v1*out){(void)x;*out=nav_sample;nav_sample.pressed=nav_sample.released=0;return true;}
static bool foreground(void*x,const risc_input_foreground_v1*c,size_t count){(void)x;if(count){assert(count==1&&!strcmp(c[0].capability,"input.touch.raw"));claims++;}else if(claims)claims--;return true;}
static bool reset(void*x){(void)x;resets++;memset(&nav_sample,0,sizeof(nav_sample));return true;}
int main(void){
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};canvas_provider p;canvas_provider_init(&p,&d,NULL,0);
 const risc_canvas_api_v1*a=risc_canvas_get_v1(&p.api,sizeof(p.api));assert(a);
 uint64_t s=999;risc_canvas_open_v1 r={sizeof(r),RISC_CANVAS_RGBA8,RISC_CANVAS_RESPONSIVE,0,0};assert(a->open(a->context,&r,&s)==RISC_CANVAS_UNSUPPORTED&&s==999);
 r.encoding=RISC_CANVAS_MONO1;assert(!a->open(a->context,&r,&s));
 risc_canvas_geometry_v1 g={.struct_size=sizeof(g)};assert(!a->geometry(a->context,s,&g)&&g.width==8&&g.height==8);
 risc_canvas_event_v1 e={.struct_size=sizeof(e)};assert(!a->next(a->context,s,&e)&&e.kind==RISC_CANVAS_INPUT_GAP);
 size_t count=9;uint64_t seq=0;assert(!a->contacts(a->context,s,NULL,0,&count,&seq)&&count==0&&seq>e.sequence);
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};assert(!a->begin_frame(a->context,s,&f));assert(a->close(a->context,s)==RISC_CANVAS_BUSY);assert(!a->cancel_frame(a->context,s,f.lease));assert(!a->quiesce(a->context,s));assert(a->begin_frame(a->context,s,&f)==RISC_CANVAS_BUSY);assert(!a->resume(a->context,s));assert(!a->close(a->context,s));assert(a->geometry(a->context,s,&g)==RISC_CANVAS_STALE);
 risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
 canvas_provider_init(&p,&d,&touch,0);a=&p.api;r.layout=RISC_CANVAS_FIT;r.logical_width=8;r.logical_height=4;
 assert(!a->open(a->context,&r,&s));assert(!a->contacts(a->context,s,NULL,0,&count,&seq));
 queued=(risc_touch_event_v1){.sequence=1,.kind=RISC_TOUCH_EVENT_DOWN,.id=200,.x=4,.y=3};queued_result=1;
 assert(!a->next(a->context,s,&e));assert(e.contact==200&&e.x==4&&e.y==1&&e.phase==RISC_CANVAS_DOWN);
 queued=(risc_touch_event_v1){.sequence=2,.kind=RISC_TOUCH_EVENT_MOVE,.id=200,.x=4,.y=0};queued_result=1;
 assert(!a->next(a->context,s,&e));assert(e.y==-2); /* captured drag outside letterbox */
 queued_result=-1;assert(!a->next(a->context,s,&e)&&e.kind==RISC_CANVAS_INPUT_GAP);assert(a->next(a->context,s,&e)==RISC_CANVAS_AGAIN);
 touch_snapshot.sequence=3;touch_snapshot.contact_count=1;touch_snapshot.contacts[0]=(risc_touch_contact_v1){.id=200,.x=4,.y=0};
 uint64_t oldseq=seq;count=0;assert(a->contacts(a->context,s,NULL,0,&count,&seq)==RISC_CANVAS_AGAIN&&count==1&&seq==oldseq);
 risc_canvas_event_v1 contact={.struct_size=sizeof(contact)};assert(!a->contacts(a->context,s,&contact,1,&count,&seq));assert(contact.contact==200&&contact.y==-2&&contact.sequence==seq);
 queued=(risc_touch_event_v1){.sequence=4,.kind=RISC_TOUCH_EVENT_UP,.id=200,.x=4,.y=0};queued_result=1;
 assert(!a->next(a->context,s,&e)&&e.phase==RISC_CANVAS_UP&&e.sequence>seq);
 queued=(risc_touch_event_v1){.sequence=5,.kind=RISC_TOUCH_EVENT_DOWN,.id=201,.x=4,.y=0};queued_result=1;assert(a->next(a->context,s,&e)==RISC_CANVAS_IDLE);
 assert(!a->close(a->context,s));
 assert(!a->open(a->context,&r,&s));assert(!a->next(a->context,s,&e)&&e.kind==RISC_CANVAS_INPUT_GAP);
 assert(!a->close(a->context,s));
 risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,foreground,reset};
 canvas_provider_init(&p,&d,&touch,0);p.navigation=&nav;a=&p.api;touch_snapshot.contact_count=0;assert(!a->open(a->context,&r,&s));assert(claims==1);
 assert(!a->contacts(a->context,s,NULL,0,&count,&seq));nav_sample.pressed=RISC_NAV_BACK;assert(!a->next(a->context,s,&e)&&e.kind==RISC_CANVAS_NAVIGATION&&e.navigation.pressed==RISC_NAV_BACK);
 assert(!a->quiesce(a->context,s));assert(claims==0&&!p.subscription);assert(!a->resume(a->context,s));assert(claims==1&&p.subscription);assert(!a->close(a->context,s));assert(claims==0&&resets>=4);
 canvas_provider_init(&p,&d,NULL,0);a=&p.api;r.layout=RISC_CANVAS_RESPONSIVE;r.logical_width=r.logical_height=0;
 assert(!a->open(a->context,&r,&s));assert(!a->contacts(a->context,s,NULL,0,&count,&seq));assert(!a->geometry(a->context,s,&g));uint32_t oldrevision=g.revision;
 physical_width=10;assert(!a->next(a->context,s,&e)&&e.kind==RISC_CANVAS_GEOMETRY_CHANGED&&e.geometry_revision>oldrevision);assert(!a->geometry(a->context,s,&g)&&g.width==10);assert(!a->close(a->context,s));
 printf("Capability-table open/geometry/no-touch/input-cut/quiesce/close checks passed\n");
 return 0;
}
