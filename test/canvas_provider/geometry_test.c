#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "canvas_geometry.h"
int main(void){
 /* Give all padding different bytes, then assign each semantic field. */
 canvas_mapping a,b;memset(&a,0xa5,sizeof(a));memset(&b,0x5a,sizeof(b));
 a.safe.x=b.safe.x=1;
 a.safe.y=b.safe.y=1;
 a.safe.width=b.safe.width=1;
 a.safe.height=b.safe.height=1;
 a.physical_width=b.physical_width=1;
 a.physical_height=b.physical_height=1;
 a.rotation=b.rotation=1;
 a.width=b.width=1;
 a.height=b.height=1;
 a.output_width=b.output_width=1;
 a.output_height=b.output_height=1;
 a.wide_x=b.wide_x=1;
 a.wide_y=b.wide_y=1;
 a.output_x=b.output_x=1;
 a.output_y=b.output_y=1;
 assert(canvas_mapping_equal(&a,&b));
 b.safe.x=2;assert(!canvas_mapping_equal(&a,&b));b.safe.x=1;
 b.safe.y=2;assert(!canvas_mapping_equal(&a,&b));b.safe.y=1;
 b.safe.width=2;assert(!canvas_mapping_equal(&a,&b));b.safe.width=1;
 b.safe.height=2;assert(!canvas_mapping_equal(&a,&b));b.safe.height=1;
 b.physical_width=2;assert(!canvas_mapping_equal(&a,&b));b.physical_width=1;
 b.physical_height=2;assert(!canvas_mapping_equal(&a,&b));b.physical_height=1;
 b.rotation=2;assert(!canvas_mapping_equal(&a,&b));b.rotation=1;
 b.width=2;assert(!canvas_mapping_equal(&a,&b));b.width=1;
 b.height=2;assert(!canvas_mapping_equal(&a,&b));b.height=1;
 b.output_width=2;assert(!canvas_mapping_equal(&a,&b));b.output_width=1;
 b.output_height=2;assert(!canvas_mapping_equal(&a,&b));b.output_height=1;
 b.wide_x=false;assert(!canvas_mapping_equal(&a,&b));b.wide_x=true;
 b.wide_y=false;assert(!canvas_mapping_equal(&a,&b));b.wide_y=true;
 b.output_x=2;assert(!canvas_mapping_equal(&a,&b));b.output_x=1;
 b.output_y=2;assert(!canvas_mapping_equal(&a,&b));b.output_y=1;
 unsigned cases=0;
 for(unsigned rot=0;rot<4;rot++)for(unsigned layout=1;layout<=3;layout++){
  risc_canvas_open_v1 r={sizeof(r),RISC_CANVAS_MONO1,layout,layout==3?0:320,layout==3?0:180};canvas_mapping m;
  assert(canvas_mapping_open(&m,466,400,rot,11,17,23,29,&r));
  unsigned valid=0;
  for(unsigned y=0;y<(rot%2?466:400);y++)for(unsigned x=0;x<(rot%2?400:466);x++){
   uint32_t sx,sy,px,py; if(!canvas_mapping_sample(&m,x,y,&sx,&sy))continue;
   canvas_mapping_physical(&m,x,y,&px,&py);assert(px<466&&py<400);
   int32_t tx,ty;assert(canvas_mapping_contact(&m,px,py,466,400,&tx,&ty));assert(tx==(int32_t)sx&&ty==(int32_t)sy);valid++;
  }
  assert(valid);cases++;
 }
 risc_canvas_open_v1 r={sizeof(r),1,3,0,0};canvas_mapping m,sentinel;
 memset(&sentinel,0xa5,sizeof(sentinel));m=sentinel;
 assert(!canvas_mapping_open(&m,466,466,0,233,0,233,0,&r));assert(!memcmp(&m,&sentinel,sizeof(m)));
 r.logical_width=1;assert(!canvas_mapping_open(&m,466,466,0,0,0,0,0,&r));
 r=(risc_canvas_open_v1){sizeof(r),1,1,UINT32_MAX,1};assert(!canvas_mapping_open(&m,466,466,0,0,0,0,0,&r));
 r=(risc_canvas_open_v1){sizeof(r),1,1,320,180};assert(canvas_mapping_open(&m,466,466,0,69,69,69,69,&r));
 int32_t x,y;assert(canvas_mapping_contact(&m,0,0,466,466,&x,&y));assert(x<0&&y<0);
 /* Independent physical corner expectations for a4x3surface, not inverse roundtrips. */
 r=(risc_canvas_open_v1){sizeof(r),1,3,0,0};
 const uint32_t expected[4][2]={{0,0},{3,0},{3,2},{0,2}};
 for(unsigned rotation=0;rotation<4;rotation++){
  assert(canvas_mapping_open(&m,4,3,rotation,0,0,0,0,&r));uint32_t px,py;
  canvas_mapping_physical(&m,0,0,&px,&py);assert(px==expected[rotation][0]&&py==expected[rotation][1]);
 }
 r=(risc_canvas_open_v1){sizeof(r),1,1,8,4};assert(canvas_mapping_open(&m,8,8,0,0,0,0,0,&r));
 uint32_t sx,sy;assert(!canvas_mapping_sample(&m,0,1,&sx,&sy));assert(canvas_mapping_sample(&m,0,2,&sx,&sy)&&sx==0&&sy==0);
 assert(canvas_mapping_sample(&m,7,5,&sx,&sy)&&sx==7&&sy==3);assert(!canvas_mapping_sample(&m,7,6,&sx,&sy));
 r.layout=2;assert(canvas_mapping_open(&m,8,8,0,0,0,0,0,&r));assert(canvas_mapping_sample(&m,0,0,&sx,&sy)&&sx==2&&sy==0);assert(canvas_mapping_sample(&m,7,7,&sx,&sy)&&sx==5&&sy==3);
 printf("%u geometry profile cases and refusal/captured-drag checks passed\n",cases);
}
