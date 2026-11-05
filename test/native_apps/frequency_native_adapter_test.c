/* Execute the production adapter: tokens, snapshot replay, queue and cleanup. */
#define main inherited_handoff_main
#ifdef FREQUENCY_HANDOFF_FIXTURE
#include FREQUENCY_HANDOFF_FIXTURE
#else
#include "portable_handoff_test.c"
#endif
#undef main
#include "PortableFrequencyNative.h"

static void start(bool asynchronous) {
 begin_test();mock_flags=asynchronous?RISC_DISPLAY_INFO_ASYNC_PRESENT:0;
 assert(app_module_init()==0);handoff_finish();mock_latency=asynchronous?100:0;
 portable_frequency_native_begin();portable_frequency_native_layout(1);
}
static void seal(unsigned owner) {
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x07e0);
 if(owner==10)portable_frequency_native_present(false);else portable_native_present(owner,false);
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
 mock_down=true;t5_app_input_t out=step();assert(!out.tapped&&frequency_native_owned());
 mock_down=false;out=step();assert(out.tapped&&!out.buttons&&out.touch_x==120&&out.touch_y==120);
}
static void visible(void) {
 seal(10);assert(!portable_frequency_native_ready(NULL,NULL)&&!portable_frequency_native_repaint());submit();
 assert(native_token_owner==10&&native_present_token==paper_token&&paper_token);
 complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));
}
static void repaint_and_queue(void) {
 start(true);visible();uint64_t generation,watermark;
 assert(portable_frequency_native_ready(&generation,&watermark));
 seal(10);assert(portable_frequency_native_ready(NULL,NULL));submit();
 assert(paper_token&&native_token_owner==10);fresh_tap();assert(paper_token);
 /* The provider can have complete queued contacts before display completion. */
 mock_down=true;assert(mock_poll_touch(NULL,1));mock_down=false;assert(mock_poll_touch(NULL,1));
 assert(mock_event_count==2);complete_frame();assert(mock_event_count==2);
 uint64_t g,w;assert(portable_frequency_native_ready(&g,&w)&&g==generation&&w==watermark);
 t5_app_input_t out=step();assert(!out.tapped);out=step();assert(out.tapped);
 /* Held ownership survives completion and the release remains actionable. */
 mock_down=true;out=step();assert(frequency_native_owned());seal(10);complete_frame();
 assert(frequency_native_owned());mock_down=false;out=step();assert(out.tapped);
 end_test();
}
static void layout_reset_and_stale(void) {
 start(true);visible();seal(10);submit();
 uint64_t old_token=paper_token,old_generation=frequency_native_token_generation;
 portable_frequency_native_layout(2);complete_frame();assert(!portable_frequency_native_ready(NULL,NULL));
 seal(10);submit();uint64_t token=paper_token;
 portable_native_presented(old_token);assert(native_present_token==token&&native_token_owner==10);
 assert(!portable_frequency_native_ready(NULL,NULL));complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));
 seal(10);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(raster_sealed&&raster_native_present_owner==10);
 uint64_t sealed_generation=raster_frequency_native_generation;
#endif
 portable_frequency_native_begin();portable_frequency_native_layout(3);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(raster_sealed&&raster_frequency_native_generation==sealed_generation);
