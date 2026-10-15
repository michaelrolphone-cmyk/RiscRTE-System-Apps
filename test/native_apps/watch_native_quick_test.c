#include "PortableQuickRender.h"
#include "WatchLayout.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void) {
 const unsigned width=410,height=502,pitch=827,logical_height=294;
 const size_t capacity=(size_t)pitch*height;
 uint8_t *storage=malloc(capacity+2),*logical=malloc(480u*logical_height);
 assert(storage&&logical);memset(storage,0xa5,capacity+2);
 risc_display_surface_v1 f={.pixels=storage+1,.width=width,.height=height,.stride_bytes=pitch,.size_bytes=capacity,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 risc_display_surface_v1 g={.pixels=logical,.width=240,.height=logical_height,.stride_bytes=480,.size_bytes=480u*logical_height,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 pqa_state s;pqa_init(&s);assert(pqa_set_height(&s,logical_height));
 pqa_set_levels(&s,true,60,true,50);s.contexts_controls=s.contexts_valid=true;
 for(unsigned mode=0;mode<4;mode++) {
  memset(logical,0x52,480u*logical_height);
  for(unsigned y=0;y<height;y++)memset(storage+1+y*pitch,0x52,width*2u);
  s.position_q8=s.target_q8=(mode==1?173:(int)logical_height)*256;
  s.torch=mode==2;s.error_flags=mode==3?PQA_ERROR_SAVE:0;
  assert(pqa_render(&f,&s,"10:08",true,73));assert(pqa_render(&g,&s,"10:08",true,73));
  for(unsigned y=0;y<height;y++) {
   for(unsigned x=0;x<width;x++)assert(!memcmp(storage+1+y*pitch+x*2,logical+(y*240/width)*480+(x*240/width)*2,2));
   for(unsigned x=width*2;x<pitch;x++)assert(storage[1+y*pitch+x]==0xa5);
  }
  assert(storage[0]==0xa5 && storage[capacity+1]==0xa5);
 }
 s.torch=false;s.position_q8=s.target_q8=pqa_open_q8(&s);s.neutral_gate=false;
 assert(pqa_input(&s,10,true,1,1,120,280,false));
 assert(s.handle_pressed);assert(pqa_input(&s,20,true,0,1,120,280,false));assert(s.target_q8==0);
 assert(!pqa_set_height(&s,240));pqa_cancel(&s);assert(pqa_set_height(&s,240));
 s.logical_height=294;f.size_bytes=(height-1)*pitch+width*2u-1;assert(!pqa_render(&f,&s,"",false,0));
 f.size_bytes=capacity;f.width=0;assert(!pqa_render(&f,&s,"",false,0));
 free(storage);free(logical);
 puts("Native Quick: uniform-pixel identity, full-height overlay/torch, footer hit, unaligned padded/truncated surfaces passed");
 return 0;
}
