#pragma once
#include <stdbool.h>
#include <stdint.h>
/* App-owned spring/friction model; tuning uses seconds, pixels and pixels/s.
 * At most 25 four-millisecond integration steps per call, no background timer.
 * Edge damping is >10x interior friction, preventing repeated wall bounces. */
#define SB_MOTION_MAX 19u
#define SB_MOTION_OVERSCROLL 40.0f
#define SB_MOTION_EDGE_SPRING 180.0f
#define SB_MOTION_EDGE_DAMPING 25.0f
#define SB_MOTION_FRICTION 2.4f
#define SB_MOTION_SNAP_SPRING 110.0f
#define SB_MOTION_SNAP_DAMPING 21.0f
typedef struct { float x,y; } sb_point;
typedef struct {
    sb_point points[SB_MOTION_MAX];
    float min_x,max_x,min_y,max_y;
    unsigned count,target;
    float x,y,vx,vy,drag_x,drag_y;
    bool dragging,snapping,settled;
} sb_motion;
static inline float sb_abs(float x) { return x<0?-x:x; }
static inline float sb_limit(float x,float n) { return x < -n ? -n : x > n ? n : x; }
static inline float sb_length(float x,float y) {
    /* All inputs are bounded to +/-2048. Fixed-point root avoids libm. */
    int a=(int)(x*16),b=(int)(y*16);
    unsigned n=(unsigned)(a*a)+(unsigned)(b*b),r=0,bit=1u<<30;
    while(bit>n)bit>>=2;
    while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
    return (float)r/16.0f;
}
static inline void sb_motion_init(sb_motion *s,const sb_point *points,unsigned n) {
    *s=(sb_motion){0};s->count=n>SB_MOTION_MAX?SB_MOTION_MAX:n;s->settled=true;
    for(unsigned i=0;i<s->count;++i){
        s->points[i]=points[i];
        if(!i || points[i].x<s->min_x)s->min_x=points[i].x;
        if(!i || points[i].x>s->max_x)s->max_x=points[i].x;
        if(!i || points[i].y<s->min_y)s->min_y=points[i].y;
        if(!i || points[i].y>s->max_y)s->max_y=points[i].y;
    }
}
static inline sb_point sb_project(const sb_motion *s,float x,float y) {
    return (sb_point){x<s->min_x?s->min_x:x>s->max_x?s->max_x:x,
                      y<s->min_y?s->min_y:y>s->max_y?s->max_y:y};
}
static inline unsigned sb_nearest(const sb_motion *s) {
    float best=1e12f;unsigned n=0;
    for(unsigned i=0;i<s->count;++i){float dx=s->x-s->points[i].x,dy=s->y-s->points[i].y,d=dx*dx+dy*dy;if(d<best){best=d;n=i;}}
    return n;
}
static inline void sb_motion_begin(sb_motion *s) {
    if(!s->count)return;
    s->dragging=true;s->snapping=false;s->settled=false;s->vx=s->vy=0;
    s->drag_x=s->x;s->drag_y=s->y;
}
static inline float sb_rubber_more(float overscroll,float delta) {
    if(overscroll>=SB_MOTION_OVERSCROLL)return SB_MOTION_OVERSCROLL;
    float remaining=SB_MOTION_OVERSCROLL-overscroll;
    return SB_MOTION_OVERSCROLL-remaining/(1+delta*remaining/(SB_MOTION_OVERSCROLL*SB_MOTION_OVERSCROLL));
}
static inline float sb_drag_axis(float p,float delta,float lo,float hi) {
    /* Integrate outward resistance exactly, so sample frequency is irrelevant.
     * Inward motion follows the finger directly, including overscroll regrabs. */
    if(p<lo && delta<0)return lo-sb_rubber_more(lo-p,-delta);
    if(p>hi && delta>0)return hi+sb_rubber_more(p-hi,delta);
    float desired=p+delta;
    if(desired<lo && p>=lo)return lo-sb_rubber_more(0,lo-desired);
    if(desired>hi && p<=hi)return hi+sb_rubber_more(0,desired-hi);
    return desired;
}
static inline void sb_motion_drag(sb_motion *s,float dx,float dy,uint32_t ms) {
    if(!s->dragging || !s->count)return;
    float x=sb_drag_axis(s->x,sb_limit(dx,256),s->min_x,s->max_x);
    float y=sb_drag_axis(s->y,sb_limit(dy,256),s->min_y,s->max_y);
    if(ms<1)ms=1;
    if(ms>100)ms=100;
    s->vx=sb_limit((x-s->x)*1000.0f/(float)ms,1600);s->vy=sb_limit((y-s->y)*1000.0f/(float)ms,1600);
    s->x=x;s->y=y;s->settled=false;
}

