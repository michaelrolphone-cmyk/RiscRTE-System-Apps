#include "PortableQuickActions.h"
#include "PortableQuickRender.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(void){
 uint16_t small[240*240],large[414*502];
 risc_display_surface_v1 a={.pixels=small,.width=240,.height=240,.stride_bytes=480,.size_bytes=sizeof(small),.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 risc_display_surface_v1 b={.pixels=large,.width=410,.height=502,.stride_bytes=828,.size_bytes=sizeof(large),.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 pqa_state s;pqa_init(&s);pqa_set_levels(&s,true,40,true,50);
 for(unsigned step=1;step<=3;step++){
  s.position_q8=s.target_q8=step==1?160*256:PQA_OPEN_Q8;s.torch=step==3;
  for(unsigned i=0;i<240*240;i++)small[i]=0x1234;
  for(unsigned i=0;i<414*502;i++)large[i]=0xa5a5;
  for(unsigned y=0;y<502;y++)for(unsigned x=0;x<410;x++)large[y*414+x]=0x1234;
  assert(pqa_render(&a,&s,"12:34",true,75)&&pqa_render(&b,&s,"12:34",true,75));
  for(unsigned y=0;y<502;y++)for(unsigned x=0;x<414;x++){
   if(x>=410)assert(large[y*414+x]==0xa5a5);
   else if(y*240/410<240)assert(large[y*414+x]==small[(y*240/410)*240+x*240/410]);
  }
 }
 b.size_bytes=100;assert(!pqa_render(&b,&s,"12:34",true,75));
 puts("Quick native renderer: open/partial/torch match 240 reference per physical pixel; guards and invalid span PASS");
}
