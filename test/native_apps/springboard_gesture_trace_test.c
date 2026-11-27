#include <assert.h>
#include <stdio.h>
#include "PortableSpringboardGestureTrace.h"
static unsigned count;static uint32_t words[14];
static void emit(void*c,uint32_t v){(void)c;assert(count<14);words[count++]=v;}
static portable_sb_diag_sample sample(unsigned sequence,unsigned x,unsigned y){return(portable_sb_diag_sample){sequence,x,y,x,y,466,466,7,true,sequence==1,false};}
int main(void){unsigned runs=0;for(unsigned rot=0;rot<=180;rot+=180)for(int axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2){
 portable_sb_diag s={0};count=0;portable_sb_diag_sample p=sample(1,233,233);portable_sb_diag_step(&s,&p,0,0,0,rot,emit,0);assert(s.active&&!count);
 for(unsigned n=2;n<10002;n++){p=sample(n,233+(axis?0:(n%50)),233+(axis?(n%50):0));int d=n<5000?sign:-sign;portable_sb_diag_step(&s,&p,1,axis?0:d,axis?d:0,rot,emit,0);assert(!count);}
 p=sample(10002,233,233);p.released=true;portable_sb_diag_step(&s,&p,2,0,0,rot,emit,0);assert(count==14&&!s.active);assert(words[1]==((PORTABLE_SB_DIAG_CONTACT<<24)|(15u<<8)|7));++runs;
 }
 for(unsigned fault=0;fault<12;fault++){portable_sb_diag s={0};count=0;portable_sb_diag_sample p=sample(1,233,233);portable_sb_diag_step(&s,&p,0,0,0,0,emit,0);p=sample(2,230,230);
 switch(fault){case 0:p.valid=false;break;case 1:p.width=0;break;case 2:p.height=1025;break;case 3:p.px=466;break;case 4:p.y=466;break;case 5:p.id=8;break;case 6:p.sequence=UINT64_MAX;break;case 7:p.sequence=0;break;case 8:p.width=465;break;case 9:p.released=true;break;case 10:portable_sb_diag_clear(&s);break;default:p.began=false;portable_sb_diag_clear(&s);portable_sb_diag_step(&s,&p,0,0,0,0,emit,0);break;}
 portable_sb_diag_step(&s,&p,1,-1,-1,0,emit,0);p=sample(3,233,233);p.released=true;portable_sb_diag_step(&s,&p,2,0,0,0,emit,0);assert(!count&&!s.active);++runs;}
 for(int bad=-257;bad<=257;bad+=514){portable_sb_diag s={0};count=0;portable_sb_diag_sample p=sample(1,233,233);portable_sb_diag_step(&s,&p,0,0,0,0,emit,0);p=sample(2,232,232);portable_sb_diag_step(&s,&p,1,bad,0,0,emit,0);assert(!s.active&&!count);++runs;}
 for(unsigned j=0;j<3;j++){unsigned dim=j==0?1:j==1?466:1024;portable_sb_diag s={0};count=0;portable_sb_diag_sample p=sample(1,dim-1,0);p.width=p.height=dim;portable_sb_diag_step(&s,&p,0,0,0,0,emit,0);p.sequence=2;p.began=false;p.released=true;portable_sb_diag_step(&s,&p,2,0,0,0,emit,0);assert(count==8&&!s.active);++runs;}
 printf("PASS bounded gesture helper %u cases; 10000 moves produce at most14 terminal records\n",runs);
}
