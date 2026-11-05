/* Execute the production adapter: tokens, snapshot replay, queue and cleanup. */
#define main inherited_handoff_main
#ifdef TIMECARD_HANDOFF_FIXTURE
#include TIMECARD_HANDOFF_FIXTURE
#else
#include "portable_handoff_test.c"
#endif
#undef main
#include "PortableTimecardNative.h"

static void start(bool asynchronous) {
 begin_test();mock_flags=asynchronous?RISC_DISPLAY_INFO_ASYNC_PRESENT:0;
 assert(app_module_init()==0);handoff_finish();mock_latency=asynchronous?100:0;
 portable_timecard_native_begin();portable_timecard_native_layout(1);
}
static void seal(unsigned owner) {
 assert(portable_paper_frame_ready());clear();fill(0,0,240,240,0x07e0);
 if(owner==14)portable_timecard_native_present(false);else portable_native_present(owner,false);
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
 mock_down=true;t5_app_input_t out=step();assert(!out.tapped&&timecard_native_owned());
 mock_down=false;out=step();assert(out.tapped&&!out.buttons&&out.touch_x==120&&out.touch_y==120);
}
static void visible(void) {
 seal(14);assert(!portable_timecard_native_ready(NULL,NULL)&&!portable_timecard_native_repaint());submit();
 assert(native_token_owner==14&&native_present_token==paper_token&&paper_token);
 complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));
}
static void repaint_and_queue(void) {
 start(true);visible();uint64_t generation,watermark;
 assert(portable_timecard_native_ready(&generation,&watermark));
 seal(14);assert(portable_timecard_native_ready(NULL,NULL));submit();
 assert(paper_token&&native_token_owner==14);fresh_tap();assert(paper_token);
 /* The provider can have complete queued contacts before display completion. */
 mock_down=true;assert(mock_poll_touch(NULL,1));mock_down=false;assert(mock_poll_touch(NULL,1));
 assert(mock_event_count==2);complete_frame();assert(mock_event_count==2);
 uint64_t g,w;assert(portable_timecard_native_ready(&g,&w)&&g==generation&&w==watermark);
 t5_app_input_t out=step();assert(!out.tapped);out=step();assert(out.tapped);
 /* Held ownership survives completion and the release remains actionable. */
 mock_down=true;out=step();assert(timecard_native_owned());seal(14);complete_frame();
 assert(timecard_native_owned());mock_down=false;out=step();assert(out.tapped);
 end_test();
}
static void layout_reset_and_stale(void) {
 start(true);visible();seal(14);submit();
 uint64_t old_token=paper_token,old_generation=timecard_native_token_generation;
 portable_timecard_native_layout(2);complete_frame();assert(!portable_timecard_native_ready(NULL,NULL));
 seal(14);submit();uint64_t token=paper_token;
 portable_native_presented(old_token);assert(native_present_token==token&&native_token_owner==14);
 assert(!portable_timecard_native_ready(NULL,NULL));complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));
 seal(14);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(raster_sealed&&raster_native_present_owner==14);
 uint64_t sealed_generation=raster_timecard_native_generation;
#endif
 portable_timecard_native_begin();portable_timecard_native_layout(3);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(raster_sealed&&raster_timecard_native_generation==sealed_generation);
