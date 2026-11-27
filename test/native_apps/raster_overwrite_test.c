#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static uint8_t *provider,*software;static size_t provider_bytes,software_bytes,provider_cleared,software_cleared;
static void *tracked_clear(void *p,int v,size_t n){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)provider,c=(uintptr_t)software;
 if(provider&&a>=b&&a-b<provider_bytes)provider_cleared+=n;
 if(software&&a>=c&&a-c<software_bytes)software_cleared+=n;
 return memset(p,v,n);
}
#include <stdbool.h>
static bool refuse_software;
static void *tracked_alloc(size_t n){if(refuse_software&&n==software_bytes){refuse_software=false;return NULL;}return malloc(n);}
#define malloc tracked_alloc
#define memset tracked_clear
#define PORTABLE_SAFE_VIEWPORT
#define PORTABLE_FORCE_FULL_FRAMES
#define PORTABLE_RASTER_SNAPSHOT
#include "../../lib/PortableApps/src/adapter.c"
#undef memset
#undef malloc
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};const unsigned portable_catalog_count=0;
static unsigned submitted,acquired,clock_calls,fail_clock;static bool invalid_surface,reject_submit;
static bool clock_read(risc_runtime_health_v1*h){++clock_calls;if(fail_clock&&clock_calls==fail_clock)failed=true;h->uptime_ms=0;return true;}
static void wait_(uint32_t n){(void)n;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=clock_read,.yield_ms=wait_};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
static bool acquire_(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;++acquired;*out=(risc_display_surface_v1){.frame=1,.pixels=provider,.width=info.width,.height=info.height,.stride_bytes=info.width*2+12,.size_bytes=provider_bytes,.pixel_format=f};if(invalid_surface)out->width--;return true;}
static void release_(void*c,risc_display_frame_v1 f){(void)c;(void)f;}
static bool submit_(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)f;(void)d;(void)n;(void)o;++submitted;*t=1;return !reject_submit;}
static bool status_(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){(void)c;(void)t;*out=(risc_display_present_status_v1){.state=RISC_DISPLAY_PRESENT_COMPLETE};return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.acquire=acquire_,.release=release_,.submit=submit_,.present_status=status_};
int main(void){
 unsigned cases=0;rt=&runtime;display=&display_api;
 for(unsigned w=240;w<=466;w+=113)for(unsigned extra=0;extra<=2;extra+=2)for(unsigned inset=0;inset<2;inset++)for(unsigned fault=0;fault<5;fault++){
  info=(risc_display_info_v1){.width=w,.height=w+extra};surface_format=RISC_DISPLAY_FORMAT_RGB565;paper_rotated=false;failed=false;
  content_view=(portable_safe_viewport){inset?7:0,inset?9:0,w-(inset?14:0),w+extra-(inset?18:0)};
#ifdef PORTABLE_RASTER_OVERWRITE_CLEAR
  raster_command whole={.kind=RS_FILL,.args={0,0,w,w+extra,0}};
  assert(raster_initializes_all_pixels(&whole)==!inset);
  whole.args[2]--;assert(!raster_initializes_all_pixels(&whole));whole.args[2]++;
  whole.kind=RS_BITMAP;assert(!raster_initializes_all_pixels(&whole));whole.kind=RS_FILL;
  whole.paper_offset=1;assert(!raster_initializes_all_pixels(&whole));whole.paper_offset=0;
  surface_format=RISC_DISPLAY_FORMAT_MONO1;assert(!raster_initializes_all_pixels(&whole));surface_format=RISC_DISPLAY_FORMAT_RGB565;
  paper_rotated=true;assert(!raster_initializes_all_pixels(&whole));paper_rotated=false;
#ifdef PORTABLE_TOUCH_SCROLL
  whole.clipped=true;assert(!raster_initializes_all_pixels(&whole));whole.clipped=false;
#endif
#endif
  provider_bytes=(w*2+12)*(w+extra);software_bytes=w*(w+extra)*2;provider=malloc(provider_bytes+64);software=malloc(software_bytes);assert(provider&&software);
  memset(provider,0xa5,provider_bytes+64);memset(software,0x5a,software_bytes);raster_offscreen=fault==4?NULL:software;refuse_software=fault==4;
  provider_cleared=software_cleared=submitted=acquired=clock_calls=0;fail_clock=fault==1?2:0;invalid_surface=fault==2;reject_submit=fault==3;
  surface=(risc_display_surface_v1){0};paper_token=0;raster_begin_allowed=true;raster_recording=raster_sealed=raster_replaying=raster_started=false;
  clear_color(0x2468);fill(2,3,4,5,0x1357);present(false);
  for(unsigned i=0;raster_sealed&&!failed&&i<10000;i++)assert(raster_progress()||failed);
  if(!fault||fault==4){assert(submitted==1);
   for(unsigned y=0;y<w+extra;y++)for(unsigned x=0;x<w;x++){
    unsigned expected=0;if(x>=content_view.x&&x<content_view.x+content_view.width&&y>=content_view.y&&y<content_view.y+content_view.height)expected=0x2468;
    if(x>=content_view.x+2&&x<content_view.x+6&&y>=content_view.y+3&&y<content_view.y+8)expected=0x1357;
    uint16_t actual;memcpy(&actual,provider+y*(w*2+12)+x*2,2);assert(actual==expected);
   }
#ifdef PORTABLE_RASTER_OVERWRITE_CLEAR
   assert(provider_cleared==(fault==4?software_bytes:0));assert(software_cleared==(fault==4?0:inset?software_bytes:0));
#else
   assert(provider_cleared==software_bytes&&software_cleared==(fault==4?0:software_bytes));
#endif
  }else assert(failed);
  if(fault==1||fault==2)assert(submitted==0);
  for(unsigned y=0;y<w+extra;y++)for(unsigned x=w*2;x<w*2+12;x++)assert(provider[y*(w*2+12)+x]==0xa5);
  for(unsigned x=0;x<64;x++)assert(provider[provider_bytes+x]==0xa5);
  failed=false;raster_free_commands();raster_sealed=raster_recording=raster_replaying=false;raster_offscreen=NULL;surface=(risc_display_surface_v1){0};free(provider);free(software);provider=software=NULL;++cases;
 }
 printf("Overwrite clear: %u full/partial viewport, padded, failure/rejection cases PASS\n",cases);return 0;
}
