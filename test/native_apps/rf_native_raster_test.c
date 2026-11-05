#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main
#include <time.h>
static void native_guards(void){for(unsigned y=0;y<RF_TEST_H;y++)for(unsigned x=RF_TEST_W;x<RF_TEST_W+3;x++)assert(mock_pixels[y*(RF_TEST_W+3)+x]==0xa55a);}
int main(void){
 /* The reused legacy fixture initializes a 240px pattern. This test initializes
  * the actual capability-sized raster and validates its complete stride. */
 assert(!mock_grants);mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 assert(!app_module_init());handoff_finish();assert(acquire_surface());
 size_t bytes=surface.size_bytes;uint8_t *initial=malloc(bytes),*expected=malloc(bytes);assert(initial&&expected);
 for(unsigned y=0;y<RF_TEST_H;y++)for(unsigned x=0;x<RF_TEST_W+3;x++)mock_pixels[y*(RF_TEST_W+3)+x]=x<RF_TEST_W?(uint16_t)(x*127+y*393):0xa55a;
 memcpy(initial,surface.pixels,bytes);unsigned checks=0,max_rows=0;uint64_t direct_work=0,replay_work=0;clock_t start=clock();
 const unsigned faces[]={0,1,2,6};const int positions[]={-30,-3,0,27,RF_TEST_H-8,RF_TEST_H+4};
 for(unsigned face=0;face<4;face++)for(unsigned pos=0;pos<6;pos++)for(unsigned narrow=0;narrow<3;narrow++){
  raster_command command={.kind=RS_NU_NATIVE_TEXT,.args={(int)faces[face],-4,positions[pos],narrow==0?1:narrow==1?55:RF_TEST_W+10,0x27c1ff},.text_present=true};
  strcpy(command.text," !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
  memcpy(surface.pixels,initial,bytes);raster_recording=raster_replaying=false;raster_pixels=0;raster_execute(&command);unsigned direct=raster_pixels;memcpy(expected,surface.pixels,bytes);
  memcpy(surface.pixels,initial,bytes);raster_pixels=0;raster_replaying=true;raster_command_span(&command);unsigned rows=0;
  for(;raster_band_top<raster_band_limit;raster_band_top++){raster_band_bottom=raster_band_top+1;raster_execute(&command);rows++;}
  raster_replaying=false;assert(!memcmp(expected,surface.pixels,bytes));assert(raster_pixels<=direct);assert(rows<=24);if(rows>max_rows)max_rows=rows;direct_work+=direct;replay_work+=raster_pixels;native_guards();checks++;
 }
 for(unsigned pos=0;pos<6;pos++){
  raster_command command={.kind=RS_NU_NATIVE_ROUND,.args={-12,positions[pos],RF_TEST_W+30,47,19,0x27c1ff}};
  memcpy(surface.pixels,initial,bytes);raster_replaying=false;raster_pixels=0;raster_execute(&command);unsigned direct=raster_pixels;memcpy(expected,surface.pixels,bytes);
  memcpy(surface.pixels,initial,bytes);raster_pixels=0;raster_replaying=true;raster_command_span(&command);
  for(;raster_band_top<raster_band_limit;raster_band_top++){raster_band_bottom=raster_band_top+1;raster_execute(&command);}
  raster_replaying=false;assert(!memcmp(expected,surface.pixels,bytes));assert(raster_pixels==direct);native_guards();checks++;
 }
 memcpy(expected,surface.pixels,bytes);failed=true;raster_pixels=0;portable_nova_native_text(1,1,1,120,"RETAINED",0xffffff);portable_nova_native_fill(0,0,RF_TEST_W,RF_TEST_H,0xffffff);assert(!raster_pixels&&!memcmp(expected,surface.pixels,bytes));failed=false;checks++;
 double ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;free(expected);free(initial);app_module_fini();assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live);native_guards();
 printf("native raster %dx%d: comparisons=%u glyph-band-max=%u direct-pixels=%llu replay-pixels=%llu host-cpu-ms=%.3f PASS\n",RF_TEST_W,RF_TEST_H,checks,max_rows,(unsigned long long)direct_work,(unsigned long long)replay_work,ms);return 0;
}
