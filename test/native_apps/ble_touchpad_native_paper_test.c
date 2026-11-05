/* Compare actual one-bit adapter pixels/input with the selector on and off. */
#include <stdint.h>
static unsigned geometry_width=400,geometry_height=600,geometry_stride=53,geometry_size=31800;
static unsigned test_format=1,paper_snapshot_reads,paper_event_reads;
#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main
#ifdef PORTABLE_BLE_TOUCHPAD_NATIVE_UI
#include "PortableBleTouchpadNative.h"
#endif
static void ready_frame(void) {
 assert(portable_paper_frame_ready());pp_begin();pp_text(17,25,350,"TOUCHPAD",1,true,true);
 pp_circle(200,300,40,true);fill(50,450,300,20,0);
#ifdef PORTABLE_BLE_TOUCHPAD_NATIVE_UI
 portable_ble_touchpad_native_present(false);
#else
 present(false);
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
 for(unsigned n=0;raster_sealed&&n<100;n++)assert(raster_progress());
 assert(!raster_sealed);
#endif
 mock_ms+=50;assert(paper_present_progress());assert(!paper_token);
}
int main(void) {
 test_format=RISC_DISPLAY_FORMAT_MONO1;
 mock_pixels=malloc(geometry_size);assert(mock_pixels);memset(mock_pixels,0xa5,geometry_size);
 mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_RETAINS_IMAGE;mock_latency=40;
 assert(app_module_init()==0&&surface_format==RISC_DISPLAY_FORMAT_MONO1&&pp_enabled());
 portable_touch prior=touch;
#ifdef PORTABLE_BLE_TOUCHPAD_NATIVE_UI
 portable_ble_touchpad_native_begin();portable_ble_touchpad_native_layout(1);
 assert(!portable_ble_touchpad_native_ready(NULL,NULL));
#endif
 assert(!memcmp(&prior,&touch,sizeof(touch)));ready_frame();
 t5_app_input_t out;assert(poll_input(&out,0));assert(!out.tapped);
 mock_down=true;assert(poll_input(&out,0));assert(!out.tapped);
#ifdef PORTABLE_BLE_TOUCHPAD_NATIVE_UI
 portable_ble_touchpad_native_layout(2);
#endif
 ready_frame();mock_down=false;assert(poll_input(&out,0));assert(out.tapped&&out.touch_x==120&&out.touch_y==120);
 uint64_t hash=1469598103934665603ull;
 for(unsigned y=0;y<geometry_height;y++){
  const uint8_t *row=(const uint8_t*)mock_pixels+y*geometry_stride;
  for(unsigned x=0;x<50;x++){hash^=row[x];hash*=1099511628211ull;}
  for(unsigned x=50;x<geometry_stride;x++)assert(row[x]==0xa5);
 }
 app_module_fini();assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live);free(mock_pixels);
 printf("PAPER %016llx snapshots=%u events=%u taps=1\n",(unsigned long long)hash,paper_snapshot_reads,paper_event_reads);
}
