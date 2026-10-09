#include "PortablePaperTransition.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 uint8_t incoming[64],old[64],previous[64],rendered[64];
 for(unsigned width=1;width<=24;width++)for(unsigned height=1;height<=7;height++){
  unsigned row=(width+7)/8,stride=row+3,old_stride=row+2;
  memset(old,0xa5,sizeof(old));memset(rendered,0x5a,sizeof(rendered));
  memcpy(previous,old,sizeof(old));
  for(unsigned coverage=0;coverage<=16;coverage++){
   memcpy(incoming,rendered,sizeof(incoming));assert(portable_paper_blend(incoming,stride,old,old_stride,width,height,coverage));
   for(unsigned y=0;y<height;y++)for(unsigned x=0;x<stride;x++){
    if(x>=row){assert(incoming[y*stride+x]==rendered[y*stride+x]);continue;}
    for(unsigned bit=0;bit<8;bit++){
     unsigned mask=128u>>bit;bool value=!!(incoming[y*stride+x]&mask),fresh=!!(rendered[y*stride+x]&mask),prior=!!(old[y*old_stride+x]&mask);
     if(x*8+bit>=width)assert(value==fresh);
     else {if(!coverage)assert(value==prior);if(coverage==16)assert(value==fresh);
      if(coverage&&!!(previous[y*stride+x]&mask)!=prior)assert(value==fresh);}
    }
   }
   memcpy(previous,incoming,sizeof(previous));
  }
 }
 memset(incoming,0x17,sizeof(incoming));memcpy(previous,incoming,sizeof(previous));
 assert(!portable_paper_blend(NULL,4,old,4,24,7,8));
 assert(!portable_paper_blend(incoming,2,old,4,24,7,8));
 assert(!portable_paper_blend(incoming,4,old,2,24,7,8));
 assert(!portable_paper_blend(incoming,4,old,4,0,7,8));
 assert(!portable_paper_blend(incoming,4,old,4,24,7,17));
 assert(!memcmp(incoming,previous,sizeof(incoming)));
 puts("MONO1 blend: 168 padded odd geometries, all 17 coverage levels and invalid-input preservation PASS");return 0;
}
