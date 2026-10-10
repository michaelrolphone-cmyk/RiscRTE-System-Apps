/* Exercise the real shared presenter with the same custody-checking fake
 * providers as the original scene suite, never a keyboard implementation mock. */
#define main legacy_scene_main
#include "host_test.c"
#undef main
#include "SceneKeyboardV1.h"

static void keyboard_document(void){
    doc=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(doc),.revision=2,.root=1,.route_count=1,.node_count=1};
    doc.routes[0]=(risc_scene_route_v1){1,0,"Text"};
    doc.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_KEYBOARD_NODE,.action=77,
        .minimum=0,.maximum=3,.step=1,.label="Name",.text="Mixed aA !?"};
    assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);
    expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
}
static unsigned logical_width(void){return w==800?h:w;}
static unsigned logical_height(void){return w==800?w:h;}
/* Reference mockup centers: independent of presenter layout/raster code. */
static void key_center(unsigned row,unsigned column,unsigned *x,unsigned *y){
    static const unsigned xs[4][10]={
        {52,94,135,177,218,260,302,343,385,427},
        {73,114,156,198,239,281,322,364,406},
        {63,114,156,198,239,281,322,364,416},
        {52,198,343,406}
    };
    unsigned lh=logical_height();
    *x=xs[row][column]*logical_width()/480;
    *y=lh==240?(97+row*30)*lh/240:(412+row*92)*lh/800;
}
static bool installed_physical_input;
static void key_tap(unsigned row,unsigned column){
    unsigned x,y;key_center(row,column,&x,&y);
    if(installed_physical_input){
        /* Independent installed X4 contract, not the scene rotation formula:
         * portrait logical -> landscape panel -> GT911 portrait raw domain. */
        unsigned panel_x=y,panel_y=479-x;
        x=479-panel_y;y=panel_x;
    }
    touch_tap(x,y);
}
static void acknowledge(void){++doc.revision;assert(api->update(NULL,session,&doc)==RISC_SCENE_OK);settle();}
static void expect_key(unsigned key){
    risc_scene_event_v1 e;int result=tick(&e);if(result!=RISC_SCENE_OK)fprintf(stderr,"missing key %u layer %d revision %u result %d\n",key,doc.nodes[0].value,doc.revision,result);assert(result==RISC_SCENE_OK);
    if(e.value!=(int32_t)key)fprintf(stderr,"expected %u got %d layer %d revision %u\n",key,e.value,doc.nodes[0].value,doc.revision);
    assert(e.kind==RISC_SCENE_VALUE_EVENT&&e.action==77&&e.node==1&&e.value==(int32_t)key&&e.document_revision==doc.revision);
}
static void keyboard_touch(void){
    const char *rows[4][4]={
        {"QWERTYUIOP","ASDFGHJKL","\200ZXCVBNM\201",",\202.\203"},
        {"1234567890","-/:;()&@\"","\200!?+=#$%\201",",\202.\203"},
        {"qwertyuiop","asdfghjkl","\200zxcvbnm\201",",\202.\203"},
        {"[]{}<>\\|^_","`~'*-/;:\"","\200!?+=#$%\201",",\202.\203"}
    };
    bool reachable[127]={0};reachable[' ']=true;
    settle();
    for(unsigned layer=0;layer<4;layer++){
        doc.nodes[0].value=(int32_t)layer;acknowledge();
        for(unsigned row=0;row<4;row++)for(unsigned column=0;rows[layer][row][column];column++){
            unsigned key=(unsigned char)rows[layer][row][column];key_tap(row,column);expect_key(key);
            if(key<127)reachable[key]=true;
            /* A second tap cannot be queued against unacknowledged text. */
            key_tap(row,column);risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
            acknowledge();
        }
    }
    for(unsigned c=32;c<=126;c++)assert(reachable[c]);
    nav_pressed=RISC_NAV_BACK;expect_key(RISC_SCENE_KEY_CANCEL);acknowledge();
    unsigned top=prof.font_scale*7+prof.padding*2+8;
    touch_tap(prof.padding+10,top/2);expect_key(RISC_SCENE_KEY_CANCEL);acknowledge();
    nav_pressed=RISC_NAV_HOME;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
}
static void keyboard_navigation(void){
    settle();nav_pressed=RISC_NAV_CONFIRM;expect_key('Q');acknowledge();
    nav_pressed=RISC_NAV_RIGHT;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
    /* Do not execute confirm queued before its focus image becomes visible. */
    nav_pressed=RISC_NAV_CONFIRM;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    nav_pressed=RISC_NAV_CONFIRM;expect_key('W');acknowledge();
    nav_pressed=RISC_NAV_DOWN;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    nav_pressed=RISC_NAV_CONFIRM;expect_key('A');acknowledge();
    nav_pressed=RISC_NAV_RIGHT;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    nav_pressed=RISC_NAV_CONFIRM;expect_key('S');acknowledge();
    nav_pressed=RISC_NAV_UP;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    nav_pressed=RISC_NAV_CONFIRM;expect_key('W');acknowledge();
    nav_pressed=RISC_NAV_PAGE_FORWARD;expect_key(RISC_SCENE_KEY_LAYER);
    doc.nodes[0].value=1;acknowledge();nav_pressed=RISC_NAV_CONFIRM;expect_key('1');
}
static void keyboard_pending(void){
    allow_complete=false;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);assert(submits==1);
    key_tap(0,0);nav_pressed=RISC_NAV_CONFIRM;assert(tick(&e)==RISC_SCENE_IDLE);
    assert(submits==1);
    /* Even input first drained on completion predates the visible frame. */
    key_tap(0,0);nav_pressed=RISC_NAV_CONFIRM;allow_complete=true;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    /* This tap remains in the physical provider until poll() on the tick
     * that observes completion. A pre-poll snapshot cannot see it yet. */
    allow_complete=false;++doc.revision;assert(api->update(NULL,session,&doc)==0);
    assert(tick(&e)==RISC_SCENE_IDLE);
    key_center(0,0,&delayed_x,&delayed_y);delayed_tap=true;allow_complete=true;
    assert(tick(&e)==RISC_SCENE_IDLE&&!delayed_tap);settle();
    key_tap(0,0);expect_key('Q');acknowledge();
    allow_complete=false;doc.nodes[0].value=1;++doc.revision;assert(api->update(NULL,session,&doc)==0);
    assert(tick(&e)==RISC_SCENE_IDLE);
    doc.nodes[0].value=2;++doc.revision;assert(api->update(NULL,session,&doc)==0);
    key_tap(0,0);assert(tick(&e)==RISC_SCENE_IDLE);allow_complete=true;
    /* The stale layer 1 completion is never input-active. */
    key_tap(0,0);assert(tick(&e)==RISC_SCENE_IDLE);key_tap(0,0);assert(tick(&e)==RISC_SCENE_IDLE);settle();
    key_tap(0,0);expect_key('q');
}
static void keyboard_stale(void){
    settle();unsigned x,y;key_center(0,0,&x,&y);
    event_count=1;event_at=0;held_input=true;
    events[0]=(risc_touch_event_v1){++seq,ms,RISC_TOUCH_EVENT_DOWN,0,(uint16_t)x,(uint16_t)y};
    risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
    doc.nodes[0].value=2;acknowledge();
    event_at=0;event_count=1;held_input=false;
    events[0]=(risc_touch_event_v1){++seq,ms+20,RISC_TOUCH_EVENT_UP,0,(uint16_t)x,(uint16_t)y};
    assert(tick(&e)==RISC_SCENE_IDLE);settle();key_tap(0,0);expect_key('q');
}
static void assert_no_grid(void);
static void keyboard_hardware(void){
    settle();unsigned x,y;key_center(0,0,&x,&y);risc_scene_event_v1 e;
    event_count=1;event_at=0;held_input=true;
    events[0]=(risc_touch_event_v1){++seq,ms,RISC_TOUCH_EVENT_DOWN,0,(uint16_t)x,(uint16_t)y};
    assert(tick(&e)==RISC_SCENE_IDLE);doc.nodes[0].flags=RISC_SCENE_DISABLED;acknowledge();
    event_at=0;event_count=1;held_input=false;
    events[0]=(risc_touch_event_v1){++seq,ms+20,RISC_TOUCH_EVENT_UP,0,(uint16_t)x,(uint16_t)y};
    assert(tick(&e)==RISC_SCENE_IDLE);settle();assert_no_grid();
    key_tap(0,0);assert(tick(&e)==RISC_SCENE_IDLE);
    nav_pressed=RISC_NAV_RIGHT;assert(tick(&e)==RISC_SCENE_IDLE);
    nav_pressed=RISC_NAV_PAGE_FORWARD;assert(tick(&e)==RISC_SCENE_IDLE);
    nav_pressed=RISC_NAV_CONFIRM;expect_key(RISC_SCENE_KEY_DONE);acknowledge();
    nav_pressed=RISC_NAV_BACK;expect_key(RISC_SCENE_KEY_CANCEL);acknowledge();
    /* Detach also requires a newly visible grid and a fresh contact. */
    held_input=true;doc.nodes[0].flags=0;acknowledge();
    event_at=0;event_count=1;held_input=false;
    events[0]=(risc_touch_event_v1){++seq,ms+20,RISC_TOUCH_EVENT_UP,0,(uint16_t)x,(uint16_t)y};
    assert(tick(&e)==RISC_SCENE_IDLE);settle();key_tap(0,0);expect_key('Q');
}
/* Check actual raster bytes, including rotation and packed monochrome/gray. */
static unsigned pixel_level(unsigned x,unsigned y){
    unsigned px=x,py=y;
    if(prof.display_rotation==90){px=w-1-y;py=x;}
    else if(prof.display_rotation==270){px=y;py=h-1-x;}
    unsigned bits=format==5?16:format==4?8:format==3?4:format==2?2:1;
    unsigned stride=(w*bits+7)/8;const uint8_t *row=pixels+16+py*stride;
    if(format==5)return row[px*2]|(unsigned)row[px*2+1]<<8;
    return (row[px*bits/8]>>(8-bits-(px*bits%8)))&((1u<<bits)-1);
}
static unsigned background_level(void){return format==5?prof.background_rgb565:0;}
static void assert_no_grid(void){
    unsigned y=logical_height()==240?174:648;
    for(unsigned x=prof.padding;x<logical_width()-prof.padding;x++)assert(pixel_level(x,y)==background_level());
}
#include "../../Services/scene_host/glyphs.inc"
static void keyboard_raster(const char *output){
    settle();
    assert(!memcmp(glyphs[0],(uint8_t[5]){0},5));
    for(unsigned c=33;c<=126;c++){
        assert(memcmp(glyphs[c-32],(uint8_t[5]){0},5));
        if(c>='a'&&c<='z')assert(memcmp(glyphs[c-32],glyphs[c-'a'+'A'-32],5));
    }
    for(unsigned c=32;c<=126;c++){
        doc.nodes[0].text[0]=(char)c;doc.nodes[0].text[1]=0;acknowledge();
        unsigned x=prof.padding,y=prof.font_scale*7+prof.padding*2+8+prof.font_scale*8+4;
        for(unsigned col=0;col<5;col++)for(unsigned row=0;row<7;row++){
            bool ink=(glyphs[c-32][col]&(1u<<row))!=0;
            assert((pixel_level(x+col*prof.font_scale,y+row*prof.font_scale)!=background_level())==ink);
        }
    }
    strcpy(doc.nodes[0].text,"Aa !? @# $% &* () [] {} ~^");doc.nodes[0].value=2;acknowledge();
    if(output){
        unsigned bits=format==5?16:format==4?8:format==3?4:format==2?2:1;
        FILE *f=fopen(output,"wb");assert(f);assert(fwrite(pixels+16,1,(w*bits+7)/8*h,f)==(w*bits+7)/8*h);fclose(f);
    }
}
static void keyboard_invalid(void){
    unsigned before=calls;uint64_t other=0;
    for(unsigned mode=0;mode<12;mode++){
        risc_scene_document_v1 bad=doc;
        switch(mode){
        case 0:bad.nodes[0].value=-1;break;case 1:bad.nodes[0].value=4;break;
        case 2:bad.nodes[0].minimum=1;break;case 3:bad.nodes[0].maximum=4;break;
        case 4:bad.nodes[0].step=2;break;case 5:bad.nodes[0].action=0;break;
        case 6:bad.nodes[0].target=1;break;case 7:bad.nodes[0].text[0]='\n';break;
        case 8:bad.nodes[0].text[0]=(char)0x80;break;case 9:bad.nodes[0].kind=8;break;
        case 10:memset(bad.nodes[0].text,'a',sizeof(bad.nodes[0].text));break;
        case 11:bad.node_count=2;bad.nodes[1]=bad.nodes[0];bad.nodes[1].id=2;break;
        }
        assert(api->open(NULL,&bad,NULL,&other)==RISC_SCENE_INVALID);
        ++bad.revision;assert(api->update(NULL,session,&bad)==RISC_SCENE_INVALID);assert(calls==before);
    }
    settle();key_tap(0,0);expect_key('Q');
}
static void keyboard_gesture(void){
    settle();unsigned x,y;key_center(0,0,&x,&y);risc_scene_event_v1 e;
    /* A down in the gap cannot release onto a neighboring key. */
    unsigned outside=logical_width()==240?16:33;
    event_count=2;event_at=0;
    events[0]=(risc_touch_event_v1){++seq,ms,RISC_TOUCH_EVENT_DOWN,0,(uint16_t)outside,(uint16_t)y};
    events[1]=(risc_touch_event_v1){++seq,ms+20,RISC_TOUCH_EVENT_UP,0,(uint16_t)x,(uint16_t)y};
    assert(tick(&e)==RISC_SCENE_IDLE);
    /* Moving outside and back never rearms the same contact. */
    event_count=3;event_at=0;
    events[0]=(risc_touch_event_v1){++seq,ms,RISC_TOUCH_EVENT_DOWN,0,(uint16_t)x,(uint16_t)y};
    events[1]=(risc_touch_event_v1){++seq,ms+10,RISC_TOUCH_EVENT_MOVE,0,(uint16_t)outside,(uint16_t)y};
    events[2]=(risc_touch_event_v1){++seq,ms+20,RISC_TOUCH_EVENT_UP,0,(uint16_t)x,(uint16_t)y};
    assert(tick(&e)==RISC_SCENE_IDLE);
    key_tap(0,0);queue_gap=true;assert(tick(&e)==RISC_SCENE_IDLE);settle();
    /* A lost batch must not leave the presenter waiting for an event the
     * provider never received. */
    key_tap(0,0);snapshot_contacts=2;assert(tick(&e)==RISC_SCENE_IDLE);
    snapshot_contacts=0;settle();key_tap(0,0);expect_key('Q');
}
static void keyboard_superseded(void){
    allow_complete=false;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
    supersede_frame=true;allow_complete=true;key_tap(0,0);
    assert(tick(&e)==RISC_SCENE_IDLE&&submits==2);
    key_tap(0,0);assert(tick(&e)==RISC_SCENE_IDLE);settle();
    key_tap(0,0);expect_key('Q');
}

