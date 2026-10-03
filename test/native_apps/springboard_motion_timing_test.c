#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "springboard_motion.h"
static unsigned layout_dist(sb_point p){int x=(int)(p.x*16),y=(int)(p.y*16);unsigned n=(unsigned)(x*x+y*y),r=0,b=1u<<30;while(b>n)b>>=2;while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
static void points(sb_point*p,unsigned n){unsigned k=0;for(int q=-2;q<=2;q++)for(int r=-2;r<=2;r++)if(abs(q+r)<=2){sb_point v={-54*q-27*r,-46.75f*r};unsigned j=k++;while(j && layout_dist(p[j-1]) > layout_dist(v)){p[j]=p[j-1];j--;}p[j]=v;}(void)n;}
static double result[3][61][2];
int main(void){unsigned fails=0;for(unsigned n=0;n<=19;n++)for(unsigned scenario=0;scenario<5;scenario++){
unsigned hzs[]={30,60,120};double largest=0;unsigned longest=0;
for(unsigned h=0;h<3;h++){sb_point p[19];points(p,n);sb_motion s;sb_motion_init(&s,p,n);sb_motion_begin(&s);unsigned last=0;float sx=scenario==0?20:scenario==1?200:scenario==2?1000:scenario==3?-1000:400,sy=scenario==3?1000:scenario==4?0:sx;
for(unsigned frame=1;last<600;frame++){unsigned now=frame*1000/hzs[h];if(now>600)now=600;sb_motion_drag(&s,sx*(now-last)/1000,sy*(now-last)/1000,now-last);last=now;}
sb_motion_release(&s);result[h][0][0]=s.x;result[h][0][1]=s.y;last=0;unsigned settled=0;
for(unsigned frame=1;last<6000;frame++){unsigned now=frame*1000/hzs[h];sb_motion_step(&s,now-last);last=now;sb_point p2=sb_project(&s,s.x,s.y);double d=hypot(s.x-p2.x,s.y-p2.y);if(d>largest)largest=d;if(!isfinite(s.x)||!isfinite(s.y)||!isfinite(s.vx)||!isfinite(s.vy)){printf("nonfinite\n");fails++;break;}if(now%100==0){result[h][now/100][0]=s.x;result[h][now/100][1]=s.y;}if(s.settled&&!settled)settled=now;}
if(!s.settled){printf("not settled count=%u scenario=%u hz=%u\n",n,scenario,hzs[h]);fails++;}if(settled>longest)longest=settled;
}
double diff=0;for(unsigned t=0;t<=60;t++)for(unsigned h=0;h<2;h++){double d=hypot(result[h][t][0]-result[2][t][0],result[h][t][1]-result[2][t][1]);if(d>diff)diff=d;}if(diff>2.0){fprintf(stderr,"Frame-rate drift exceeded 2px\n");fails++;}printf("n=%u scenario=%u maxHzDiff=%.6f maxOverscroll=%.6f latestSettle=%u\n",n,scenario,diff,largest,longest);
}
printf("failures=%u\n",fails);return fails?1:0;}