#endif
 submit();assert(paper_token&&timecard_native_token_generation!=timecard_native_generation);
 assert(timecard_native_token_generation>old_generation);complete_frame();assert(!portable_timecard_native_ready(NULL,NULL));
 seal(14);submit();token=paper_token;portable_timecard_native_begin();portable_timecard_native_layout(4);
 assert(paper_token==token&&native_present_token==token);complete_frame();assert(!portable_timecard_native_ready(NULL,NULL));
 seal(14);complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();
 end_test();
}
static void held_and_foreign(void) {
 start(true);mock_down=true;assert(mock_poll_touch(NULL,1));seal(14);complete_frame();neutral();
 mock_down=false;t5_app_input_t out=step();assert(!out.tapped);neutral();fresh_tap();
 seal(0);assert(!portable_timecard_native_ready(NULL,NULL)&&!portable_timecard_native_repaint());
 complete_frame();assert(!portable_timecard_native_ready(NULL,NULL)&&portable_timecard_native_repaint());
 seal(14);assert(!portable_timecard_native_repaint());complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));
 for(unsigned owner=0;owner<14;owner++) {
  uint64_t generation=timecard_native_generation;
  seal(owner);complete_frame();
  assert(timecard_native_generation>generation&&!portable_timecard_native_ready(NULL,NULL));
 }
 seal(14);complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();end_test();
}
static void synchronous(void) {
 start(false);seal(14);submit();assert(!paper_token&&!native_present_token);
 neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();end_test();
}
static void foreign_preserves_raw(void) {
 start(true);visible();mock_down=true;(void)step();assert(touch.down&&timecard_native_owned());
 seal(0);assert(touch.down&&!timecard_native_owned()&&!portable_timecard_native_repaint());
 complete_frame();assert(touch.down&&portable_timecard_native_repaint());
 seal(14);complete_frame();mock_down=false;t5_app_input_t out=step();assert(!out.tapped);
 neutral();fresh_tap();end_test();
}

#ifdef PORTABLE_RASTER_SNAPSHOT
static void pipelined_snapshots(void) {
 start(true);visible();seal(14);submit();uint64_t first=paper_token;
 /* Prepare a different layout while the previous immutable token is owned. */
 portable_timecard_native_layout(2);seal(14);assert(raster_sealed&&paper_token==first);
 uint64_t generation=raster_timecard_native_generation;
 assert(raster_progress()&&paper_token==first&&!portable_timecard_native_ready(NULL,NULL));
 mock_ms+=mock_latency;
 for(unsigned n=0;raster_sealed&&n<20;n++)assert(raster_progress());
 assert(!raster_sealed&&paper_token!=first&&native_token_owner==14);
 assert(timecard_native_token_generation==generation&&!portable_timecard_native_ready(NULL,NULL));
 complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();end_test();
 /* A foreign sealed image must fence even a first frame not yet visible. */
 start(true);seal(14);submit();first=paper_token;seal(0);assert(raster_sealed&&paper_token==first);
 mock_ms+=mock_latency;assert(paper_token_progress());assert(!portable_timecard_native_ready(NULL,NULL));
 complete_frame();assert(!portable_timecard_native_ready(NULL,NULL));
 seal(14);complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();end_test();
}
#endif

static void back_request(void) {
 begin_test();assert(app_module_init()==0);handoff_finish();
 assert(portable_timecard_native_available());
 portable_timecard_native_back();assert(!navigation_pending&&!mock_launches);
 portable_timecard_native_begin();portable_timecard_native_layout(1);
 portable_timecard_native_back();
 assert(navigation_pending==T5_APP_BUTTON_BACK&&!mock_launches);
 t5_app_input_t out;assert(poll(&out,0));
 assert(mock_launches==1&&out.exit_requested&&handoff_requested&&!navigation_pending);
 end_test();
}

static void module_reentry(void) {
 start(true);visible();mock_down=true;(void)step();assert(timecard_native_owned());
 uint64_t prior_generation=timecard_native_generation;
 end_test();begin_test();mock_down=true;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT;
 assert(app_module_init()==0);handoff_finish();mock_latency=100;
 /* The selector alone cannot revive the old native map on a new invocation. */
 assert(!timecard_native_enabled&&!portable_timecard_native_ready(NULL,NULL));
 assert(!portable_timecard_native_repaint());
 portable_timecard_native_back();assert(!navigation_pending&&!mock_launches);
 portable_timecard_native_begin();portable_timecard_native_layout(1);
 assert(timecard_native_generation>prior_generation&&!timecard_native_owned());
 seal(14);complete_frame();neutral();
 mock_down=false;t5_app_input_t out=step();assert(!out.tapped&&!out.buttons);
 neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();end_test();
}

