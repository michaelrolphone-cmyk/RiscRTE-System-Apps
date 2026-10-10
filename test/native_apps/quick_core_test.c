#include "PortableQuickActions.h"
#include "PortableQuickRender.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pqa_state fresh(void) {pqa_state s;pqa_init(&s);pqa_set_levels(&s,true,40,true,50);return s;}
static pqa_state opened(void) {pqa_state s=fresh();s.position_q8=s.target_q8=PQA_OPEN_Q8;return s;}
static bool down(pqa_state *s,unsigned t,int x,int y){return pqa_input(s,t,true,1,1,x,y,true);}
static bool up(pqa_state *s,unsigned t){return pqa_input(s,t,true,0,1,0,0,true);}
static void click(pqa_state *s,unsigned t,int x,int y){assert(down(s,t,x,y));assert(up(s,t+10));}
static void test_routes(void) {
    pqa_state s=fresh();
    assert(!down(&s,0,100,70));assert(s.route==PQA_PASS);
    assert(!down(&s,20,100,4));assert(!up(&s,30));assert(!pqa_visible(&s));
    assert(down(&s,40,90,10));assert(s.route==PQA_RESERVED);
    assert(!up(&s,50));assert(s.route==PQA_REPLAY);assert(s.start_x==90 && s.start_y==10);
    assert(down(&s,60,90,10));assert(!down(&s,70,110,12));assert(s.route==PQA_REPLAY);
    assert(!down(&s,80,100,100));assert(!up(&s,90));assert(!pqa_visible(&s));
    assert(down(&s,100,90,10));assert(!down(&s,110,90,0));assert(s.route==PQA_REPLAY);up(&s,120);
    assert(!pqa_input(&s,130,true,1,1,80,20,false));up(&s,140);
    assert(!down(&s,150,80,36));assert(!up(&s,160));
    assert(down(&s,170,80,35));assert(down(&s,180,80,180));assert(s.gesture==PQA_PANEL_DRAG);
    assert(s.position_q8==145*256);assert(up(&s,190));assert(s.target_q8==PQA_OPEN_Q8);
    assert(!s.pending);
}
static void test_controls(void) {
    pqa_state s=opened();
    assert(down(&s,1,170,49));assert(s.gesture==PQA_BRIGHTNESS_DRAG);assert(s.brightness==90);
    assert(pqa_take_action(&s)==PQA_BRIGHTNESS_PREVIEW);assert(s.action_brightness==90);
    assert(down(&s,2,250,80));assert(s.brightness==100);assert(up(&s,3));
    assert(pqa_take_action(&s)==PQA_BRIGHTNESS_COMMIT);assert(s.action_brightness==100);
    assert(down(&s,10,44,62));assert(s.brightness==10);assert(up(&s,11));
    assert(s.pending==PQA_BRIGHTNESS_COMMIT);assert(s.action_brightness==10);
    assert(down(&s,12,182,62));assert(s.brightness==100);assert(s.pending==PQA_BRIGHTNESS_COMMIT);
    assert(s.action_brightness==10);assert(up(&s,13));assert(s.action_brightness==100);pqa_take_action(&s);
    assert(down(&s,20,44,92));assert(s.volume==0);assert(!s.pending);assert(up(&s,21));
    assert(pqa_take_action(&s)==PQA_VOLUME_COMMIT);assert(s.action_volume==0 && s.last_nonzero_volume==50);
    click(&s,30,51,139);assert(s.volume==50);assert(pqa_take_action(&s)==PQA_SILENT);
    click(&s,50,51,139);assert(s.volume==0);assert(pqa_take_action(&s)==PQA_SILENT);
    click(&s,70,122,139);assert(!s.pending);click(&s,90,193,139);assert(!s.pending);
    click(&s,110,122,187);assert(!s.pending);
    click(&s,130,51,187);assert(pqa_take_action(&s)==PQA_WIFI);assert(!s.target_q8);
    s=opened();click(&s,1,193,187);assert(s.torch);assert(pqa_take_action(&s)==PQA_TORCH);assert(s.action_torch);
    click(&s,20,20,20);assert(!s.torch);assert(pqa_take_action(&s)==PQA_TORCH);assert(!s.action_torch);
    s=opened();click(&s,1,120,222);assert(!s.target_q8);assert(!s.pending);
    s=opened();assert(down(&s,1,51,139));assert(down(&s,10,52,100));assert(up(&s,11));assert(!s.pending);
    s=opened();assert(down(&s,1,51,139));assert(down(&s,10,100,139));assert(up(&s,11));assert(!s.pending);
}
static void test_cancel(void) {
    pqa_state s=opened();down(&s,0,180,60);pqa_take_action(&s);assert(s.brightness==100);
    assert(pqa_input(&s,10,false,0,0,0,0,true));assert(s.brightness==40);assert(s.neutral_gate);
    assert(pqa_take_action(&s)==PQA_BRIGHTNESS_PREVIEW);assert(s.action_brightness==40);
    down(&s,20,190,180);assert(!s.pending);up(&s,30);assert(!s.neutral_gate);
    down(&s,40,44,90);assert(s.volume==0);
    assert(pqa_input(&s,50,true,2,1,90,90,true));assert(s.volume==50);assert(!s.pending);up(&s,60);
    down(&s,70,190,185);assert(pqa_input(&s,80,true,1,2,190,185,true));up(&s,90);assert(!s.torch && !s.pending);
    down(&s,100,190,185);up(&s,110);assert(s.torch);pqa_take_action(&s);
    pqa_cancel(&s);assert(!pqa_visible(&s) && s.neutral_gate);assert(pqa_take_action(&s)==PQA_TORCH);
    assert(s.brightness==40 && s.volume==50);
}
static void test_animation(void) {
    pqa_state a=fresh(),b=fresh();a.target_q8=b.target_q8=PQA_OPEN_Q8;
    pqa_animate(&a,0);pqa_animate(&b,0);
    for(unsigned t=8;t<=800;t+=8)pqa_animate(&a,t);
    for(unsigned t=40;t<=800;t+=40)pqa_animate(&b,t);
    assert(a.position_q8==b.position_q8 && a.velocity_q8==b.velocity_q8);
    assert(a.position_q8==PQA_OPEN_Q8);
    a.target_q8=0;for(unsigned t=808;t<=1600;t+=8)pqa_animate(&a,t);assert(a.position_q8==0);
    a=fresh();pqa_animate(&a,UINT32_MAX-4);a.target_q8=PQA_OPEN_Q8;pqa_animate(&a,4);assert(a.position_q8>0);
    pqa_animate(&a,10005);assert(a.position_q8==PQA_OPEN_Q8);
    a=opened();down(&a,0,100,210);down(&a,16,100,80);assert(a.position_q8==110*256);
    down(&a,24,100,180);up(&a,25);assert(a.target_q8==PQA_OPEN_Q8);
    a=opened();down(&a,0,100,210);down(&a,16,100,180);up(&a,500);assert(a.target_q8==PQA_OPEN_Q8);
}
static void test_render(void) {
    enum { STRIDE=497, SIZE=STRIDE*239+480, GUARD=19 };
    unsigned char *buf=malloc(SIZE+2*GUARD),*backup=malloc(SIZE+2*GUARD);assert(buf&&backup);
    memset(buf,0xa5,SIZE+2*GUARD);memcpy(backup,buf,SIZE+2*GUARD);
    risc_display_surface_v1 f={1,buf+GUARD,240,240,STRIDE,SIZE,RISC_DISPLAY_FORMAT_RGB565};
    pqa_state s=fresh();assert(pqa_render(&f,&s,"08:42",true,0));assert(!memcmp(buf,backup,SIZE+2*GUARD));
    for(int phase=0;phase<243;++phase) {
        memset(buf,0xa5,SIZE+2*GUARD);s.position_q8=phase*256-256;s.target_q8=PQA_OPEN_Q8;
        s.torch=phase==242;assert(pqa_render(&f,&s,NULL,true,255));
        assert(!memcmp(buf,backup,GUARD));assert(!memcmp(buf+GUARD+SIZE,backup,GUARD));
        for(int y=0;y<239;++y)assert(!memcmp(buf+GUARD+y*STRIDE+480,backup,STRIDE-480));
    }
    f.size_bytes=SIZE-1;memcpy(buf,backup,SIZE+2*GUARD);assert(!pqa_render(&f,&s,NULL,false,0));assert(!memcmp(buf,backup,SIZE+2*GUARD));
    f.size_bytes=UINT32_MAX;f.stride_bytes=UINT32_MAX;assert(!pqa_render(&f,&s,NULL,false,0));
    f.stride_bytes=479;assert(!pqa_render(&f,&s,NULL,false,0));
    /* Tight RGB565 storage has no spare last row, and results match padding. */
    unsigned char tight[240*240*2], padded[240*496];
    memset(tight,0x63,sizeof(tight));memset(padded,0x63,sizeof(padded));
    risc_display_surface_v1 a={1,tight,240,240,480,sizeof(tight),RISC_DISPLAY_FORMAT_RGB565};
    risc_display_surface_v1 b={1,padded,240,240,496,sizeof(padded),RISC_DISPLAY_FORMAT_RGB565};
    s=opened();assert(pqa_render(&a,&s,"23:59",true,0));assert(pqa_render(&b,&s,"23:59",true,0));
    for(unsigned y=0;y<240;++y)assert(!memcmp(tight+y*480,padded+y*496,480));
    a.width=239;assert(!pqa_render(&a,&s,"",true,0));a.width=240;
    a.height=239;assert(!pqa_render(&a,&s,"",true,0));a.height=240;
    a.pixel_format=RISC_DISPLAY_FORMAT_GRAY2;assert(!pqa_render(&a,&s,"",true,0));
    assert(!pqa_render(NULL,&s,"",true,0));assert(!pqa_render(&b,NULL,"",true,0));
    free(buf);free(backup);
}
static void test_fuzz(void) {
    pqa_state s=fresh();unsigned random=721347, now=0;
    for(int i=0;i<100000;++i) {
        random=random*1664525u+1013904223u;now+=random%30;
        unsigned count=(random>>29)%4;int x=(int)((random>>8)%600)-200,y=(int)((random>>18)%600)-200;
        pqa_input(&s,now,(random&63)!=0,count,(random>>25)&3,x,y,true);pqa_animate(&s,now);
        assert(s.position_q8>=0 && s.position_q8<=PQA_OPEN_Q8);
        assert(s.volume<=100 && s.brightness<=100);if(random&2)pqa_take_action(&s);
    }
}
static void test_dnd(void) {
 pqa_state s=opened();s.dnd_valid=true;
 s.radios_valid=s.wifi_enabled=s.bluetooth_enabled=true;
 click(&s,10,122,139);assert(pqa_take_action(&s)==PQA_DND);
 assert(s.dnd_enabled && s.action_dnd && s.volume==50 && s.brightness==40);
 assert(s.radios_valid && s.wifi_enabled && s.bluetooth_enabled && !s.airplane);
 click(&s,30,122,139);assert(pqa_take_action(&s)==PQA_DND);
 assert(!s.dnd_enabled && !s.action_dnd);
 s.dnd_valid=false;click(&s,50,122,139);assert(!s.pending && !s.dnd_enabled);
 /* Genuine moon tile gains active cyan without modifying any other tile. */
 unsigned char off[240*240*2]={0},on[240*240*2]={0};
 risc_display_surface_v1 a={1,off,240,240,480,sizeof(off),RISC_DISPLAY_FORMAT_RGB565};
 risc_display_surface_v1 b={1,on,240,240,480,sizeof(on),RISC_DISPLAY_FORMAT_RGB565};
 s.dnd_valid=true;assert(pqa_render(&a,&s,"12:34",true,50));
 s.dnd_enabled=true;assert(pqa_render(&b,&s,"12:34",true,50));
 unsigned changed=0;
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++) {
  bool different=memcmp(off+(y*240+x)*2,on+(y*240+x)*2,2)!=0;
  if(different){assert(x>=91 && x<153 && y>=118 && y<160);changed++;}
 }
 assert(changed>1000);
}
static void contexts_control(void){
 pqa_state s;pqa_init(&s);s.contexts_controls=s.contexts_valid=true;s.position_q8=s.target_q8=PQA_OPEN_Q8;
 assert(pqa_input(&s,10,true,1,1,190,202,true));assert(pqa_input(&s,20,true,0,0,0,0,true));
 assert(pqa_take_action(&s)==PQA_CONTEXTS&&s.action_contexts&&!s.contexts_enabled);
 s.contexts_enabled=true;assert(pqa_input(&s,30,true,1,1,190,202,true));assert(pqa_input(&s,40,true,0,0,0,0,true));
 assert(pqa_take_action(&s)==PQA_CONTEXTS&&!s.action_contexts);
 assert(pqa_input(&s,50,true,1,1,190,202,true));assert(pqa_input(&s,60,true,1,1,190,140,true));assert(pqa_input(&s,70,true,0,0,0,0,true));assert(!pqa_take_action(&s));
 s.position_q8=s.target_q8=PQA_OPEN_Q8;s.contexts_valid=false;
 assert(pqa_input(&s,80,true,1,1,190,202,true));assert(pqa_input(&s,90,true,0,0,0,0,true));assert(!pqa_take_action(&s));
}
int main(void){contexts_control();test_dnd();test_routes();test_controls();test_cancel();test_animation();test_render();test_fuzz();puts("quick actions: all controller, animation, surface and fuzz tests passed");return 0;}