#ifdef TEST_PACKAGED_PROFILE
extern const risc_driver_v2 *scene_test_profile_get(uint32_t);
/* Read using the installed panel orientation, never the selected profile's
 * rotation. This fails if either packaged policy or scene transform flips. */
static unsigned installed_pixel_level(unsigned x,unsigned y){
    unsigned px=x,py=y;
    if(w==800){px=y;py=479-x;}
    unsigned bits=format==5?16:1,stride=(w*bits+7)/8;
    const uint8_t *row=pixels+16+py*stride;
    return format==5?(row[px*2]|(unsigned)row[px*2+1]<<8):((row[px/8]>>(7-(px%8)))&1u);
}
static void packaged_orientation(void){
    assert(prof.touch_rotation==0);
    strcpy(doc.nodes[0].text,"Fq");acknowledge();
    unsigned x=prof.padding,y=prof.font_scale*7+prof.padding*2+8+prof.font_scale*8+4;
    for(unsigned c=0;c<2;c++)for(unsigned col=0;col<5;col++)for(unsigned row=0;row<7;row++){
        bool ink=(glyphs[(unsigned char)doc.nodes[0].text[c]-32][col]&(1u<<row))!=0;
        assert((installed_pixel_level(x+(c*6+col)*prof.font_scale,y+row*prof.font_scale)!=background_level())==ink);
    }
    /* The border of the displayed Q tile is at the same physical position as
     * the raw touch that selects Q, including the far side of the panel. */
    installed_physical_input=w==800;
    for(unsigned col=0;col<10;col++){
        unsigned cx,cy;key_center(0,col,&cx,&cy);
        unsigned top=w==800?372:84,bottom=top+(w==800?79:25);
        assert(installed_pixel_level(cx,top)!=background_level());
        assert(installed_pixel_level(cx,bottom)!=background_level());
    }
    assert(prof.display_rotation==(w==800?270u:0u));
    keyboard_touch();
}
#endif

