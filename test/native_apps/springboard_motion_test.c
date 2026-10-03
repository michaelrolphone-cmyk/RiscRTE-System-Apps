#include "../../Apps/springboard_motion.h"
#include <assert.h>
#include <stdio.h>
static const sb_point points[]={{0,0},{-54,0},{-27,-46.75f},{27,-46.75f},{54,0},{27,46.75f},{-27,46.75f}};
static unsigned run(unsigned count,unsigned dt,float dx,float dy,bool trace){
 sb_motion s;sb_motion_init(&s,points,count);sb_motion_begin(&s);
 for(unsigned t=0;t<300;t+=dt){unsigned step=300-t<dt?300-t:dt;sb_motion_drag(&s,dx*(float)step/300,dy*(float)step/300,step);sb_point p=sb_project(&s,s.x,s.y);assert(sb_abs(s.x-p.x)<=40.1f&&sb_abs(s.y-p.y)<=40.1f);}
 sb_motion_release(&s);unsigned elapsed=0;
 for(;elapsed<10000&&!s.settled;elapsed+=dt){sb_motion_step(&s,dt);sb_point p=sb_project(&s,s.x,s.y);assert(sb_abs(s.x-p.x)<=40.1f&&sb_abs(s.y-p.y)<=40.1f);if(trace)printf("%u,%f,%f,%f,%f\n",elapsed,s.x,s.y,s.vx,s.vy);}
 assert(s.settled);assert(elapsed<6000);assert(sb_length(s.x-points[s.target].x,s.y-points[s.target].y)<.01f);return s.target;
}
int main(int argc,char **argv){(void)argv;
 for(unsigned n=1;n<=7;++n){unsigned a=run(n,8,150,90,false),b=run(n,16,150,90,false),c=run(n,33,150,90,false);assert(a==b&&b==c);run(n,20,-220,-180,false);run(n,20,15,10,false);}
 sb_motion s;sb_motion_init(&s,points,0);sb_motion_step(&s,1000);assert(s.settled);
 sb_motion_init(&s,points,7);s.settled=false;s.vx=1600;s.vy=800;for(unsigned i=0;i<2000&&!s.settled;++i)sb_motion_step(&s,4);assert(s.settled);
 sb_motion_init(&s,points,1);sb_motion_begin(&s);sb_motion_drag(&s,256,256,1);sb_motion_release(&s);sb_motion_step(&s,2000);assert(s.vx==0&&s.vy==0);assert(sb_length(s.x,s.y)==0);
 if(argc>1)run(7,16,150,90,true);
 else puts("NOVA spring/friction: 0..7 apps, 30/60/120Hz, diagonal overscroll, fast fling, release outside, large gap, bounded settle PASS");
}
