#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "springboard_motion.h"
static uint32_t rng=17; static float randf(float l,float h){rng=1664525*rng+1013904223;return l+(h-l)*(float)(rng>>8)/16777216;}
static unsigned layout_dist(sb_point p){int x=(int)(p.x*16),y=(int)(p.y*16);unsigned n=(unsigned)(x*x+y*y),r=0,b=1u<<30;while(b>n)b>>=2;while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
static void layout(sb_point*p){unsigned k=0;for(int q=-2;q<=2;q++)for(int r=-2;r<=2;r++)if(abs(q+r)<=2){sb_point v={-54*q-27*r,-46.75f*r};unsigned j=k++;while(j && layout_dist(p[j-1]) > layout_dist(v)){p[j]=p[j-1];j--;}p[j]=v;}}
int main(void){sb_point points[19];layout(points);unsigned trials=0;float largest=0;unsigned longest=0;
for(unsigned n=0;n<=19;n++){sb_motion s;sb_motion_init(&s,points,n);sb_motion_begin(&s);sb_motion_drag(&s,256,256,1);sb_motion_release(&s);for(unsigned t=0;t<5000;t+=4)sb_motion_step(&s,4);assert(s.settled);
for(unsigned j=0;j<1000;j++){sb_motion_init(&s,points,n);if(!n)continue;s.x=randf(s.min_x-40,s.max_x+40);s.y=randf(s.min_y-40,s.max_y+40);s.vx=randf(-1600,1600);s.vy=randf(-1600,1600);s.settled=false;float ix=s.x,iy=s.y,ivx=s.vx,ivy=s.vy;unsigned t=0;for(;t<10000&&!s.settled;t+=4){sb_motion_step(&s,4);assert(isfinite(s.x)&&isfinite(s.y)&&isfinite(s.vx)&&isfinite(s.vy));assert(s.x>=s.min_x-40&&s.x<=s.max_x+40&&s.y>=s.min_y-40&&s.y<=s.max_y+40);sb_point p=sb_project(&s,s.x,s.y);float out=fmaxf(fabsf(p.x-s.x),fabsf(p.y-s.y));if(out>largest)largest=out;}if(!s.settled){printf("UNSETTLED n=%u j=%u initial=%f,%f v=%f,%f final=%f,%f v=%f,%f snap=%d target=%u bounds=%f,%f,%f,%f\n",n,j,ix,iy,ivx,ivy,s.x,s.y,s.vx,s.vy,s.snapping,s.target,s.min_x,s.max_x,s.min_y,s.max_y);return 1;}if(t>longest)longest=t;trials++;}
}
// Regrab does not move a stationary finger or introduce a visible jump at ordinary overscroll.
for(float x=1;x<=40;x+=1){sb_motion s;sb_motion_init(&s,points,1);s.x=x;sb_motion_begin(&s);sb_motion_drag(&s,0,0,20);assert(s.x==x);sb_motion_drag(&s,.01,0,20);assert(fabsf(s.x-x)<.011f);}
printf("%u randomized bounded-release trajectories settled; max axis overscroll=%.5f px, max settle=%u ms; empty and regrab pass\n",trials,largest,longest);
}
