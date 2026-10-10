/* Full-byte oracle: unchanged renderer versus whole and sliced replay paths. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "PortableQuickRender.h"
extern bool baseline_pqa_render(risc_display_surface_v1*,const pqa_state*,const char*,bool,uint8_t);
static unsigned cases;
static double max_row_ms,max_full_ms;
static void compare(const pqa_state *state,unsigned stride,unsigned first,unsigned count,const char *label,bool battery,uint8_t percent) {
 size_t bytes=(size_t)stride*240+19;uint8_t *a=malloc(bytes),*b=malloc(bytes),*c=malloc(bytes);assert(a&&b&&c);
 for(size_t i=0;i<bytes;i++)a[i]=b[i]=c[i]=(uint8_t)(i*37u+(i/257u));
 risc_display_surface_v1 x={.pixels=a+1,.width=240,.height=240,.stride_bytes=stride,.size_bytes=(uint32_t)(bytes-2),.pixel_format=RISC_DISPLAY_FORMAT_RGB565},y=x,z=x;
 y.pixels=b+1;z.pixels=c+1;
 assert(baseline_pqa_render(&x,state,label,battery,percent));clock_t full_start=clock();assert(pqa_render(&y,state,label,battery,percent));double full_ms=1000.0*(clock()-full_start)/CLOCKS_PER_SEC;if(full_ms>max_full_ms)max_full_ms=full_ms;assert(!memcmp(a,b,bytes));
 for(unsigned row=first;row<first+count;) {unsigned n=(row%3==0)?1u:(row%3==1)?7u:32u;if(n>first+count-row)n=first+count-row;
  clock_t row_start=clock();assert(pqa_render_rows(&z,state,label,battery,percent,row,n));double row_ms=1000.0*(clock()-row_start)/CLOCKS_PER_SEC;if(n==1&&row_ms>max_row_ms)max_row_ms=row_ms;row+=n;}
 for(unsigned row=0;row<240;row++) {
  const uint8_t *wanted=row>=first&&row<first+count?a:c;
  if(wanted==a)assert(!memcmp(a+1+(size_t)row*stride,c+1+(size_t)row*stride,stride));
  else for(unsigned col=0;col<stride;col++){size_t at=1+(size_t)row*stride+col;assert(c[at]==(uint8_t)(at*37u+(at/257u)));}
 }
 assert(c[0]==a[0]);assert(!memcmp(a+1+(size_t)stride*240,c+1+(size_t)stride*240,bytes-1-(size_t)stride*240));assert(!pqa_render_rows(&z,state,label,battery,percent,241,0));assert(!pqa_render_rows(&z,state,label,battery,percent,239,2));
 free(a);free(b);free(c);cases++;
}
int main(void) {
 const char bounded[8]={'2','3',':','5','9','A','B','C'};
 for(unsigned pos=0;pos<=240;pos++) {
  pqa_state state;pqa_init(&state);state.position_q8=(int)pos*256+(pos%3?113:0);state.target_q8=state.position_q8;
  state.brightness_valid=state.volume_valid=state.dnd_valid=state.radios_valid=true;
  state.brightness=pos%101;state.volume=(pos*3)%101;state.dnd_enabled=(pos&1)!=0;
  state.airplane=(pos&2)!=0;state.wifi_enabled=(pos&4)!=0;state.bluetooth_enabled=(pos&8)!=0;state.radio_controls=true;
  state.error_flags=(uint32_t)pos;compare(&state,pos&1?486:480,0,240,bounded,pos&1,(uint8_t)pos);
  if(!(pos%17)){state.torch=true;compare(&state,487,0,240,NULL,true,0);compare(&state,486,37,119,bounded,false,255);}
 }
 printf("Quick row renderer: %u complete-byte baseline/sliced cases passed; max one-row host CPU %.6f ms, max full host CPU %.6f ms\n",cases,max_row_ms,max_full_ms);return 0;
}