int main(int argc,char **argv){
    assert(argc>=3);const char *mode=argv[1];
    if(!strcmp(argv[2],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,90,0,0};}
    else if(!strcmp(argv[2],"gray")){w=480;h=800;format=3;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,0,0,0};}
#ifdef TEST_PACKAGED_PROFILE
    const risc_driver_v2 *packaged=scene_test_profile_get(2);
    assert(packaged&&packaged->start(NULL,0));prof=*(const risc_scene_profile_v1*)packaged->capability;
    assert(prof.preferred_format==format);
#endif
    if(!strcmp(mode,"keyboard-pending")){display_flags=RISC_DISPLAY_INFO_MAILBOX;expected_queue=RISC_DISPLAY_QUEUE_MAILBOX;}
    startup();keyboard_document();
    if(!strncmp(mode,"keyboard-native-",16)){
        risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
        loss_callback=mode+16;
        if(!strcmp(loss_callback,"snapshot"))loss_skip=1; /* First snapshot belongs to the pre-completion drain. */
        assert(tick(&e)==RISC_SCENE_RETAINED&&!native_alive);
        unsigned before=calls;assert(tick(&e)==RISC_SCENE_RETAINED);
        assert(api->close(NULL,session)==RISC_SCENE_RETAINED&&!driver->quiesce());driver->stop();assert(calls==before);
        printf("scene host %s %s PASS (no I/O after completion callback retention)\n",mode,argv[2]);return 0;
    }
#ifdef TEST_PACKAGED_PROFILE
    if(!strcmp(mode,"profile-orientation"))packaged_orientation();
    else
#endif
    if(!strcmp(mode,"keyboard-touch"))keyboard_touch();
    else if(!strcmp(mode,"keyboard-navigation"))keyboard_navigation();
    else if(!strcmp(mode,"keyboard-pending"))keyboard_pending();
    else if(!strcmp(mode,"keyboard-stale"))keyboard_stale();
    else if(!strcmp(mode,"keyboard-hardware"))keyboard_hardware();
    else if(!strcmp(mode,"keyboard-invalid"))keyboard_invalid();
    else if(!strcmp(mode,"keyboard-gesture"))keyboard_gesture();
    else if(!strcmp(mode,"keyboard-superseded"))keyboard_superseded();
    else if(!strcmp(mode,"keyboard-raster"))keyboard_raster(argc>3?argv[3]:NULL);
    else assert(0);
    finish();assert(!releases);
    printf("scene host %s %s PASS (frames=%u calls=%u)\n",mode,argv[2],submits,calls);return 0;
}