static void repaint_fences(void) {
 start(true);neutral();assert(portable_timecard_native_repaint());
 unsigned polls=mock_polls,presents=mock_presents;
 handoff_requested=true;assert(!portable_timecard_native_repaint());handoff_requested=false;
 home_pending=true;assert(!portable_timecard_native_repaint());home_pending=false;
 crown_pending=true;assert(!portable_timecard_native_repaint());crown_pending=false;
#ifdef PORTABLE_QUICK_ACTIONS
 quick_modal=true;assert(!portable_timecard_native_repaint());quick_modal=false;
 quick_restore_pending=true;assert(!portable_timecard_native_repaint());quick_restore_pending=false;
 quick_restoring=true;assert(!portable_timecard_native_repaint());quick_restoring=false;
 quick.ui.target_q8=256;assert(!portable_timecard_native_repaint());quick.ui.target_q8=0;
 quick.ui.neutral_gate=true;assert(!portable_timecard_native_repaint());quick.ui.neutral_gate=false;
#endif
 assert(portable_timecard_native_repaint()&&polls==mock_polls&&presents==mock_presents);
 visible();assert(!portable_timecard_native_repaint());end_test();
}
#ifdef PORTABLE_QUICK_ACTIONS
#ifdef PORTABLE_RASTER_SNAPSHOT
static void quick_snapshot_priority(void) {
 start(true);visible();seal(14);submit();uint64_t original=paper_token;
 seal(14);
 for(unsigned n=0;!raster_ready_to_submit&&n<20;n++)assert(raster_progress());
 assert(raster_sealed&&raster_ready_to_submit&&paper_token==original);
 quick.ui.position_q8=128;quick.ui.target_q8=256;
 /* Periodic timer paint must not refill/replace a snapshot queue indefinitely
  * while Quick waits for the existing immutable submissions to drain. */
 assert(!portable_paper_frame_ready());
 assert(raster_sealed&&paper_token==original);
 mock_ms+=mock_latency;submit();assert(paper_token&&paper_token!=original);
 complete_frame();assert(!paper_token&&!raster_sealed);
 assert(!portable_paper_frame_ready());
 /* Modal painting and the retained underlay still use the same admission API. */
 quick_modal=true;seal(0);complete_frame();quick_modal=false;
 quick_restoring=true;seal(0);complete_frame();quick_restoring=false;
 quick.ui.position_q8=quick.ui.target_q8=0;
 seal(14);complete_frame();neutral();fresh_tap();end_test();
}
#endif
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
 start(true);neutral();mock_x=mock_y=14;seal(14);submit();
 mock_down=true;t5_app_input_t out=step();
 assert(!out.tapped&&!app_contact.down&&quick.ui.gesture==PQA_TOP_PENDING);
 assert(!timecard_native_quick_eligible&&!timecard_native_visible);
 uint64_t down_sequence=timecard_native_quick_sequence;
 complete_frame();mock_down=false;
 for(unsigned n=0;n<4;n++){out=step();assert(!out.tapped&&!app_contact.down);}
 assert(touch.sequence>down_sequence&&!timecard_native_owned());
 mock_x=mock_y=120;fresh_tap();end_test();
 /* Layout replacement cancels both a reserved edge and an already split
  * replay whose release is queued in the adapter's existing replay slot. */
 for(unsigned split=0;split<2;split++) {
  start(true);visible();mock_x=mock_y=14;mock_down=true;out=step();
  assert(!out.tapped&&!app_contact.down&&timecard_native_quick_eligible);
  if(split){mock_down=false;out=step();assert(app_contact.down&&!out.tapped&&quick_replay_delivery);}
  portable_timecard_native_layout(2);assert(!timecard_native_quick_eligible);
  seal(14);complete_frame();mock_down=false;
  for(unsigned n=0;n<4;n++){out=step();assert(!out.tapped&&!app_contact.down);}
  mock_x=mock_y=120;fresh_tap();end_test();
 }
 /* Quick takes over a held native contact, then closes while it is still
  * held. Its own stream survives; the eventual UP cannot activate the app. */
 start(true);visible();mock_down=true;out=step();assert(timecard_native_owned());
 quick_modal=true;quick.ui.gesture=PQA_PANEL_DRAG;quick.ui.target_q8=256;
 seal(0);complete_frame();assert(touch.down&&!timecard_native_owned());
 quick_modal=false;quick.ui.position_q8=quick.ui.target_q8=0;pqa_cancel_input(&quick.ui);
 assert(!portable_timecard_native_repaint());mock_down=false;
 for(unsigned n=0;n<4;n++){out=step();assert(!out.tapped&&!app_contact.down);}
 assert(portable_timecard_native_repaint());seal(14);complete_frame();neutral();fresh_tap();end_test();
}
#endif

