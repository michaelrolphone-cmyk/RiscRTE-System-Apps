/* Actual production renderer, no mock presentation. Compare every RGB565 byte
 * and row guard against the restored launcher at square and native dimensions. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define PORTABLE_NOVA_UI
#define PORTABLE_WATCH_NATIVE_HEIGHT
#include "../../lib/PortableApps/src/adapter.c"
#include "springboard_raster_reference.inc"
const t5_app_manifest_t portable_catalog[]={{.display_name="Stopwatch",.icon="solid:f2f2"}};
const unsigned portable_catalog_count=1;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
static uint16_t *actual,*expected;static size_t bytes;
static void reset(void){
 for(size_t n=0;n<bytes/2;n++)actual[n]=(uint16_t)(n*31337u+17u);
 memcpy(expected,actual,bytes);surface.pixels=actual;surface.frame=1;
}
static void equal(void){assert(!memcmp(actual,expected,bytes));}
static void scene(bool reference){
 for(int r=1;r<=30;r++) {
  if(reference)reference_np_circle(13+(r*31)%220,12+(r*47)%(np_height()-24),r,0xed4f54,255);
  else np_circle(13+(r*31)%220,12+(r*47)%(np_height()-24),r,0xed4f54,255);
 }
 nova_mask_pending=true;if(reference)reference_np_mask();else np_mask();
 np_caption(16,"10:21 AM",true,0xffffff);np_caption(np_height()-43,"STOPWATCH",false,0xffffff);
}
int main(void){
 const unsigned dimensions[][2]={{240,240},{410,502},{320,390},{239,293},{1,2},{128,128}};
 for(unsigned k=0;k<sizeof(dimensions)/sizeof(*dimensions);k++){
  unsigned w=dimensions[k][0],h=dimensions[k][1];assert(watch_layout_open(&native_watch_layout,w,h,NULL));
  info.width=w;info.height=h;bytes=(w+3)*h*2;actual=malloc(bytes);expected=malloc(bytes);assert(actual&&expected);
  surface=(risc_display_surface_v1){.frame=1,.width=w,.height=h,.stride_bytes=(w+3)*2,.size_bytes=bytes,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
  reset();nova_mask_pending=true;np_mask();surface.pixels=expected;nova_mask_pending=true;reference_np_mask();equal();
  for(unsigned a=0;a<=256;a++){
   reset();for(int y=-1;y<=np_height();y+=7)for(int x=-1;x<=240;x+=7)native_watch_pixel(x,y,0xbad152,a);
   surface.pixels=expected;for(int y=-1;y<=np_height();y+=7)for(int x=-1;x<=240;x+=7)reference_native_watch_pixel(x,y,0xbad152,a);equal();
  }
  for(int r=1;r<=64;r++)for(unsigned a=0;a<256;a+=51){reset();np_circle(32,32,r,0xabcdef,a);surface.pixels=expected;reference_np_circle(32,32,r,0xabcdef,a);equal();}
  reset();scene(false);surface.pixels=expected;scene(true);equal();
  double times[2];for(int ref=0;ref<2;ref++){surface.pixels=actual;clock_t start=clock();for(int n=0;n<30;n++)scene(ref);times[ref]=(double)(clock()-start)/CLOCKS_PER_SEC;}
  printf("PASS %ux%u pixel equality; host scene old=%.6fs new=%.6fs ratio=%.3f (not target timings)\n",w,h,times[1],times[0],times[0]/times[1]);
  free(actual);free(expected);
 }
}
