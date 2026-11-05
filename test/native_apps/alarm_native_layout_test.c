#include "PortableAlarmLayout.h"
#include <assert.h>
#include <stdio.h>

static unsigned layouts,blocked_layouts,edges;
static void bounds(portable_alarm_rect r,unsigned w,unsigned h) {
    assert(r.x>=0 && r.y>=0 && r.w>=0 && r.h>=0);
    assert((uint64_t)r.x+r.w<=w && (uint64_t)r.y+r.h<=h);
}
static void hits(portable_alarm_rect r) {
    if(!r.w || !r.h){assert(!portable_alarm_hit(r,r.x,r.y));return;}
    assert(portable_alarm_hit(r,r.x,r.y));
    assert(portable_alarm_hit(r,r.x+r.w-1,r.y+r.h-1));
    assert(!portable_alarm_hit(r,r.x-1,r.y));
    assert(!portable_alarm_hit(r,r.x,r.y-1));
    assert(!portable_alarm_hit(r,r.x+r.w,r.y));
    assert(!portable_alarm_hit(r,r.x,r.y+r.h));
    assert(!portable_alarm_hit(r,INT32_MIN,INT32_MAX));edges+=7;
}
static void one(unsigned w,unsigned h,bool blocked) {
    portable_alarm_layout l;assert(portable_alarm_layout_for(w,h,blocked,&l));
    bounds(l.title,w,h);bounds(l.state,w,h);bounds(l.warning,w,h);
    bounds(l.rule,w,h);bounds(l.dismiss,w,h);bounds(l.retry,w,h);
    hits(l.dismiss);hits(l.retry);
    if(blocked && l.dismiss.w && l.dismiss.h && l.retry.w && l.retry.h)
        assert(l.dismiss.x+l.dismiss.w<=l.retry.x || l.retry.x+l.retry.w<=l.dismiss.x ||
               l.dismiss.y+l.dismiss.h<=l.retry.y || l.retry.y+l.retry.h<=l.dismiss.y);
    if(!blocked)assert(!l.retry.w&&!l.retry.h);
    layouts++;blocked_layouts+=blocked;
}
int main(void) {
    const unsigned extents[]={1,2,3,15,16,31,32,63,64,119,120,149,150,160,239,240,410,502,640,2048,65535};
    for(unsigned i=0;i<sizeof(extents)/sizeof(extents[0]);i++)
        for(unsigned j=0;j<sizeof(extents)/sizeof(extents[0]);j++)
            for(unsigned b=0;b<2;b++)one(extents[i],extents[j],b!=0);
    uint32_t seed=0x195af584u;
    for(unsigned i=0;i<100000;i++) {
        seed=seed*1664525u+1013904223u;unsigned w=seed%65535u+1u;
        seed=seed*1664525u+1013904223u;unsigned h=seed%65535u+1u;
        one(w,h,((seed>>8)&1)!=0);
    }
    portable_alarm_layout l;
    assert(!portable_alarm_layout_for(0,240,false,&l));
    assert(!portable_alarm_layout_for(240,0,false,&l));
    assert(!portable_alarm_layout_for(65536,240,false,&l));
    assert(!portable_alarm_layout_for(240,UINT32_MAX,false,&l));
    assert(!portable_alarm_layout_for(240,240,false,NULL));
    assert(blocked_layouts>40000 && blocked_layouts<60000);
    printf("{\"layouts\":%u,\"blocked_layouts\":%u,\"hit_assertions\":%u,\"invalid\":5}\n",layouts,blocked_layouts,edges);
    return 0;
}
