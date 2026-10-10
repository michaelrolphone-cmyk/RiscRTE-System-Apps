/* A deferred first raster advances the existing elapsed handoff timeline. */
#define RASTER_LATENCY_NO_MAIN
#include "portable_raster_latency_test.c"
static void origin_case(unsigned delay,bool wrap,bool offscreen_failure) {
 begin_test();mock_format=RISC_DISPLAY_FORMAT_RGB565;
 mock_ms=wrap?UINT32_MAX-50u:100u;
 assert(!app_module_init());assert(springboard_presentation_get());
 const t5_app_api_v1 *api=t5_app_get_api(1);assert(api->frame_ready());
 unsigned origin=mock_ms;api->clear();api->fill_rect(0,0,width(),height(),1);api->present(false);
 assert(raster_sealed&&handoff_pending&&!handoff_active&&!mock_frames);
 mock_ms+=delay;
 if(offscreen_failure)mock_fail_allocation=mock_allocations+1;
 while(raster_sealed)assert(raster_progress());
 assert(handoff_started==origin);
 unsigned alpha=portable_transition_alpha(delay);
 assert(handoff_active==(alpha<256));
 uint16_t expected[240*240],scratch[240*240];
 for(unsigned i=0;i<240u*240u;i++)expected[i]=0;
 assert(portable_transition_rgb565(expected,480,mock_old,480,scratch,sizeof(scratch),240,240,alpha));
 for(unsigned y=0;y<240;y++)assert(!memcmp(expected+y*240,mock_pixels+y*243,480));
 assert(portable_paper_frame_drain());end_test();assert(!mock_bytes);
 printf("raster handoff origin: delay=%u wrap=%u fallback=%u alpha=%u PASS\n",delay,wrap,offscreen_failure,alpha);
}
int main(void) {
 const unsigned delays[]={0,17,120,700};
 for(unsigned wrap=0;wrap<2;wrap++)for(unsigned i=0;i<4;i++)for(unsigned fallback=0;fallback<2;fallback++)
  origin_case(delays[i],wrap,fallback);
 return 0;
}
