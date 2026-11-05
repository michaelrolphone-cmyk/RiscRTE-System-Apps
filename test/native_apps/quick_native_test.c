#include "PortableQuickRender.h"
#include "PortableQuickLayout.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pqa_state make(unsigned w,unsigned h,bool contexts) {
    pqa_state s;pqa_init(&s);assert(pqa_set_dimensions(&s,w,h));
    pqa_set_levels(&s,true,40,true,50);s.dnd_valid=s.radios_valid=s.radio_controls=true;
    s.contexts_controls=contexts;s.contexts_valid=true;s.position_q8=s.target_q8=pqa_open_q8(&s);return s;
}
static void down(pqa_state *s,unsigned t,int x,int y) {assert(pqa_input(s,t,true,1,71,x,y,true));}
static void up(pqa_state *s,unsigned t) {assert(pqa_input(s,t,true,0,71,0,0,true));}
static void tap(pqa_state *s,pqa_rect r) {down(s,100,r.x+r.w/2,r.y+r.h/2);up(s,110);}
static void controls(unsigned w,unsigned h,bool contexts) {
    pqa_state s=make(w,h,contexts);pqa_layout l=pqa_layout_for(&s);
    assert(pqa_width(&s)==w&&pqa_height(&s)==h);
    assert(!pqa_set_dimensions(&s,h,w));
    for(int tile=0;tile<6;++tile) {
        s=make(w,h,contexts);tap(&s,l.tiles[tile]);
        const uint32_t expected[]={PQA_SILENT,PQA_DND,PQA_AIRPLANE,PQA_WIFI,PQA_BLUETOOTH,PQA_TORCH};
        assert(pqa_take_action(&s)==expected[tile]);
    }
    if(contexts) {s=make(w,h,true);tap(&s,l.contexts);assert(pqa_take_action(&s)==PQA_CONTEXTS);}
    s=make(w,h,contexts);down(&s,100,l.slider_left,l.slider_y[0]);
    assert(s.gesture==PQA_BRIGHTNESS_DRAG&&s.brightness==10);assert(pqa_take_action(&s)==PQA_BRIGHTNESS_PREVIEW);
    down(&s,110,l.slider_right,l.slider_y[0]);assert(s.brightness==100);up(&s,120);
    assert(pqa_take_action(&s)==PQA_BRIGHTNESS_COMMIT&&s.action_brightness==100);
    down(&s,130,l.slider_left,l.slider_y[0]);pqa_take_action(&s);
    assert(pqa_input(&s,140,false,0,0,0,0,true));assert(s.brightness==100&&s.neutral_gate);
    assert(pqa_take_action(&s)==PQA_BRIGHTNESS_PREVIEW&&s.action_brightness==100);up(&s,150);
    down(&s,160,l.slider_left,l.slider_y[1]);assert(s.gesture==PQA_VOLUME_DRAG&&s.volume==0);up(&s,170);
    assert(pqa_take_action(&s)==PQA_VOLUME_COMMIT&&s.action_volume==0);
    s=make(w,h,contexts);tap(&s,l.handle);assert(!s.target_q8&&!pqa_take_action(&s));
    s=make(w,h,contexts);s.position_q8=s.target_q8=0;
    down(&s,100,(int)w-1,0);assert(s.route==PQA_RESERVED);
    assert(!pqa_input(&s,110,true,0,71,0,0,true)&&s.route==PQA_REPLAY&&s.start_x==(int)w-1);
    assert(!pqa_input(&s,120,true,1,71,(int)w,0,true));assert(s.route==PQA_PASS);
    assert(!pqa_input(&s,130,true,0,71,0,0,true));
    down(&s,200,(int)w/2,0);down(&s,216,(int)w/2,(int)h-1);up(&s,232);
    assert(s.target_q8==pqa_open_q8(&s));
    pqa_animate(&s,240);pqa_animate(&s,1241);assert(s.position_q8==pqa_open_q8(&s));
    down(&s,1250,l.tiles[0].x+l.tiles[0].w/2,l.tiles[0].y+l.tiles[0].h/2);
    assert(pqa_input(&s,1260,true,1,72,0,0,true));up(&s,1270);assert(!pqa_take_action(&s));
}
static void save(const char *path,const risc_display_surface_v1 *s) {
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",s->width,s->height);
    for(unsigned y=0;y<s->height;++y)for(unsigned x=0;x<s->width;++x) {
        const uint8_t *p=(const uint8_t*)s->pixels+(size_t)y*s->stride_bytes+2*x;
        unsigned v=p[0]|((unsigned)p[1]<<8);uint8_t rgb[]={(uint8_t)((v>>11)*255/31),(uint8_t)(((v>>5)&63)*255/63),(uint8_t)((v&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));
}
static void raster(unsigned w,unsigned h,bool contexts,const char *directory) {
    unsigned stride=w*2+7,size=(h-1)*stride+w*2;size_t bytes=size+38;
    uint8_t *a=malloc(bytes),*b=malloc(bytes),*old=malloc(bytes);assert(a&&b&&old);
    for(size_t i=0;i<bytes;++i)old[i]=(uint8_t)(i*37+i/257);
    risc_display_surface_v1 f={1,a+19,w,h,stride,size,RISC_DISPLAY_FORMAT_RGB565},g=f;g.pixels=b+19;
    for(int phase=0;phase<7;++phase) {
        memcpy(a,old,bytes);memcpy(b,old,bytes);pqa_state s=make(w,h,contexts);
        s.position_q8=(int)h*256*phase/5;s.target_q8=phase?pqa_open_q8(&s):0;s.torch=phase==6;s.error_flags=phase==4?PQA_ERROR_SAVE:0;
        const char bounded[8]={'2','3',':','5','9','A','B','C'};
        assert(pqa_render(&f,&s,bounded,phase!=2,phase==3?255:74));
        for(unsigned y=0;y<h;) {unsigned n=y%11+1;if(n>h-y)n=h-y;assert(pqa_render_rows(&g,&s,bounded,phase!=2,phase==3?255:74,y,n));y+=n;}
        assert(!memcmp(a,b,bytes));assert(!memcmp(a,old,19));assert(!memcmp(a+19+size,old+19+size,19));
        for(unsigned y=0;y+1<h;++y)assert(!memcmp(a+19+y*stride+2*w,old+19+y*stride+2*w,7));
        if(!phase)assert(!memcmp(a,old,bytes));
        if(directory&&phase==5) {char path[512];snprintf(path,sizeof(path),"%s/quick-%ux%u-%d.ppm",directory,w,h,contexts);save(path,&f);}
        memcpy(b,old,bytes);unsigned first=h/3,count=h/4;
        assert(pqa_render_rows(&g,&s,bounded,phase!=2,phase==3?255:74,first,count));
        for(unsigned y=0;y<h;++y)assert(!memcmp(b+19+y*stride,(y>=first&&y<first+count?a:old)+19+y*stride,y+1==h?2*w:stride));
        assert(!pqa_render_rows(&g,&s,NULL,false,0,h+1,0));assert(!pqa_render_rows(&g,&s,NULL,false,0,h,1));
        risc_display_surface_v1 bad=f;bad.size_bytes=size-1;assert(!pqa_render(&bad,&s,NULL,false,0));
        bad=f;bad.stride_bytes=w*2-1;assert(!pqa_render(&bad,&s,NULL,false,0));
        if(h>1) {bad=f;bad.stride_bytes=UINT32_MAX;bad.size_bytes=UINT32_MAX;assert(!pqa_render(&bad,&s,NULL,false,0));}
        bad=f;bad.width=w==2048?w-1:w+1;assert(!pqa_render(&bad,&s,NULL,false,0));
    }
    free(a);free(b);free(old);
}
static void legacy_equality(void) {
    unsigned char a[240*240*2],b[240*240*2];pqa_state native=make(240,240,false),legacy=native;legacy.native_width=0;
    risc_display_surface_v1 f={1,a,240,240,480,sizeof(a),RISC_DISPLAY_FORMAT_RGB565},g=f;g.pixels=b;
    for(int i=0;i<242;++i) {
        memset(a,0x63,sizeof(a));memset(b,0x63,sizeof(b));
        native.position_q8=legacy.position_q8=i*256;native.contexts_controls=legacy.contexts_controls=(i%2)!=0;native.torch=legacy.torch=i==241;
        assert(pqa_render(&f,&native,"12:34",true,79));assert(pqa_render(&g,&legacy,"12:34",true,79));assert(!memcmp(a,b,sizeof(a)));
    }
}
int main(int argc,char **argv) {
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    const unsigned sizes[][2]={{120,150},{160,160},{240,240},{410,502},{502,410},{2048,240}};
#else
    const unsigned sizes[][2]={{240,240}};
#endif
    pqa_state s;pqa_init(&s);assert(!pqa_set_dimensions(NULL,240,240));assert(!pqa_set_dimensions(&s,0,240));assert(!pqa_set_dimensions(&s,240,0));assert(!pqa_set_dimensions(&s,2049,240));assert(!pqa_set_dimensions(&s,240,2049));
    s.paper=true;assert(!pqa_set_dimensions(&s,240,240));s.paper=false;
    assert(pqa_set_dimensions(&s,120,150));assert(pqa_set_height(&s,294));assert(!s.native_width&&pqa_height(&s)==294);
    for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);++i)for(int contexts=0;contexts<2;++contexts) {controls(sizes[i][0],sizes[i][1],contexts);raster(sizes[i][0],sizes[i][1],contexts,argc>1?argv[1]:NULL);}
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    raster(1,1,false,NULL);raster(1,2048,true,NULL);raster(2048,1,true,NULL);
#endif
    legacy_equality();
    puts("Native Quick: dimension matrix, all controls, gesture replay/cancel, unaligned guards, sliced parity, tiny/extreme surfaces and exact 240 raster passed");return 0;
}
