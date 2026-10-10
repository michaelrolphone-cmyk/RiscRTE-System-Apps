#include "RiscSceneV1.h"
#include "RiscSceneStateV1.h"
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "SceneProfileV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t pixels[800*480*2+32];
static unsigned w=240,h=240,format=5,calls,submits,releases,polls,unsubscribes;
static uint64_t ms,seq;
static bool held,presenting,subscribed,allow_complete=true,bad_status,bad_snapshot,bad_unsubscribe,bad_poll,queue_gap,driver_fault,held_input;
static int busy_acquires;
static bool supersede_frame;
static unsigned snapshot_contacts,delayed_x,delayed_y;
static bool delayed_tap;
static risc_touch_event_v1 events[40];static unsigned event_count,event_at;
static uint32_t nav_pressed,display_flags,expected_intent,expected_queue;
static bool native_alive=true;
static const char *loss_callback;
static unsigned loss_skip;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){
    static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime)};
    return version==1&&native_alive?&runtime:NULL;
}
static void io(const char *name){
    assert(native_alive&&"provider I/O after native retention");++calls;
    if(loss_callback&&!strcmp(loss_callback,name)){if(loss_skip)--loss_skip;else native_alive=false;}
}
static bool get_info(void *c,risc_display_info_v1 *out){(void)c;io(__func__);*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=w,.height=h,.supported_formats=RISC_DISPLAY_FORMAT_BIT(format),.preferred_format=format,.flags=display_flags};return true;}
static bool acquire(void *c,uint32_t f,risc_display_surface_v1 *out){
    (void)c;io(__func__);assert(f==format);
    if(busy_acquires){--busy_acquires;return false;}
    assert(!held&&!presenting);held=true;memset(pixels,0xa5,sizeof(pixels));
    unsigned bits=f==5?16:f==4?8:f==3?4:f==2?2:1;
    uint32_t stride=(w*bits+7)/8;
    *out=(risc_display_surface_v1){submits+1,pixels+16,w,h,stride,stride*h,f};return true;
}
static void check_guards(void){
    for(unsigned i=0;i<16;i++)assert(pixels[i]==0xa5);
    uint32_t bits=format==5?16:format==4?8:format==3?4:format==2?2:1,bytes=(w*bits+7)/8*h;
    for(unsigned i=16+bytes;i<32+bytes;i++)assert(pixels[i]==0xa5);
}
static void release(void *c,uint64_t frame){(void)c;io(__func__);assert(frame&&held&&!presenting);check_guards();held=false;++releases;}
static bool submit(void *c,uint64_t f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){
    (void)c;(void)d;io(__func__);assert(held&&f&&n==0&&o->queue_policy==expected_queue&&o->intent==expected_intent);check_guards();held=false;presenting=true;*t=++submits;return true;
}
static bool status(void *c,uint64_t t,risc_display_present_status_v1*out){(void)c;io(__func__);assert(!held&&presenting&&t==submits);if(bad_status)return false;out->state=allow_complete?(supersede_frame?RISC_DISPLAY_PRESENT_SUPERSEDED:RISC_DISPLAY_PRESENT_COMPLETE):RISC_DISPLAY_PRESENT_ACTIVE;if(allow_complete){presenting=false;supersede_frame=false;}return true;}
static uint64_t subscribe(void*c){(void)c;io(__func__);assert(!subscribed);subscribed=true;return 42;}
static bool unsubscribe(void*c,uint64_t s){(void)c;io(__func__);assert(subscribed&&s==42);++unsubscribes;if(bad_unsubscribe)return false;subscribed=false;return true;}
static bool poll(void*c,size_t n){(void)c;io(__func__);assert(n<=2);++polls;ms+=10;
    if(delayed_tap){
        delayed_tap=false;event_at=0;event_count=2;
        events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)delayed_x,(uint16_t)delayed_y};
        events[1]=(risc_touch_event_v1){++seq,ms+20,3,0,(uint16_t)delayed_x,(uint16_t)delayed_y};
    }
    return !bad_poll;
}
static int32_t next(void*c,uint64_t s,risc_touch_event_v1*out){(void)c;io(__func__);assert(s==42&&subscribed);if(driver_fault)return -2;if(queue_gap){queue_gap=false;return -1;}if(event_at==event_count)return 0;*out=events[event_at++];return 1;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;io(__func__);if(bad_snapshot)return false;
    *out=(risc_touch_snapshot_v1){.sequence=seq,.timestamp_ms=ms,.width=(uint16_t)(w==800?h:w),.height=(uint16_t)(w==800?w:h),.contact_count=held_input?1:(uint8_t)snapshot_contacts};
    for(unsigned i=0;i<out->contact_count;i++)out->contacts[i].id=(uint8_t)i;
    return true;
}
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){(void)c;io(__func__);*out=(risc_input_navigation_frame_v1){.pressed=nav_pressed};nav_pressed=0;return true;}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;io(__func__);assert(n<=1);if(n)assert(!strcmp(f[0].capability,"input.touch.raw"));return true;}
static bool reset(void*c){(void)c;io(__func__);nav_pressed=0;return true;}
static uint64_t now(void*c){(void)c;io(__func__);return ms;}
static void sleep_ms(void*c,uint32_t n){(void)c;(void)n;assert(0&&"presenter must not sleep");}
static const risc_display_output_api_v1 disp={1,sizeof(disp),NULL,get_info,acquire,release,submit,status,NULL,NULL};
static const risc_touch_api_v1 raw={1,sizeof(raw),NULL,subscribe,unsubscribe,poll,next,snapshot};
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,foreground,reset};
static const risc_platform_clock_api_v1 clk={1,sizeof(clk),NULL,now,sleep_ms};
static risc_scene_profile_v1 prof={1,sizeof(prof),5,2,42,8,0xffff,0x0841,0x05ff,0,0,0};
static risc_scene_document_v1 doc;
static uint64_t session;
static const risc_scene_api_v1 *api;
static const risc_driver_v2 *driver;
static int tick(risc_scene_event_v1*out){*out=(risc_scene_event_v1){.struct_size=sizeof(*out)};return api->next(NULL,session,out);}
static void settle(void){risc_scene_event_v1 e;for(unsigned i=0;i<3;i++)assert(tick(&e)==RISC_SCENE_IDLE);}
static void touch_tap(unsigned x,unsigned y){event_at=0;event_count=2;events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)x,(uint16_t)y};events[1]=(risc_touch_event_v1){++seq,ms+20,3,0,(uint16_t)x,(uint16_t)y};}
static void make_document(void){
    doc=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(doc),.revision=1,.root=1,.route_count=2,.node_count=6};
    doc.routes[0]=(risc_scene_route_v1){1,0,"ALARMS"};doc.routes[1]=(risc_scene_route_v1){2,9,"EDIT"};
    doc.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE,.label="ONE-SHOT ALARM",.text="READY"};
    doc.nodes[1]=(risc_scene_node_v1){.id=2,.route=1,.kind=RISC_SCENE_LINK,.target=2,.label="EDIT TIME"};
    doc.nodes[2]=(risc_scene_node_v1){.id=3,.route=1,.kind=RISC_SCENE_ACTION,.action=20,.label="ARM NEXT"};
    doc.nodes[3]=(risc_scene_node_v1){.id=4,.route=2,.kind=RISC_SCENE_TIME_OF_DAY,.action=21,.value=390,.minimum=0,.maximum=1439,.step=1,.label="TIME OF DAY"};
    doc.nodes[4]=(risc_scene_node_v1){.id=5,.route=2,.kind=RISC_SCENE_ACTION,.action=22,.label="SAVE"};
    doc.nodes[5]=(risc_scene_node_v1){.id=6,.route=2,.kind=RISC_SCENE_ACTION,.action=9,.label="DISCARD"};
}
static void startup(void){
    driver=t5_driver_get(2);assert(driver&&!t5_driver_get(1));api=driver->capability;
    risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
    unsigned before=calls;assert(driver->start(deps,5));assert(calls==before);assert(driver->quiesce());
    make_document();assert(api->open(NULL,&doc,NULL,&session)==RISC_SCENE_OK);assert(session);
}
static void finish(void){assert(api->close(NULL,session)==RISC_SCENE_OK);assert(!held&&!presenting&&!subscribed&&driver->quiesce());driver->stop();}
static void checkpoint_test(void){
    risc_scene_navigation_v1 n={.api_version=1,.struct_size=sizeof(n),.depth=2,.routes={1,2},.focus={2,4}},out;
    uint8_t encoded[256],payload[3]={1,2,3},copy[3];size_t size=0,read=0;
    assert(risc_scene_state_encode(encoded,sizeof(encoded),7,&n,payload,3,&size));
    assert(size==99&&risc_scene_state_decode(encoded,size,7,&out,copy,3,&read)&&read==3&&!memcmp(copy,payload,3)&&!memcmp(&n,&out,sizeof(n)));
    for(size_t i=0;i<size;i++){encoded[i]^=1;assert(!risc_scene_state_decode(encoded,size,7,&out,copy,3,&read));encoded[i]^=1;}
    assert(!risc_scene_state_decode(encoded,size-1,7,&out,copy,3,&read));assert(!risc_scene_state_decode(encoded,size,8,&out,copy,3,&read));
    n.depth=9;assert(!risc_scene_state_encode(encoded,sizeof(encoded),7,&n,payload,3,&size));
}
static void validation(void){
    unsigned before=calls;uint64_t other=0;
    for(unsigned mode=0;mode<12;mode++){
        risc_scene_document_v1 bad=doc;
        switch(mode){case 0:bad.struct_size--;break;case 1:bad.api_version++;break;case 2:bad.route_count=9;break;case 3:bad.node_count=25;break;case 4:bad.root=33;break;case 5:bad.nodes[0].id=2;break;case 6:bad.nodes[3].value=1440;break;case 7:memset(bad.nodes[0].label,'x',40);break;case 8:bad.nodes[1].target=19;break;case 9:bad.nodes[3].step=0;break;case 10:bad.reserved[0]=1;break;case 11:bad.routes[1].id=1;break;}
        assert(api->open(NULL,&bad,NULL,&other)==RISC_SCENE_INVALID);assert(calls==before);
    }
    assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_BUSY);assert(api->update(NULL,session,&doc)==RISC_SCENE_STALE);
    assert(api->close(NULL,session+1)==RISC_SCENE_STALE);
}
static void behavior(const char*output){
    settle();unsigned logical_w=w==800?h:w,top=prof.font_scale*7+prof.padding*2+8;
    touch_tap(logical_w/2,top+prof.row_height+prof.row_height/2);
    risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    risc_scene_navigation_v1 saved={.struct_size=sizeof(saved)};uint32_t flags;
    assert(api->snapshot(NULL,session,&saved,&flags)==0&&saved.depth==2&&saved.routes[1]==2);
    /* Interaction is generated entirely by the class presenter. */
    unsigned field_height=2*prof.row_height,by=top+field_height-prof.font_scale*7-12;
    touch_tap(prof.padding+(logical_w-2*prof.padding)*3/8,by+8);
    assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_VALUE_EVENT&&e.action==21&&e.node==4&&e.value==450);
    assert(api->snapshot(NULL,session,&saved,&flags)==0&&saved.focus[1]==4);
    doc.nodes[3].value=e.value;doc.revision++;
    assert(api->update(NULL,session,&doc)==0);settle();
    if(output){FILE*f=fopen(output,"wb");assert(f);unsigned bits=format==5?16:format==4?8:format==3?4:format==2?2:1;assert(fwrite(pixels+16,1,(w*bits+7)/8*h,f)==(w*bits+7)/8*h);fclose(f);}
    uint64_t old=session;finish();
    risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
    assert(driver->start(deps,5));assert(api->open(NULL,&doc,&saved,&session)==0&&session!=old);assert(api->update(NULL,old,&doc)==RISC_SCENE_STALE);settle();
    nav_pressed=RISC_NAV_BACK;assert(tick(&e)==RISC_SCENE_OK&&e.action==9); /* app decides discard vs preserve */
    nav_pressed=RISC_NAV_HOME;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
    finish();assert(!releases);
}
static void native_retention(const char *mode){
    driver=t5_driver_get(2);assert(driver);api=driver->capability;
    risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
    assert(driver->start(deps,5));make_document();
    const char *point=strchr(mode+7,'-');assert(point);++point;
    char callback[40];snprintf(callback,sizeof(callback),"%s",point);
    char *failure=strstr(callback,"-fail");if(failure){*failure=0;bad_snapshot=true;}
    if(!strncmp(mode,"native-open-",12)){
        loss_callback=callback;if(!strcmp(callback,"before"))native_alive=false;assert(api->open(NULL,&doc,NULL,&session)==RISC_SCENE_RETAINED);
    }else{
        assert(api->open(NULL,&doc,NULL,&session)==RISC_SCENE_OK);
        risc_scene_event_v1 e;
        if(!strcmp(callback,"status"))assert(tick(&e)==RISC_SCENE_IDLE);
        loss_callback=callback;if(!strcmp(callback,"before"))native_alive=false;
        if(!strncmp(mode,"native-close-",13))assert(api->close(NULL,session)==RISC_SCENE_RETAINED);
        else assert(tick(&e)==RISC_SCENE_RETAINED);
    }
    assert(!native_alive);unsigned before=calls;uint64_t other=0;uint32_t flags=0;
    risc_scene_event_v1 e={.struct_size=sizeof(e)};risc_scene_navigation_v1 path={.struct_size=sizeof(path)};
    /* Once latched, even an apparent native recovery must not resume I/O. */
    native_alive=true;
    assert(api->open(NULL,&doc,NULL,&other)==RISC_SCENE_RETAINED);
    assert(api->update(NULL,session,&doc)==RISC_SCENE_RETAINED);
    assert(api->next(NULL,session,&e)==RISC_SCENE_RETAINED);
    assert(api->navigate(NULL,session,RISC_SCENE_ROOT,0)==RISC_SCENE_RETAINED);
    assert(api->snapshot(NULL,session,&path,&flags)==RISC_SCENE_RETAINED);
    assert(api->close(NULL,session)==RISC_SCENE_RETAINED);
    assert(!driver->quiesce());driver->stop();assert(calls==before);
}
int main(int argc,char**argv){
    assert(argc>=3);const char*mode=argv[1];
    if(!strcmp(argv[2],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,90,0,0};}
    else if(!strcmp(argv[2],"gray")){w=480;h=800;format=3;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,0,0,0};}
    if(!strncmp(mode,"native-",7)){native_retention(mode);printf("scene host %s %s PASS (no I/O after retention)\n",mode,argv[2]);return 0;}
    checkpoint_test();startup();validation();
    if(!strcmp(mode,"behavior")){behavior(argc>3?argv[3]:NULL);}
    else if(!strcmp(mode,"inflight")){
        allow_complete=false;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);assert(!held&&presenting&&submits==1);
        unsigned before=polls;for(unsigned i=0;i<10;i++)assert(tick(&e)==RISC_SCENE_IDLE);assert(polls==before+10&&submits==1);
        assert(api->close(NULL,session)==RISC_SCENE_AGAIN&&!held&&presenting&&!driver->quiesce());
        assert(api->update(NULL,session,&doc)==RISC_SCENE_BUSY);allow_complete=true;finish();
    }else if(!strcmp(mode,"stale-frame")){
        settle();doc.revision++;doc.nodes[1].target=1;assert(api->update(NULL,session,&doc)==0);allow_complete=false;
        risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
        unsigned tw=w==800?h:w,top=prof.font_scale*7+prof.padding*2+8;
        touch_tap(tw/2,top+prof.row_height+10);assert(tick(&e)==RISC_SCENE_IDLE);
        risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags;assert(api->snapshot(NULL,session,&n,&flags)==0&&n.depth==1);
        allow_complete=true;settle();finish();
    }else if(!strcmp(mode,"gap")||!strcmp(mode,"transient")||!strcmp(mode,"held")){
        settle();unsigned tw=w==800?h:w,top=prof.font_scale*7+prof.padding*2+8;
        if(!strcmp(mode,"held")){held_input=true;queue_gap=true;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);}
        touch_tap(tw/2,top+prof.row_height+10);queue_gap=!strcmp(mode,"gap");bad_poll=!strcmp(mode,"transient");
        risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
        risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags;assert(api->snapshot(NULL,session,&n,&flags)==0&&n.depth==1);
        held_input=false;bad_poll=false;settle();touch_tap(tw/2,top+prof.row_height+10);assert(tick(&e)==RISC_SCENE_IDLE);settle();
        assert(api->snapshot(NULL,session,&n,&flags)==0&&n.depth==2);finish();
    }else if(!strcmp(mode,"busy-acquire")){
        busy_acquires=3;settle();assert(!held&&!submits);settle();assert(submits==1);finish();
    }else {
        settle();risc_scene_event_v1 e;
        if(!strcmp(mode,"status-fault")){doc.revision++;assert(api->update(NULL,session,&doc)==0);assert(tick(&e)==RISC_SCENE_IDLE);bad_status=true;assert(tick(&e)==RISC_SCENE_RETAINED);}
        else if(!strcmp(mode,"snapshot-fault")){bad_snapshot=true;assert(tick(&e)==RISC_SCENE_RETAINED);}
        else if(!strcmp(mode,"provider-fault")){driver_fault=true;assert(tick(&e)==RISC_SCENE_RETAINED);}
        else if(!strcmp(mode,"close-fault")){bad_unsubscribe=true;assert(api->close(NULL,session)==RISC_SCENE_RETAINED);}
        else assert(0);
        unsigned before=calls;assert(api->close(NULL,session)==RISC_SCENE_RETAINED);assert(tick(&e)==RISC_SCENE_RETAINED);assert(api->update(NULL,session,&doc)==RISC_SCENE_RETAINED);assert(!driver->quiesce());driver->stop();assert(calls==before);
    }
    printf("scene host %s %s PASS (frames=%u calls=%u)\n",mode,argv[2],submits,calls);return 0;
}