static portable_timecard_contact contact(void) {
 portable_timecard_contact c;
 assert(portable_timecard_native_contact(&c));return c;
}
static void scroll_contact_lifecycle(void) {
 start(true);visible();portable_timecard_contact c=contact();
 assert(c.valid&&!c.down&&!c.began&&!c.released&&!c.cancelled);
 assert(!portable_timecard_native_contact(NULL));
 mock_down=true;(void)step();c=contact();
 assert(c.valid&&c.began&&c.down&&!c.released&&!c.cancelled&&c.x==120&&c.y==120);
 unsigned polls=mock_polls,queued=mock_event_count;uint64_t sequence=touch.sequence;
 (void)contact();(void)contact();assert(mock_polls==polls&&mock_event_count==queued&&touch.sequence==sequence);
 (void)step();c=contact();assert(c.valid&&c.down&&!c.began&&!c.released&&!c.cancelled);
 mock_y=160;(void)step();c=contact();
 assert(c.valid&&c.down&&!c.began&&c.y==160&&touch.moved&&timecard_native_owned());
 /* Drag-only levels survive the tap slop and same-map async refresh. */
 seal(14);complete_frame();(void)step();c=contact();
 assert(c.valid&&c.down&&c.y==160&&!c.began&&!c.cancelled);
 mock_down=false;t5_app_input_t out=step();c=contact();
 assert(c.valid&&c.released&&!c.down&&!c.cancelled&&!out.tapped);
 (void)step();c=contact();assert(c.valid&&!c.released&&!c.down);
 mock_y=120;fresh_tap();
 /* App-neutral navigation/handoff must not expose the prior release twice. */
 handoff_requested=true;assert(poll(&out,0)&&out.exit_requested);
 c=contact();assert(c.cancelled&&!c.released&&!c.down);handoff_requested=false;
 mock_down=true;(void)step();assert(contact().down);
 portable_timecard_native_layout(2);c=contact();assert(c.cancelled&&!c.down);
 seal(14);complete_frame();mock_down=false;out=step();c=contact();
 assert(!out.tapped&&!c.released);neutral();fresh_tap();end_test();
}
static void malformed_contact_release(void) {
 /* A stale sequence, wrong contact, or UP delivered before MOVE must cancel
  * the whole contact rather than release a scrolling controller or a button. */
 for(unsigned malformed=0;malformed<3;malformed++) {
  start(true);visible();mock_down=true;(void)step();assert(contact().began);
  if(malformed==2){mock_y=150;assert(mock_poll_touch(NULL,1));}
  mock_down=false;assert(mock_poll_touch(NULL,1));
  if(malformed==0)mock_events[mock_event_head].sequence--;
  else if(malformed==1)mock_events[mock_event_head].id++;
  else {
   unsigned next=(mock_event_head+1)%RISC_TOUCH_QUEUE_LENGTH;
   risc_touch_event_v1 saved=mock_events[mock_event_head];
   mock_events[mock_event_head]=mock_events[next];mock_events[next]=saved;
  }
  t5_app_input_t out=step();portable_timecard_contact c=contact();
  assert(c.cancelled&&!c.released&&!c.down&&!out.tapped&&!timecard_native_owned());
  mock_y=120;
  for(unsigned n=0;n<5;n++){out=step();c=contact();assert(!out.tapped&&!c.released);}
  fresh_tap();end_test();
 }
}
static void identical_pixels_recovery(void) {
 start(true);visible();
 /* Opt in to the existing RGB damage history. The ordinary fixture disables
  * it to isolate token custody; this case restores byte-identical foreign pixels. */
 previous_pixels=mock_malloc(240u*240u*sizeof(*previous_pixels));
 previous_valid=false;seal(14);complete_frame();assert(previous_valid);
 timecard_native_frame(0);assert(portable_timecard_native_repaint());
 unsigned before=mock_presents;
 seal(14);submit();assert(mock_presents==before+1&&paper_token&&native_token_owner==14);
 complete_frame();neutral();assert(portable_timecard_native_ready(NULL,NULL));fresh_tap();end_test();
}
#ifdef PORTABLE_QUICK_ACTIONS
#ifdef ALARM_SERVICE_SLEEP_RESUME_SUPPORTED
static void canonical_api1_outputs(void) {
 start(true);visible();
 assert(alarms.api->struct_size==sizeof(alarm_service_sleep_v1));
 assert(((const alarm_service_sleep_v1 *)alarms.api)->resume_sleep==timecard_alarm_resume);
 assert(quick_alarm_output_modes()==ALARM_MODE_BOTH&&!timecard_resume_calls);
 end_test();
}
#endif
static void quick_contact_clear(void) {
 start(true);visible();fresh_tap();assert(contact().released);
 mock_x=mock_y=14;mock_down=true;(void)step();
 portable_timecard_contact c=contact();assert(c.cancelled&&!c.released&&!c.down);
 mock_down=false;(void)step();c=contact();assert(c.valid&&c.began&&c.down&&!c.released);
 (void)step();c=contact();assert(c.valid&&!c.down&&c.released);
 mock_x=mock_y=120;end_test();
}
#endif