static inline void sb_motion_release(sb_motion *s) {s->dragging=false;s->snapping=false;}
static inline void sb_motion_cancel(sb_motion *s) {
    sb_point p=sb_project(s,s->x,s->y);s->x=p.x;s->y=p.y;s->vx=s->vy=0;s->dragging=false;
    s->target=sb_nearest(s);s->snapping=s->count!=0;s->settled=!s->count;
}
static inline void sb_motion_target(sb_motion *s,unsigned target) {
    if(target>=s->count)return;
    s->target=target;s->snapping=true;s->settled=false;
}
static inline float sb_acceleration(float p,float v,float lo,float hi,float target,bool snap) {
    float q=p<lo?lo:p>hi?hi:p;
    if(sb_abs(p-q)>.015f)return (q-p)*SB_MOTION_EDGE_SPRING-v*SB_MOTION_EDGE_DAMPING;
    if(snap)return (target-p)*SB_MOTION_SNAP_SPRING-v*SB_MOTION_SNAP_DAMPING;
    float distance=p-lo<hi-p?p-lo:hi-p;
    float near=distance<50?1-distance/50:0;
    return -v*(SB_MOTION_FRICTION+near*(12.0f-SB_MOTION_FRICTION));
}
static inline void sb_bound_axis(float *p,float *v,float lo,float hi) {
    if(*p<lo-SB_MOTION_OVERSCROLL){*p=lo-SB_MOTION_OVERSCROLL;if(*v<0)*v=0;}
    if(*p>hi+SB_MOTION_OVERSCROLL){*p=hi+SB_MOTION_OVERSCROLL;if(*v>0)*v=0;}
}
static inline void sb_motion_step(sb_motion *s,uint32_t elapsed_ms) {
    if(s->dragging || s->settled || !s->count)return;
    if(elapsed_ms>250){sb_motion_cancel(s);return;}
    if(elapsed_ms>100)elapsed_ms=100;
    while(elapsed_ms){
        unsigned ms=elapsed_ms>4?4:elapsed_ms;elapsed_ms-=ms;float dt=(float)ms/1000.0f;
        sb_point p=sb_project(s,s->x,s->y);bool edge=sb_abs(p.x-s->x)>.015f||sb_abs(p.y-s->y)>.015f;
        if(!edge && !s->snapping && sb_length(s->vx,s->vy)<12){s->snapping=true;s->target=sb_nearest(s);}
        /* Spring capture changes acceleration, never resets position/velocity. */
        float ax=sb_acceleration(s->x,s->vx,s->min_x,s->max_x,s->points[s->target].x,s->snapping);
        float ay=sb_acceleration(s->y,s->vy,s->min_y,s->max_y,s->points[s->target].y,s->snapping);
        s->vx+=ax*dt;s->vy+=ay*dt;s->x+=s->vx*dt;s->y+=s->vy*dt;
        sb_bound_axis(&s->x,&s->vx,s->min_x,s->max_x);sb_bound_axis(&s->y,&s->vy,s->min_y,s->max_y);
        if(s->snapping && sb_length(s->x-s->points[s->target].x,s->y-s->points[s->target].y)<.2f && sb_length(s->vx,s->vy)<.5f){s->x=s->points[s->target].x;s->y=s->points[s->target].y;s->vx=s->vy=0;s->settled=true;break;}
    }
}
