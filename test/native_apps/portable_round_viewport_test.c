#include <assert.h>
#include <stdio.h>
#include "PortableRoundViewport.h"
int main(void){unsigned sizes[][2]={{240,240},{300,300},{466,466},{465,465},{400,480},{480,400}};unsigned tested=0;
 for(unsigned s=0;s<sizeof(sizes)/sizeof(*sizes);++s)for(unsigned margin=0;margin<=12;margin+=4){unsigned w=sizes[s][0],h=sizes[s][1];
  for(unsigned top=0;top<h;++top)for(unsigned span=1;span<=60;span+=7){unsigned bottom=top+span;if(bottom>h)continue;portable_round_span b;
   bool ok=portable_round_band(w,h,top,bottom,margin,&b);
   if(ok){assert(b.left>=0&&b.right<=(int)w);for(unsigned y=top;y<bottom;++y){assert(portable_round_contains(w,h,b.left,y,margin));assert(portable_round_contains(w,h,b.right-1,y,margin));}++tested;}
  }}printf("PASS %u full-band chord rectangles, odd/even/rectangular rasters and radial margins\n",tested);
}