static void failed_status(void) {
 start(true);visible();seal(14);submit();mock_fail_status=true;
 assert(!paper_present_progress()&&failed&&!portable_timecard_native_ready(NULL,NULL));
 uint64_t token=paper_token;unsigned grants=mock_grants,allocations=mock_live;
 app_module_fini();assert(paper_token==token&&native_present_token==token);
 assert(mock_grants==grants&&mock_subscriptions==1&&mock_live==allocations);
}
int main(void) {
 scroll_contact_lifecycle();malformed_contact_release();identical_pixels_recovery();
 repaint_and_queue();layout_reset_and_stale();held_and_foreign();foreign_preserves_raw();synchronous();
#ifdef PORTABLE_RASTER_SNAPSHOT
 pipelined_snapshots();
#endif
 repaint_fences();back_request();module_reentry();
#ifdef PORTABLE_QUICK_ACTIONS
 quick_replay();quick_replay_fences();quick_contact_clear();
#ifdef ALARM_SERVICE_SLEEP_RESUME_SUPPORTED
 canonical_api1_outputs();
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
 quick_snapshot_priority();
#endif
#endif
 failed_status();
 puts("Timecard production adapter: frame-token owner14, pending/queued/held scrolling, cancelled/reordered releases, read-only current-poll contact, identical-pixel recovery, stale tokens, immutable snapshot/reset identity, foreign owners0..13 with raw custody, synchronous completion, repaint fences, Quick top-edge replay and snapshot priority, queued adapter Back, module re-entry, failure and cleanup PASS");
}
