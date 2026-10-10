/* Actual adapter, asynchronous display and packed surface compositor. */
#define RASTER_LATENCY_NO_MAIN
#include "portable_raster_latency_test.c"
static void pattern(void) {
 for(int y=0;y<height();y+=23)fill((y*7)%width()-9,y,width()/2+13,11,0);
 text_color(21,97,"CACHED PIXELS",30,0);pp_circle(width()/2,height()/2,43,true);
}
static unsigned read_logical(const uint8_t *pixels,unsigned stride,int x,int y) {
 int px=paper_rotated?y:x,py=paper_rotated?(int)info.height-1-x:y;
#ifdef PORTABLE_PAPER_PREFERENCES
 if(paper_flip_ui){px=(int)info.width-1-px;py=(int)info.height-1-py;}
#endif
 return !!(pixels[(size_t)py*stride+px/8]&(128u>>(px&7)));
}
static void write_logical(uint8_t *pixels,unsigned stride,int x,int y,unsigned bit) {
 int px=paper_rotated?y:x,py=paper_rotated?(int)info.height-1-x:y;
#ifdef PORTABLE_PAPER_PREFERENCES
 if(paper_flip_ui){px=(int)info.width-1-px;py=(int)info.height-1-py;}
#endif
 uint8_t *b=&pixels[(size_t)py*stride+px/8],mask=128u>>(px&7);
 *b=(*b&~mask)|(bit?mask:0);
}
static void layers_case(bool rotated) {
 begin_test();mock_format=RISC_DISPLAY_FORMAT_MONO1;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_RETAINS_IMAGE;
 assert(!app_module_init());paper_rotated=rotated;handoff_finish();
 portable_raster_layer *cache=NULL;
 assert(portable_paper_frame_ready());clear();assert(portable_layer_begin(&cache));pattern();portable_raster_layer_end();
 portable_raster_layer_blit(cache,0,0,width(),height(),0,0);present(true);assert(portable_paper_frame_drain());
 assert(cache->complete && cache->refs==1 && !surface.frame);
 const size_t bytes=surface.size_bytes;unsigned char *expected=malloc(bytes),*submitted=malloc(bytes);assert(expected&&submitted);
 unsigned comparisons=0;
 const int offsets[]={-801,-479,-129,-8,-1,0,1,7,13,127,479,801};
 for(unsigned flip=0;flip<2;flip++) {
#ifdef PORTABLE_PAPER_PREFERENCES
  paper_flip_ui=flip!=0;
#endif
  if(!portable_layer_valid(cache)) {
   assert(portable_paper_frame_ready());clear();assert(portable_layer_begin(&cache));pattern();portable_raster_layer_end();
   portable_raster_layer_blit(cache,0,0,width(),height(),0,0);present(true);assert(portable_paper_frame_drain());
  }
  for(unsigned area=0;area<3;area++)for(unsigned i=0;i<12;i++)for(unsigned j=0;j<12;j++) {
   int dx=offsets[i],dy=offsets[j],sx0=area?29:0,sy0=area?73:0;
   int sw=area?width()-77:width(),sh=area?height()-99:height();
   raster_clip_active=area==2;raster_clip=(portable_scroll_viewport){37,61,width()-83,height()-120};
   unsigned allocations=mock_allocations;
   /* Clear outside the blit clip so untouched pixels have a known reference. */
   raster_clip_active=false;assert(portable_paper_frame_ready());clear();raster_clip_active=area==2;
   portable_raster_layer_blit(cache,sx0,sy0,sw,sh,dx,dy);raster_clip_active=false;
   assert(raster_command_count==2);assert(mock_allocations==allocations);
   memcpy(expected,mock_pixels,bytes);
   for(unsigned row=0;row<info.height;row++)memset(expected+(size_t)row*surface.stride_bytes,0,(info.width+7)/8);
   for(int y=0;y<height();y++)for(int x=0;x<width();x++) {
    int sx=x-dx+sx0,sy=y-dy+sy0;
    if((area!=2||portable_scroll_inside(&raster_clip,x,y))&&sx>=sx0&&sy>=sy0&&sx<sx0+sw&&sy<sy0+sh)write_logical(expected,surface.stride_bytes,x,y,read_logical(cache->pixels,cache->stride,sx,sy));
   }
   present(false);assert(portable_paper_frame_drain());
   assert(!memcmp(expected,mock_pixels,bytes));assert(mock_allocations==allocations);++comparisons;
  }
 }
 /* Replacing a cache after recording a blit keeps the old generation alive. */
 assert(portable_paper_frame_ready());clear();portable_raster_layer *old=cache;
 portable_raster_layer_blit(old,0,0,width(),height(),0,0);assert(old->refs==2);
 assert(portable_layer_begin(&cache));fill(0,0,width(),height(),0);portable_raster_layer_end();
 assert(cache!=old && old->refs==1);present(false);assert(portable_paper_frame_drain());
 /* The recorded first blit used the old image, not the replacement's black pixels. */
 assert(read_logical((const uint8_t*)mock_pixels,surface.stride_bytes,width()-1,height()-1)==0);
 /* While a submitted frame is BUSY, finish and replace software images. The
  * provider-owned pixels remain byte-identical until its token completes. */
 mock_latency=1000;
 assert(portable_paper_frame_ready());clear();portable_raster_layer_blit(cache,0,0,width(),height(),0,0);present(false);
 while(raster_sealed){assert(raster_progress());mock_ms++;}
 assert(paper_token);memcpy(submitted,mock_pixels,bytes);
 for(unsigned i=0;i<3;i++) {
  assert(portable_paper_frame_ready());clear();portable_raster_layer_blit(cache,0,0,width(),height(),(int)i+1,0);present(false);
  while(!raster_ready_to_submit){assert(raster_progress());mock_ms++;}
  assert(paper_token && !surface.frame && !memcmp(submitted,mock_pixels,bytes));
 }
 assert(portable_paper_frame_drain());portable_layer_release(&cache);free(expected);free(submitted);
 end_test();assert(!mock_bytes);
 printf("layers: rotated=%u comparisons=%u; warm frames=2 commands/0 allocations; retained generations and BUSY replacement PASS\n",rotated,comparisons);
}
int main(void){setvbuf(stdout,NULL,_IONBF,0);layers_case(false);layers_case(true);return 0;}
