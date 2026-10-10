/* Deterministic CPU work cost in the actual portable raster. This preserves
 * provider edge queues and measures capture separately from model dispatch. */
#define FRAME_DELIVERY_NO_MAIN
#include "portable_frame_delivery_test.c"
#include <time.h>
static unsigned cpu_ms_per_batch,charged_pixels,captured_at;
static void raster_test_clock(void) {
 unsigned batches=(raster_pixels-charged_pixels)/512u;
 if(cpu_ms_per_batch && batches){mock_ms+=batches*cpu_ms_per_batch;charged_pixels+=batches*512u;}
}
static bool raster_probe_poll(void *c,size_t budget) {
 mock_poll_touch(c,budget);
 for(;probe_scheduled<2;probe_scheduled++) {
  unsigned at=20+probe_scheduled*5;
  if(mock_ms<at)break;
  if(!captured_at)captured_at=mock_ms;
  probe_push(probe_scheduled?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN,at);
 }
 return true;
}
static bool raster_nav_poll(void*c,risc_input_navigation_frame_v1*out) {
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(nav_edges<2 && mock_ms>=20+nav_edges*5) {
  if(nav_edges++)out->released=RISC_NAV_RIGHT;
  else out->pressed=out->buttons=RISC_NAV_RIGHT;
 }
 return true;
}
static void raster_case(unsigned format,unsigned cost) {
 begin_test();mock_format=format;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
  (format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0);
 probe_head=probe_count=probe_scheduled=probe_begins=probe_taps=probe_model=0;
 nav_edges=0;captured_at=0;cpu_ms_per_batch=0;charged_pixels=0;
 probe_snapshot=(risc_touch_snapshot_v1){.width=format==RISC_DISPLAY_FORMAT_MONO1?480:240,.height=format==RISC_DISPLAY_FORMAT_MONO1?800:240};
 assert(!app_module_init());
 risc_touch_api_v1 input=mock_touch;input.poll=raster_probe_poll;input.next=probe_next;input.snapshot=probe_snap;touch.api=&input;
 risc_input_navigation_api_v1 nav=probe_navigation;nav.poll=raster_nav_poll;navigation=&nav;
 if(format==RISC_DISPLAY_FORMAT_MONO1)assert(paper_presentation_get());else assert(springboard_presentation_get());handoff_finish();
 const t5_app_api_v1 *api=t5_app_get_api(1);assert(api->frame_ready());
 t5_app_input_t neutral;assert(api->poll(&neutral,0));
 mock_ms=0;raster_pixels=charged_pixels=0;cpu_ms_per_batch=cost;
 if(!cost&&getenv("RASTER_FAIL_ALLOCATION"))mock_fail_allocation=mock_allocations+(unsigned)atoi(getenv("RASTER_FAIL_ALLOCATION"));
 clock_t start=clock();api->clear();
 for(unsigned n=0;n<3;n++)api->fill_rect(0,0,width(),height(),n&1u);
 char borrowed[80]="COMPLETE API RASTER SNAPSHOT";
 api->draw_text(20,80,borrowed);memset(borrowed,'X',20);
 api->fill_rounded_rect_tone(12,30,180,48,7,2);
 api->draw_label(20,130,180,"LATEST MODEL");
 assert(api->draw_icon(30,170,"solid:f013",36,true));
 assert(!api->draw_icon(30,170,"does-not-exist",36,true));
 unsigned drawn_at=mock_ms;double host_draw_ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;
 api->present(true);unsigned first_dispatch=0,nav_dispatch=0;
 for(unsigned i=0;i<100 && (probe_taps<1||nav_edges<2);i++) {
  t5_app_input_t e;assert(api->poll(&e,4));
  if(e.buttons&T5_APP_BUTTON_RIGHT)nav_dispatch=mock_ms;
  springboard_contact c;np_contact(&c);
  if(c.began){probe_begins++;first_dispatch=mock_ms;}
  if(c.valid&&c.released&&c.tap_eligible&&!c.cancelled){probe_taps++;probe_model++;}
 }
 assert(probe_begins==1&&probe_taps==1&&probe_model==1&&nav_edges==2);
 assert(captured_at<=20+cost*2+4);
#ifdef PORTABLE_RASTER_SNAPSHOT
 if(cost)assert(first_dispatch-captured_at<=8);
#else
 if(cost)assert(first_dispatch>=drawn_at&&first_dispatch-captured_at>100);
#endif
 unsigned dispatched_pixels=raster_pixels;assert(api->frame_drain());
 uint32_t hash=2166136261u;unsigned bytes=format==RISC_DISPLAY_FORMAT_MONO1?60:480;
 char destination[512];snprintf(destination,sizeof(destination),"%s/raster-%u-%u.bin",getenv("RASTER_OUTPUT"),format,cost);
 FILE*f=fopen(destination,"wb");assert(f);
 for(unsigned y=0;y<info.height;y++)assert(fwrite((uint8_t*)mock_pixels+y*surface.stride_bytes,1,bytes,f)==bytes);
 assert(!fclose(f));
 for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<bytes;x++){hash^=((uint8_t*)mock_pixels)[y*surface.stride_bytes+x];hash*=16777619u;}
 printf("{\"format\":%u,\"cost_ms_per_512_pixel_visits\":%u,\"draw_return_ms\":%u,\"touch_capture_ms\":%u,\"model_dispatch_ms\":%u,\"nav_dispatch_ms\":%u,\"host_cpu_draw_ms\":%.3f,\"pixel_visits_before_model\":%u,\"pixel_visits_total\":%u,\"completed_at_ms\":%u,\"raster_hash\":%u}\n",format,cost,drawn_at,captured_at,first_dispatch,nav_dispatch,host_draw_ms,dispatched_pixels,raster_pixels,mock_ms,hash);
 cpu_ms_per_batch=0;assert(api->frame_drain());end_test();
}
int main(void){for(unsigned f=0;f<2;f++)for(unsigned cost=0;cost<3;cost++)raster_case(f?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565,cost);return 0;}
