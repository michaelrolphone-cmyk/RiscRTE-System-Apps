/* Additive lifecycle table and real presenter exercised with custody-checking
 * providers. The unconfigured legacy suite remains independently runnable. */
#define main legacy_scene_main
#include "host_test.c"
#undef main
#include "RiscSceneLifecycleV1.h"
#include "SceneKeyboardV1.h"

static const risc_scene_lifecycle_api_v1 *lifecycle;
static unsigned lw(void){return w==800?h:w;}
static unsigned lh(void){return w==800?w:h;}
static uint32_t flags_now(void){
    risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=UINT32_MAX;
    assert(api->snapshot(NULL,session,&n,&flags)==RISC_SCENE_OK);return flags&~RISC_SCENE_PRESENTING;
}
static void configure(uint32_t features){
    assert(lifecycle->configure(NULL,session,features)==RISC_SCENE_OK);
}
static void add_event(unsigned kind,unsigned x,unsigned y,unsigned id){
    assert(event_count<40);
    events[event_count]=(risc_touch_event_v1){++seq,ms+event_count*10,(uint8_t)kind,(uint8_t)id,(uint16_t)x,(uint16_t)y};
    ++event_count;
}
static void clear_events(void){event_count=event_at=0;}
static void gesture(unsigned x,unsigned start,unsigned end,bool release_touch){
    clear_events();add_event(RISC_TOUCH_EVENT_DOWN,x,start,0);
    add_event(RISC_TOUCH_EVENT_MOVE,x,end,0);
    if(release_touch)add_event(RISC_TOUCH_EVENT_UP,x,end,0);
    held_input=!release_touch;
}
static void release_touch(unsigned x,unsigned y){
    clear_events();add_event(RISC_TOUCH_EVENT_UP,x,y,0);held_input=false;
}
static void discover(void){
    assert(!risc_scene_lifecycle_get_v1(NULL));
    risc_scene_api_v1 legacy={.api_version=1,.struct_size=sizeof(legacy)};
    assert(!risc_scene_lifecycle_get_v1(&legacy));
    risc_scene_lifecycle_api_v1 copy=*lifecycle;
    for(size_t size=sizeof(legacy);size<sizeof(copy);size++){
        /* Actual short allocations make out-of-bounds discovery visible to
         * ASan, rather than merely advertising a shorter full-size object. */
        void *short_api=malloc(size);assert(short_api);memcpy(short_api,&copy,size);
        ((risc_scene_api_v1 *)short_api)->struct_size=(uint32_t)size;
        assert(!risc_scene_lifecycle_get_v1(short_api));free(short_api);
    }
    copy.base.api_version=2;assert(!risc_scene_lifecycle_get_v1(&copy.base));
    copy.base.api_version=1;copy.configure=NULL;assert(!risc_scene_lifecycle_get_v1(&copy.base));
    copy=*lifecycle;assert(risc_scene_lifecycle_get_v1(&copy.base)==&copy);
    unsigned before=calls;
    assert(lifecycle->configure(NULL,session+1,0)==RISC_SCENE_STALE);
    assert(lifecycle->configure(NULL,session,2)==RISC_SCENE_INVALID);
    assert(calls==before);
    settle();touch_tap(lw()/2,lh()/2);risc_scene_event_v1 e;(void)tick(&e);
    assert(!(flags_now()&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY)));
}
static void activity(void){
    settle();configure(0);assert(flags_now()&RISC_SCENE_INPUT_BUSY);settle();assert(!flags_now());
    clear_events();add_event(RISC_TOUCH_EVENT_DOWN,lw()/2,lh()/2,0);held_input=true;
    risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
    /* Invalid snapshot must not consume the activity latch. */
    risc_scene_navigation_v1 n={0};uint32_t f=0;
    assert(api->snapshot(NULL,session,&n,&f)==RISC_SCENE_INVALID);
    assert((flags_now()&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))==(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY));
    assert(flags_now()==RISC_SCENE_INPUT_BUSY);
    assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_INPUT_BUSY);
    clear_events();add_event(RISC_TOUCH_EVENT_MOVE,lw()/2+lw()/4,lh()/2,0);
    add_event(RISC_TOUCH_EVENT_UP,lw()/2+lw()/4,lh()/2,0);held_input=false;
    assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_ACTIVITY);assert(!flags_now());
    nav_pressed=nav_buttons=RISC_NAV_DOWN;assert(tick(&e)==RISC_SCENE_IDLE);
    assert((flags_now()&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))==(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY));
    settle();assert(flags_now()==RISC_SCENE_INPUT_BUSY);
    nav_buttons=0;nav_released=RISC_NAV_DOWN;assert(tick(&e)==RISC_SCENE_IDLE);
    assert(flags_now()==RISC_SCENE_ACTIVITY);assert(!flags_now());
    /* Configuration consumes queued and held inherited input, but the held
     * level still blocks idle until authoritative neutral input arrives. */
    touch_tap(lw()/2,10);held_input=true;nav_pressed=RISC_NAV_HOME;configure(0);
    assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_INPUT_BUSY);
    held_input=false;assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_ACTIVITY);
    assert(!flags_now());
    clear_events();add_event(RISC_TOUCH_EVENT_MOVE,10,10,0);assert(tick(&e)==RISC_SCENE_IDLE);
    configure(0);assert((flags_now()&12u)==12u);settle();assert(!flags_now());
}
static void input_busy(void){
    settle();configure(0);settle();risc_scene_event_v1 e;
    bad_poll=true;assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()&RISC_SCENE_INPUT_BUSY);
    bad_poll=false;assert(tick(&e)==RISC_SCENE_IDLE);assert(!(flags_now()&RISC_SCENE_INPUT_BUSY));
    queue_gap=true;assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()&RISC_SCENE_INPUT_BUSY);
    assert(tick(&e)==RISC_SCENE_IDLE);assert(!(flags_now()&RISC_SCENE_INPUT_BUSY));
    clear_events();for(unsigned i=0;i<20;i++)add_event(RISC_TOUCH_EVENT_MOVE,10,10,0);
    assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_ACTIVITY);
    assert(tick(&e)==RISC_SCENE_IDLE);assert(!(flags_now()&RISC_SCENE_INPUT_BUSY));
    snapshot_buttons=1;assert(tick(&e)==RISC_SCENE_IDLE);assert((flags_now()&12u)==12u);
    snapshot_buttons=0;assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_ACTIVITY);
    snapshot_contacts=2;assert(tick(&e)==RISC_SCENE_IDLE);assert((flags_now()&12u)==12u);
    snapshot_contacts=0;assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()==RISC_SCENE_ACTIVITY);
}
static void controls(void){
    settle();configure(RISC_SCENE_FEATURE_SHARED_CONTROLS);settle();
    unsigned x=lw()/2,y=lh()*24/800+2;
    gesture(x,1,y,false);risc_scene_event_v1 e;
    assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_CONTROLS_EVENT&&
           !e.node&&!e.action&&!e.value&&e.document_revision==doc.revision&&e.sequence==1);
    assert((flags_now()&12u)==12u);release_touch(x,y);assert(tick(&e)==RISC_SCENE_IDLE);
    /* No release, tap or Home event escapes the consumed modal boundary. */
    touch_tap(lw()-prof.padding-10,10);nav_pressed=RISC_NAV_HOME;
    assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()&RISC_SCENE_INPUT_BUSY);
    configure(RISC_SCENE_FEATURE_SHARED_CONTROLS);settle();assert(!flags_now());
    /* Normal header taps continue to work. */
    touch_tap(lw()-prof.padding-10,10);assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
    touch_tap(lw()-prof.padding-10,10);assert(tick(&e)==RISC_SCENE_IDLE);
    configure(0);settle();gesture(x,1,y,true);assert(tick(&e)==RISC_SCENE_IDLE);
    assert(!(flags_now()&RISC_SCENE_INPUT_BUSY));
}
static void cancellations(void){
    settle();configure(1);settle();risc_scene_event_v1 e;
    unsigned x=lw()/2,y=lh()*24/800+2;
    clear_events();add_event(1,x,1,0);add_event(2,x+lw()/4,1,0);add_event(2,x,y,0);add_event(3,x,y,0);
    assert(tick(&e)==RISC_SCENE_IDLE);
    gesture(x,lh()/3,lh()/2,true);assert(tick(&e)==RISC_SCENE_IDLE);
    clear_events();add_event(1,x,1,0);add_event(1,x,1,1);add_event(2,x,y,0);add_event(3,x,y,0);
    assert(tick(&e)==RISC_SCENE_IDLE);settle();
    /* The authoritative same-report snapshot includes a second finger even
     * when its DOWN follows the first finger's threshold-crossing MOVE. */
    clear_events();add_event(1,x,1,0);held_input=true;assert(tick(&e)==RISC_SCENE_IDLE);
    clear_events();add_event(2,x,y,0);add_event(1,x,1,1);
    events[1].timestamp_ms=events[0].timestamp_ms;held_input=false;snapshot_contacts=2;
    assert(tick(&e)==RISC_SCENE_IDLE);assert(flags_now()&RISC_SCENE_INPUT_BUSY);
    clear_events();add_event(3,x,y,0);add_event(3,x,1,1);snapshot_contacts=0;
    assert(tick(&e)==RISC_SCENE_IDLE);settle();
    clear_events();add_event(1,x,1,0);held_input=true;assert(tick(&e)==RISC_SCENE_IDLE);
    queue_gap=true;clear_events();add_event(2,x,y,0);assert(tick(&e)==RISC_SCENE_IDLE);
    release_touch(x,y);assert(tick(&e)==RISC_SCENE_IDLE);settle();
    clear_events();add_event(1,x,1,0);held_input=true;assert(tick(&e)==RISC_SCENE_IDLE);
    ++doc.revision;assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);
    clear_events();add_event(2,x,y,0);add_event(3,x,y,0);held_input=false;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    /* Current logical gestures remain active while paper is pending. */
    ++doc.revision;assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);allow_complete=false;begin_frame();
    gesture(x,1,y,true);assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_CONTROLS_EVENT);
    configure(1);allow_complete=true;settle();
    /* A genuine provider gap cancels recognition, independent of drain size. */
    gesture(x,1,y,true);for(unsigned i=0;i<16;i++)add_event(2,x,y,0);queue_gap=true;
    assert(tick(&e)==RISC_SCENE_IDLE);settle();assert(!(flags_now()&RISC_SCENE_INPUT_BUSY));
    gesture(x,1,y,true);assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_CONTROLS_EVENT);

}
static void report_order(void){
    settle();configure(1);settle();risc_scene_event_v1 e;
    unsigned x=lw()/2,y=lh()*24/800+2;
    /* A complete older Home tap survives a later ambiguous report. */
    clear_events();add_event(1,lw()-prof.padding-10,10,0);add_event(3,lw()-prof.padding-10,10,0);
    add_event(1,x,1,0);add_event(1,x,1,1);snapshot_contacts=2;
    assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
    snapshot_contacts=0;clear_events();configure(1);settle();
    /* An entire ambiguous report may already have lifted by snapshot time.
     * MOVE cannot escape before the second DOWN from its own report. */
    clear_events();add_event(1,x,1,0);add_event(2,x,y,0);add_event(1,x,1,1);
    events[2].timestamp_ms=events[1].timestamp_ms;
    add_event(3,x,y,0);add_event(3,x,1,1);
    assert(tick(&e)==RISC_SCENE_IDLE);
    /* Genuine neutral releases allow the next separate report immediately. */
    gesture(x,1,y,true);assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_CONTROLS_EVENT);
}
static void keyboard_suppression(void){
    settle();configure(1);
    doc=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(doc),.revision=2,.root=1,.route_count=1,.node_count=1};
    doc.routes[0]=(risc_scene_route_v1){1,0,"Text"};
    doc.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_KEYBOARD_NODE,.action=77,.maximum=3,.step=1,.label="Name"};
    assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;settle();
    gesture(lw()/2,1,lh()/4,true);risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
    assert(flags_now()==RISC_SCENE_ACTIVITY);
    nav_pressed=RISC_NAV_BACK;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_VALUE_EVENT&&e.value==RISC_SCENE_KEY_CANCEL);
    assert((flags_now()&12u)==12u);assert(flags_now()==RISC_SCENE_INPUT_BUSY);
    ++doc.revision;assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);settle();assert(!flags_now());
}
static void close_reopen(void){
    settle();configure(1);settle();gesture(lw()/2,1,lh()*24/800+2,false);
    risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_CONTROLS_EVENT);
    assert(api->close(NULL,session)==RISC_SCENE_OK);nav_pressed=RISC_NAV_HOME;
    assert(api->open(NULL,&doc,NULL,&session)==RISC_SCENE_OK);configure(1);settle();
    assert(flags_now()&RISC_SCENE_INPUT_BUSY);release_touch(lw()/2,lh()*24/800+2);
    assert(tick(&e)==RISC_SCENE_IDLE);settle();assert(!(flags_now()&RISC_SCENE_INPUT_BUSY));
    allow_complete=false;++doc.revision;assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);
    begin_frame();assert(api->close(NULL,session)==RISC_SCENE_AGAIN);
    assert(lifecycle->configure(NULL,session,1)==RISC_SCENE_BUSY);
    assert(flags_now()&RISC_SCENE_CLOSING);allow_complete=true;
}
static void configure_retention(const char *callback){
    settle();loss_callback=callback;
    if(!strcmp(callback,"before"))native_alive=false;
    if(!strcmp(callback,"snapshot-fail")){loss_callback="snapshot";bad_snapshot=true;}
    assert(lifecycle->configure(NULL,session,1)==RISC_SCENE_RETAINED);
    unsigned before=calls;native_alive=true;loss_callback=NULL;
    assert(lifecycle->configure(NULL,session,0)==RISC_SCENE_RETAINED);
    assert(api->close(NULL,session)==RISC_SCENE_RETAINED);risc_scene_event_v1 e;
    assert(tick(&e)==RISC_SCENE_RETAINED);driver->stop();assert(calls==before&&!driver->quiesce());
}
int main(int argc,char **argv){
    assert(argc==3);const char *mode=argv[1];
    if(!strcmp(argv[2],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
    else if(!strcmp(argv[2],"gray")){w=480;h=800;format=3;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,0,0,0};}
    startup();lifecycle=risc_scene_lifecycle_get_v1(api);assert(lifecycle);
    if(!strncmp(mode,"lifecycle-native-",17))configure_retention(mode+17);
    else {
        if(!strcmp(mode,"lifecycle-discovery"))discover();
        else if(!strcmp(mode,"lifecycle-activity"))activity();
        else if(!strcmp(mode,"lifecycle-busy"))input_busy();
        else if(!strcmp(mode,"lifecycle-controls"))controls();
        else if(!strcmp(mode,"lifecycle-cancellation"))cancellations();
        else if(!strcmp(mode,"lifecycle-report-order"))report_order();
        else if(!strcmp(mode,"lifecycle-keyboard"))keyboard_suppression();
        else if(!strcmp(mode,"lifecycle-close"))close_reopen();
        else assert(0);
        finish();
    }
    printf("scene host %s %s PASS\n",mode,argv[2]);return 0;
}
