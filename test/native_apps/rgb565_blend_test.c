#include "PortableRgb565Blend.h"
#include <assert.h>
#include <stdio.h>
static uint16_t reference(uint16_t before,uint32_t rgb,unsigned alpha){if(alpha>255)alpha=255;unsigned r=(((before>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;unsigned g=(((before>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;unsigned b=((before&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));}
int main(void){unsigned long long cases=0;for(unsigned n=0;n<=65025;n++)assert(portable_rgb565_div255(n)==n/255);
for(unsigned channel=0;channel<3;channel++){unsigned bits=channel==1?6:5,shift=channel==0?11:channel==1?5:0,source_shift=16-channel*8;for(unsigned old=0;old<(1u<<bits);old++)for(unsigned src=0;src<256;src++)for(unsigned alpha=0;alpha<512;alpha++){uint16_t before=(uint16_t)(old<<shift);uint32_t rgb=src<<source_shift;assert(reference(before,rgb,alpha)==portable_rgb565_over(before,rgb,alpha));cases++;}}
uint32_t state=1;for(unsigned i=0;i<1000000;i++){state=state*1664525+1013904223;uint16_t before=state;state=state*1664525+1013904223;uint32_t rgb=state;unsigned alpha=(state>>24)*2;assert(reference(before,rgb,alpha)==portable_rgb565_over(before,rgb,alpha));cases++;}
printf("PASS %llu channel/full-pixel cases;65026 exact bounded divisions\n",cases);}
