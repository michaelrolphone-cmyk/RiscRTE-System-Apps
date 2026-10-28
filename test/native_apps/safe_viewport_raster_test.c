#if defined(TEST_SETTINGS_VIEW)
#define PORTABLE_SETTINGS_APP
#elif defined(TEST_WIFI_VIEW)
#define PORTABLE_WIFI_SETTINGS_APP
#endif
#define PORTABLE_SAFE_VIEWPORT
#define PORTABLE_FORCE_FULL_FRAMES
#include "../../lib/PortableApps/src/adapter.c"
#include <assert.h>
#include <stdio.h>
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};const unsigned portable_catalog_count=0;
static bool healthy(risc_runtime_health_v1*h){h->uptime_ms=0;return true;}static void wait_(uint32_t n){(void)n;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=healthy,.yield_ms=wait_};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
int main(void){rt=&runtime;
 for(unsigned w=240;w<=700;w+=23)for(unsigned h=240;h<=800;h+=29){
#if defined(TEST_SETTINGS_VIEW) || defined(TEST_WIFI_VIEW)
  if(w<268||h<272)continue;
#endif
  info=(risc_display_info_v1){.width=w,.height=h};surface_format=RISC_DISPLAY_FORMAT_RGB565;paper_rotated=false;failed=false;
  assert(portable_safe_viewport_open(&content_view,w,h,17,13,11,19));
  size_t stride=w*2+12,size=stride*h;uint8_t*p=malloc(size+128);assert(p);memset(p,0xa5,size+128);
  surface=(risc_display_surface_v1){.frame=1,.pixels=p+64,.width=w,.height=h,.stride_bytes=stride,.size_bytes=size,.pixel_format=surface_format};
  fill(-20,-20,content_view.width+40,content_view.height+40,0x07e0);
  np_pixel(0,0,0x0000ff,255);
  unsigned text_x=17+(content_view.width-240)/2,text_y=13+(content_view.height-240)/2;
#if defined(TEST_SETTINGS_VIEW)
  sv_pixel(0,0,0xff0000,255);
#elif defined(TEST_WIFI_VIEW)
  wv_pixel(0,0,0xff0000,255);
#else
  (void)text_x;(void)text_y;
#endif
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
   const bool inside=x>=17&&x<w-11&&y>=13&&y<h-19;uint16_t value;memcpy(&value,p+64+y*stride+x*2,2);uint16_t expected=inside?0x07e0:0xa5a5;if(x==17&&y==13)expected=0x001f;
#if defined(TEST_SETTINGS_VIEW) || defined(TEST_WIFI_VIEW)
   if(x==text_x&&y==text_y)expected=0xf800;
#endif
   assert(value==expected);
  }
  for(unsigned y=0;y<h;y++)for(size_t x=w*2;x<stride;x++)assert(p[64+y*stride+x]==0xa5);
  for(unsigned i=0;i<64;i++)assert(p[i]==0xa5&&p[64+size+i]==0xa5);free(p);surface=(risc_display_surface_v1){0};
 }
 puts("PASS actual adapter RGB565 fill: generated safe bounds, translated pixel columns/rows, clipping, stride padding and allocation guards");
}
