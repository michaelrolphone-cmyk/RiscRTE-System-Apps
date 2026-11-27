#define RASTER_LATENCY_NO_MAIN
#include "portable_raster_latency_test.c"
#include CIRCLE_REFERENCE
static unsigned circle_comparisons;
static void circle_case(unsigned format) {
 begin_test();mock_format=format;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
  (format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0);
 assert(!app_module_init());handoff_finish();assert(acquire_surface());
 const size_t bytes=surface.size_bytes;unsigned char*initial=malloc(bytes),*expected=malloc(bytes);assert(initial&&expected);
 memcpy(initial,surface.pixels,bytes);unsigned active=format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<active;x++)initial[(size_t)y*surface.stride_bytes+x]=(unsigned char)((y*active+x)*73u+29u);
 const unsigned radii[]={1,2,7,31,64,127,256};const unsigned alphas[]={0,1,127,254,255};
 for(unsigned r=1;r<=256;r++){
  uint32_t rgb=r*47917u;
  memcpy(surface.pixels,initial,bytes);np_circle_reference(width()/2,height()/2,r,rgb,255);memcpy(expected,surface.pixels,bytes);
  memcpy(surface.pixels,initial,bytes);np_circle(width()/2,height()/2,r,rgb,255);
  assert(!memcmp(expected,surface.pixels,bytes));++circle_comparisons;
 }
 const int xy[][2]={{-5,7},{0,0},{10,-9},{200,233},{465,465},{800,800}};
 for(unsigned a=0;a<sizeof(alphas)/sizeof(*alphas);a++)for(unsigned r=0;r<sizeof(radii)/sizeof(*radii);r++)for(unsigned p=0;p<sizeof(xy)/sizeof(*xy);p++){
  uint32_t rgb=0x875ac3u;
  memcpy(surface.pixels,initial,bytes);raster_replaying=false;np_circle_reference(xy[p][0],xy[p][1],radii[r],rgb,alphas[a]);memcpy(expected,surface.pixels,bytes);
  for(unsigned sliced=0;sliced<2;sliced++){
   memcpy(surface.pixels,initial,bytes);raster_replaying=!!sliced;raster_band_columns=false;
   if(sliced)for(raster_band_top=0;raster_band_top<height();raster_band_top+=7){raster_band_bottom=raster_band_top+7;if(raster_band_bottom>height())raster_band_bottom=height();np_circle(xy[p][0],xy[p][1],radii[r],rgb,alphas[a]);}
   else np_circle(xy[p][0],xy[p][1],radii[r],rgb,alphas[a]);
   raster_replaying=false;assert(!memcmp(expected,surface.pixels,bytes));++circle_comparisons;
  }
 }
#ifdef PORTABLE_TOUCH_SCROLL
 raster_clip_active=true;raster_clip=(portable_scroll_viewport){37,41,19,13};
 memcpy(surface.pixels,initial,bytes);np_circle_reference(60,60,47,0x24abcd,255);memcpy(expected,surface.pixels,bytes);
 memcpy(surface.pixels,initial,bytes);np_circle(60,60,47,0x24abcd,255);assert(!memcmp(expected,surface.pixels,bytes));++circle_comparisons;
 raster_clip_active=false;
#endif
 free(expected);free(initial);end_test();assert(!mock_bytes);
 printf("circle spans format=%u cumulative complete-buffer comparisons=%u PASS\n",format,circle_comparisons);
}
int main(void){setvbuf(stdout,NULL,_IONBF,0);circle_case(RISC_DISPLAY_FORMAT_RGB565);
#ifndef RASTER_RGB466
circle_case(RISC_DISPLAY_FORMAT_MONO1);
#endif
return 0;}
