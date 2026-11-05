/* Execute the production adapter: tokens, snapshot replay, queue and cleanup. */
#define main inherited_handoff_main
#include "portable_handoff_test.c"
#undef main
#include "PortableBleTouchpadNative.h"

static void start(bool asynchronous) {
 begin_test();mock_flags=asynchronous?RISC_DISPLAY_INFO_ASYNC_PRESENT:0;
 assert(app_module_init()==0);handoff_finish();mock_latency=asynchronous?100:0;
 portable_ble_touchpad_native_begin();portable_ble_touchpad_native_layout(1);
}
static void seal(unsigned owner) {
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x07e0);
 if(owner==7)portable_ble_touchpad_native_present(false);else portable_native_present(owner,false);
}
static void submit(void) {
#ifdef PORTABLE_RASTER_SNAPSHOT
 for(unsigned n=0;raster_sealed&&n<20;n++)assert(raster_progress());
 assert(!raster_sealed);
#endif
}
static void complete_frame(void) {
 submit();mock_ms+=mock_latency;assert(paper_present_progress());assert(!paper_token);
}
static t5_app_input_t step(void) {t5_app_input_t out;assert(poll_input(&out,0));return out;}
static void neutral(void) {for(unsigned n=0;n<3;n++){t5_app_input_t out=step();assert(!out.tapped&&!out.buttons);}}
static void fresh_tap(void) {
 mock_down=true;t5_app_input_t out=step();assert(!out.tapped&&ble_touchpad_native_owned());
 mock_down=false;out=step();assert(out.tapped&&!out.buttons&&out.touch_x==120&&out.touch_y==120);
}
static void visible(void) {
 seal(7);assert(!portable_ble_touchpad_native_ready(NULL,NULL));submit();
 assert(native_token_owner==7&&native_present_token==paper_token&&paper_token);
 complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();
}
static void repaint_and_queue(void) {
 start(true);visible();uint64_t generation,watermark;
 assert(portable_ble_touchpad_native_ready(&generation,&watermark));
 seal(7);assert(portable_ble_touchpad_native_ready(NULL,NULL));submit();
 assert(paper_token&&native_token_owner==7);fresh_tap();assert(paper_token);
 /* The provider can have complete queued contacts before display completion. */
 mock_down=true;assert(mock_poll_touch(NULL,1));mock_down=false;assert(mock_poll_touch(NULL,1));
 assert(mock_event_count==2);complete_frame();assert(mock_event_count==2);
 uint64_t g,w;assert(portable_ble_touchpad_native_ready(&g,&w)&&g==generation&&w==watermark);
 t5_app_input_t out=step();assert(!out.tapped);out=step();assert(out.tapped);
 /* Held ownership survives completion and the release remains actionable. */
 mock_down=true;out=step();assert(ble_touchpad_native_owned());seal(7);complete_frame();
 assert(ble_touchpad_native_owned());mock_down=false;out=step();assert(out.tapped);
 end_test();
}
static void layout_reset_and_stale(void) {
 start(true);visible();seal(7);submit();
 uint64_t old_token=paper_token,old_generation=ble_touchpad_native_token_generation;
 portable_ble_touchpad_native_layout(2);complete_frame();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 seal(7);submit();uint64_t token=paper_token;
 portable_native_presented(old_token);assert(native_present_token==token&&native_token_owner==7);
 assert(!portable_ble_touchpad_native_ready(NULL,NULL));complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();
 seal(7);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(raster_sealed&&raster_native_present_owner==7);
 uint64_t sealed_generation=raster_ble_touchpad_native_generation;
#endif
 portable_ble_touchpad_native_begin();portable_ble_touchpad_native_layout(3);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(raster_sealed&&raster_ble_touchpad_native_generation==sealed_generation);
#endif
 submit();assert(paper_token&&ble_touchpad_native_token_generation!=ble_touchpad_native_generation);
 assert(ble_touchpad_native_token_generation>old_generation);complete_frame();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 seal(7);submit();token=paper_token;portable_ble_touchpad_native_begin();portable_ble_touchpad_native_layout(4);
 assert(paper_token==token&&native_present_token==token);complete_frame();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 seal(7);complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();fresh_tap();
 end_test();
}
static void held_and_foreign(void) {
 start(true);mock_down=true;assert(mock_poll_touch(NULL,1));seal(7);complete_frame();neutral();
 mock_down=false;t5_app_input_t out=step();assert(!out.tapped);neutral();fresh_tap();
 seal(0);assert(!portable_ble_touchpad_native_ready(NULL,NULL));complete_frame();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 seal(7);complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();
 seal(5);complete_frame();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 seal(7);complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();fresh_tap();end_test();
}
static void synchronous(void) {
 start(false);seal(7);submit();assert(!paper_token&&!native_present_token);
 assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();fresh_tap();end_test();
}

#ifdef PORTABLE_RASTER_SNAPSHOT
static void pipelined_snapshots(void) {
 start(true);visible();seal(7);submit();uint64_t first=paper_token;
 /* Prepare a different layout while the previous immutable token is owned. */
 portable_ble_touchpad_native_layout(2);seal(7);assert(raster_sealed&&paper_token==first);
 uint64_t generation=raster_ble_touchpad_native_generation;
 assert(raster_progress()&&paper_token==first&&!portable_ble_touchpad_native_ready(NULL,NULL));
 mock_ms+=mock_latency;
 for(unsigned n=0;raster_sealed&&n<20;n++)assert(raster_progress());
 assert(!raster_sealed&&paper_token!=first&&native_token_owner==7);
 assert(ble_touchpad_native_token_generation==generation&&!portable_ble_touchpad_native_ready(NULL,NULL));
 complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();fresh_tap();end_test();
 /* A foreign sealed image must fence even a first frame not yet visible. */
 start(true);seal(7);submit();first=paper_token;seal(0);assert(raster_sealed&&paper_token==first);
 mock_ms+=mock_latency;assert(paper_token_progress());assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 complete_frame();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 seal(7);complete_frame();assert(portable_ble_touchpad_native_ready(NULL,NULL));neutral();fresh_tap();end_test();
}
#endif

static void failed_status(void) {
 start(true);visible();seal(7);submit();mock_fail_status=true;
 assert(!paper_present_progress()&&failed&&!portable_ble_touchpad_native_ready(NULL,NULL));
 uint64_t token=paper_token;unsigned grants=mock_grants,allocations=mock_live;
 app_module_fini();assert(paper_token==token&&native_present_token==token);
 assert(mock_grants==grants&&mock_subscriptions==1&&mock_live==allocations);
}
int main(void) {
 repaint_and_queue();layout_reset_and_stale();held_and_foreign();synchronous();
#ifdef PORTABLE_RASTER_SNAPSHOT
 pipelined_snapshots();
#endif
 failed_status();
 puts("Touchpad production adapter: frame-token owner7, pending/queued/held input, stale tokens, immutable snapshot/reset identity, foreign owners, synchronous completion, failure and cleanup PASS");
}