#endif
 submit();assert(paper_token&&frequency_native_token_generation!=frequency_native_generation);
 assert(frequency_native_token_generation>old_generation);complete_frame();assert(!portable_frequency_native_ready(NULL,NULL));
 seal(10);submit();token=paper_token;portable_frequency_native_begin();portable_frequency_native_layout(4);
 assert(paper_token==token&&native_present_token==token);complete_frame();assert(!portable_frequency_native_ready(NULL,NULL));
 seal(10);complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));fresh_tap();
 end_test();
}
static void held_and_foreign(void) {
 start(true);mock_down=true;assert(mock_poll_touch(NULL,1));seal(10);complete_frame();neutral();
 mock_down=false;t5_app_input_t out=step();assert(!out.tapped);neutral();fresh_tap();
 seal(0);assert(!portable_frequency_native_ready(NULL,NULL)&&!portable_frequency_native_repaint());
 complete_frame();assert(!portable_frequency_native_ready(NULL,NULL)&&portable_frequency_native_repaint());
 seal(10);assert(!portable_frequency_native_repaint());complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));
 seal(8);complete_frame();assert(!portable_frequency_native_ready(NULL,NULL));
 seal(9);complete_frame();assert(!portable_frequency_native_ready(NULL,NULL));
 seal(10);complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));fresh_tap();end_test();
}
static void synchronous(void) {
 start(false);seal(10);submit();assert(!paper_token&&!native_present_token);
 neutral();assert(portable_frequency_native_ready(NULL,NULL));fresh_tap();end_test();
}
static void foreign_preserves_raw(void) {
 start(true);visible();mock_down=true;(void)step();assert(touch.down&&frequency_native_owned());
 seal(0);assert(touch.down&&!frequency_native_owned()&&!portable_frequency_native_repaint());
 complete_frame();assert(touch.down&&portable_frequency_native_repaint());
 seal(10);complete_frame();mock_down=false;t5_app_input_t out=step();assert(!out.tapped);
 neutral();fresh_tap();end_test();
}

#ifdef PORTABLE_RASTER_SNAPSHOT
static void pipelined_snapshots(void) {
 start(true);visible();seal(10);submit();uint64_t first=paper_token;
 /* Prepare a different layout while the previous immutable token is owned. */
 portable_frequency_native_layout(2);seal(10);assert(raster_sealed&&paper_token==first);
 uint64_t generation=raster_frequency_native_generation;
 assert(raster_progress()&&paper_token==first&&!portable_frequency_native_ready(NULL,NULL));
 mock_ms+=mock_latency;
 for(unsigned n=0;raster_sealed&&n<20;n++)assert(raster_progress());
 assert(!raster_sealed&&paper_token!=first&&native_token_owner==10);
 assert(frequency_native_token_generation==generation&&!portable_frequency_native_ready(NULL,NULL));
 complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));fresh_tap();end_test();
 /* A foreign sealed image must fence even a first frame not yet visible. */
 start(true);seal(10);submit();first=paper_token;seal(0);assert(raster_sealed&&paper_token==first);
 mock_ms+=mock_latency;assert(paper_token_progress());assert(!portable_frequency_native_ready(NULL,NULL));
 complete_frame();assert(!portable_frequency_native_ready(NULL,NULL));
 seal(10);complete_frame();neutral();assert(portable_frequency_native_ready(NULL,NULL));fresh_tap();end_test();
}
#endif

static void back_request(void) {
 begin_test();assert(app_module_init()==0);handoff_finish();
 assert(portable_frequency_native_available());
 portable_frequency_native_back();assert(!navigation_pending&&!mock_launches);
 portable_frequency_native_begin();portable_frequency_native_layout(1);
 portable_frequency_native_back();
 assert(navigation_pending==T5_APP_BUTTON_BACK&&!mock_launches);
 t5_app_input_t out;assert(poll(&out,0));
 assert(mock_launches==1&&out.exit_requested&&handoff_requested&&!navigation_pending);
 end_test();
}

