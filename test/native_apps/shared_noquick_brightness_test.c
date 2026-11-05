/* Without Quick Actions, no brightness preference, grant, UI or write is added. */
#define main existing_toolbar_main
#include TOOLBAR_FIXTURE
#undef main
static unsigned noquick_brightness_calls;
static uint8_t noquick_pixels[410*502*2];
static bool noquick_info(void*c,risc_display_info_v1*out){
 bool ok=fx_info(c,out);out->width=410;out->height=502;
 out->flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_BRIGHTNESS;
 out->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);return ok;
}
static bool noquick_frame(void*c,uint32_t format,risc_display_surface_v1*out){
 (void)c;io();assert(!frames&&format==RISC_DISPLAY_FORMAT_RGB565);frames=1;
 *out=(risc_display_surface_v1){.frame=1,.pixels=noquick_pixels,.width=410,.height=502,
  .stride_bytes=820,.size_bytes=sizeof(noquick_pixels),.pixel_format=format};return true;
}
static bool noquick_set(void*c,uint16_t v,uint16_t m){(void)c;(void)v;(void)m;io();++noquick_brightness_calls;return true;}
static risc_display_output_api_v1 noquick_display;
static bool noquick_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 bool ok=fx_acquire(n,v,i,g);if(ok&&g->api==&fx_display)g->api=&noquick_display;return ok;
}
int main(void){
 noquick_display=fx_display;noquick_display.get_info=noquick_info;noquick_display.acquire=noquick_frame;noquick_display.set_brightness=noquick_set;
 fx_runtime.acquire=noquick_acquire;
 for(unsigned i=0;i<3;i++){
  assert(!app_module_init());assert(portable_paper_frame_ready());clear_color(0x2468);present(false);
  assert(portable_paper_frame_drain()&&!failed&&!retained);app_module_fini();assert(!live&&!subscriptions&&!frames&&!noquick_brightness_calls);
 }
 puts("No-Quick adapter keeps brightness policy, provider grants and UI unchanged PASS");return 0;
}
