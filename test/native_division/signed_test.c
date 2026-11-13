#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
extern int64_t __divdi3(int64_t,int64_t);
extern uint64_t __udivdi3(uint64_t,uint64_t);
static uint64_t seed=UINT64_C(0x93a58cfedb702169);
static uint64_t next(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static void check(int64_t a,int64_t b){
 if(!b){assert(__divdi3(a,b)==0);return;}
 if(a==INT64_MIN&&b==-1){assert(__divdi3(a,b)==INT64_MIN);return;}
 assert(__divdi3(a,b)==a/b);
}
int main(void){
 const int64_t values[]={0,1,-1,2,-2,INT64_MAX,INT64_MIN,INT64_MIN+1,INT32_MAX,INT32_MIN,INT64_C(0x100000000),-INT64_C(0x100000000)};
 for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++)for(unsigned j=0;j<sizeof(values)/sizeof(*values);j++)check(values[i],values[j]);
 for(unsigned i=0;i<20000;i++){uint64_t a=next(),b=next();check((int64_t)a,(int64_t)b);assert(__udivdi3(a,b)==a/b);}
 puts("signed/unsigned compiler support 20144 pairs PASS");return 0;
}