static void repaint_fences(void) {
 start(true);neutral();assert(portable_frequency_native_repaint());
 unsigned polls=mock_polls,presents=mock_presents;
 handoff_requested=true;assert(!portable_frequency_native_repaint());handoff_requested=false;
 home_pending=true;assert(!portable_frequency_native_repaint());home_pending=false;
 crown_pending=true;assert(!portable_frequency_native_repaint());crown_pending=false;
#ifdef PORTABLE_QUICK_ACTIONS
 quick_modal=true;assert(!portable_frequency_native_repaint());quick_modal=false;
 quick_restore_pending=true;assert(!portable_frequency_native_repaint());quick_restore_pending=false;
 quick_restoring=true;assert(!portable_frequency_native_repaint());quick_restoring=false;
 quick.ui.target_q8=256;assert(!portable_frequency_native_repaint());quick.ui.target_q8=0;
 quick.ui.neutral_gate=true;assert(!portable_frequency_native_repaint());quick.ui.neutral_gate=false;
#endif
 assert(portable_frequency_native_repaint()&&polls==mock_polls&&presents==mock_presents);
 visible();assert(!portable_frequency_native_repaint());end_test();
}
#ifdef PORTABLE_QUICK_ACTIONS
static void quick_replay(void) {
 start(true);visible();mock_x=mock_y=14;
 mock_down=true;t5_app_input_t out=step();assert(!out.tapped&&!app_contact.down);
 mock_down=false;out=step();assert(!out.tapped&&app_contact.down);
 assert(app_contact.x==14&&app_contact.y==14&&quick_replay_delivery);
 out=step();assert(out.tapped&&!app_contact.down&&!quick_replay_delivery);
 assert(out.touch_x==14&&out.touch_y==14);
 mock_x=mock_y=120;end_test();
}
static void quick_replay_fences(void) {
 /* Reserve DOWN while the first native frame is pending. Visibility arrives
  * before UP, but the old edge must never become native contact or a tap. */
 start(true);neutral();mock_x=mock_y=14;seal(10);submit();
 mock_down=true;t5_app_input_t out=step();
 assert(!out.tapped&&!app_contact.down&&quick.ui.gesture==PQA_TOP_PENDING);
 assert(!frequency_native_quick_eligible&&!frequency_native_visible);
 uint64_t down_sequence=frequency_native_quick_sequence;
 complete_frame();mock_down=false;
 for(unsigned n=0;n<4;n++){out=step();assert(!out.tapped&&!app_contact.down);}
 assert(touch.sequence>down_sequence&&!frequency_native_owned());
 mock_x=mock_y=120;fresh_tap();end_test();
 /* Layout replacement cancels both a reserved edge and an already split
  * replay whose release is queued in the adapter's existing replay slot. */
 for(unsigned split=0;split<2;split++) {
  start(true);visible();mock_x=mock_y=14;mock_down=true;out=step();
  assert(!out.tapped&&!app_contact.down&&frequency_native_quick_eligible);
  if(split){mock_down=false;out=step();assert(app_contact.down&&!out.tapped&&quick_replay_delivery);}
  portable_frequency_native_layout(2);assert(!frequency_native_quick_eligible);
  seal(10);complete_frame();mock_down=false;
  for(unsigned n=0;n<4;n++){out=step();assert(!out.tapped&&!app_contact.down);}
  mock_x=mock_y=120;fresh_tap();end_test();
 }
 /* Quick takes over a held native contact, then closes while it is still
  * held. Its own stream survives; the eventual UP cannot activate the app. */
 start(true);visible();mock_down=true;out=step();assert(frequency_native_owned());
 quick_modal=true;quick.ui.gesture=PQA_PANEL_DRAG;quick.ui.target_q8=256;
 seal(0);complete_frame();assert(touch.down&&!frequency_native_owned());
 quick_modal=false;quick.ui.position_q8=quick.ui.target_q8=0;pqa_cancel_input(&quick.ui);
 assert(!portable_frequency_native_repaint());mock_down=false;
 for(unsigned n=0;n<4;n++){out=step();assert(!out.tapped&&!app_contact.down);}
 assert(portable_frequency_native_repaint());seal(10);complete_frame();neutral();fresh_tap();end_test();
}
#endif

static void failed_status(void) {
 start(true);visible();seal(10);submit();mock_fail_status=true;
 assert(!paper_present_progress()&&failed&&!portable_frequency_native_ready(NULL,NULL));
 uint64_t token=paper_token;unsigned grants=mock_grants,allocations=mock_live;
 app_module_fini();assert(paper_token==token&&native_present_token==token);
 assert(mock_grants==grants&&mock_subscriptions==1&&mock_live==allocations);
}
int main(void) {
 repaint_and_queue();layout_reset_and_stale();held_and_foreign();foreign_preserves_raw();synchronous();
#ifdef PORTABLE_RASTER_SNAPSHOT
 pipelined_snapshots();
#endif
 repaint_fences();back_request();
#ifdef PORTABLE_QUICK_ACTIONS
 quick_replay();quick_replay_fences();
#endif
 failed_status();
 puts("Frequency production adapter: frame-token owner10, pending/queued/held input, stale tokens, immutable snapshot/reset identity, foreign owners0/8/9 with raw custody, synchronous completion, repaint fences, Quick top-edge replay, queued adapter Back, failure and cleanup PASS");
}
