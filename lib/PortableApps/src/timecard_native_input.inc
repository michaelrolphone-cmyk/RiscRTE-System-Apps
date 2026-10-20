/* Timecard-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableTimecardNative.h"
static uint64_t timecard_native_generation,timecard_native_layout_id;
static uint64_t timecard_native_submit_generation,timecard_native_submit_layout;
static uint64_t timecard_native_token_generation,timecard_native_token_layout;
static uint64_t timecard_native_visible_sequence,timecard_native_gesture_generation;
static uint8_t timecard_native_gesture_contact;
static bool timecard_native_enabled;
static portable_touch_sample timecard_native_sample;
bool portable_timecard_native_contact(portable_timecard_contact *out) {
 if(!out)return false;
 *out=(portable_timecard_contact){.valid=timecard_native_sample.valid,
  .began=timecard_native_sample.began,.down=timecard_native_sample.down,
  .released=timecard_native_sample.released,.cancelled=timecard_native_sample.cancelled,
  .x=timecard_native_sample.x,.y=timecard_native_sample.y};
 return timecard_native_enabled&&!failed;
}
static bool timecard_native_layout_known,timecard_native_visible;
static bool timecard_native_sequence_valid,timecard_native_gesture;
#ifdef PORTABLE_QUICK_ACTIONS
static uint64_t timecard_native_quick_generation,timecard_native_quick_sequence;
static uint16_t timecard_native_quick_x,timecard_native_quick_y;
static uint8_t timecard_native_quick_contact;
static bool timecard_native_quick_eligible;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_timecard_native_generation,raster_timecard_native_layout;
#endif
bool portable_timecard_native_available(void) {
 return surface_format==RISC_DISPLAY_FORMAT_RGB565;
}
static void timecard_native_invalidate(void) {
 timecard_native_visible=timecard_native_sequence_valid=timecard_native_gesture=false;
 timecard_native_sample=(portable_touch_sample){.cancelled=true};
#ifdef PORTABLE_QUICK_ACTIONS
 timecard_native_quick_eligible=false;
#endif
}
static void timecard_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(timecard_native_generation==UINT64_MAX){failed=true;return;}
 ++timecard_native_generation;
}
void portable_timecard_native_begin(void) {
 if(!portable_timecard_native_available())return;
 timecard_native_enabled=true;
 timecard_native_advance();
 timecard_native_layout_known=false;timecard_native_invalidate();
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_timecard_native_layout(uint64_t id) {
 if(!timecard_native_enabled||failed)return;
 if(timecard_native_layout_known&&timecard_native_layout_id==id)return;
 timecard_native_advance();timecard_native_layout_id=id;timecard_native_layout_known=true;
 timecard_native_invalidate();
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool timecard_native_frame_matches(void) {
 return !paper_token||(native_token_owner==14&&
   timecard_native_token_generation==timecard_native_generation&&
   timecard_native_token_layout==timecard_native_layout_id);
}
static bool timecard_native_owned(void) {
 return timecard_native_gesture&&timecard_native_layout_known&&
   timecard_native_gesture_generation==timecard_native_generation;
}
static void timecard_native_frame(unsigned owner) {
 if(!timecard_native_enabled||owner==14||!timecard_native_layout_known)return;
 /* Fence foreign/overlay pixels at seal or submission, before an older native
  * completion could authorize a hit map hidden beneath the incoming frame. */
 timecard_native_advance();timecard_native_invalidate();
 /* The foreign owner keeps its raw contact stream (notably Quick drags).
  * A later native completion establishes its own neutral/sequence boundary. */
}
#ifdef PORTABLE_QUICK_ACTIONS
static void timecard_native_quick_observe(const portable_touch_sample *sample) {
 if(!timecard_native_enabled)return;
 if(!sample->valid||sample->cancelled){timecard_native_quick_eligible=false;return;}
 if(!sample->began)return;
 /* Capture the actual DOWN before Quick can reserve it. A later release or
  * synthesized began must never substitute its newer provider sequence. */
 timecard_native_quick_generation=timecard_native_generation;
 timecard_native_quick_sequence=touch.sequence;
 timecard_native_quick_contact=sample->contact_id;
 timecard_native_quick_x=sample->x;timecard_native_quick_y=sample->y;
 timecard_native_quick_eligible=sample->down&&sample->tap_eligible&&
   portable_timecard_native_ready(NULL,NULL)&&touch.sequence>timecard_native_visible_sequence;
}
static bool timecard_native_quick_replay(const portable_touch_sample *sample) {
 bool eligible=timecard_native_quick_eligible;
 timecard_native_quick_eligible=false;
 return eligible&&sample->valid&&!sample->cancelled&&(sample->down||sample->released)&&
   sample->contact_id==timecard_native_quick_contact&&
   timecard_native_quick_generation==timecard_native_generation&&
   timecard_native_quick_sequence>timecard_native_visible_sequence&&
   portable_timecard_native_ready(NULL,NULL);
}
#endif
static void timecard_native_presented(void) {
 if(!timecard_native_layout_known||failed||
    timecard_native_token_generation!=timecard_native_generation||
    timecard_native_token_layout!=timecard_native_layout_id)return;
 /* Repainting identical controls cannot make an already visible hit map
  * disappear. Preserve queued starts as well as owned gestures; only a new
  * layout generation establishes a fresh visibility watermark. */
 if(timecard_native_visible&&!timecard_native_owned())return;
 risc_touch_snapshot_v1 snapshot={0};
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  timecard_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
  timecard_native_visible=timecard_native_gesture=false;
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 if(!timecard_native_owned()) {
  timecard_native_gesture=false;
  /* A held contact first seen behind the frame must still reach neutral. A
   * neutral snapshot needs only a visibility watermark: resetting it on every
   * telemetry repaint would perpetually re-arm QuickActions' cancellation
   * gate when display latency exceeds the analysis refresh interval. Queued
   * pre-visibility edges remain ordered and cannot start a native gesture. */
  if(!touch.initialized||touch.resync||snapshot.contact_count)input_reset();
 }
 timecard_native_visible_sequence=snapshot.sequence;
 timecard_native_sequence_valid=touch.initialized&&!touch.resync;
 timecard_native_visible=timecard_native_sequence_valid&&!failed;
}
static bool timecard_native_input_accept(const portable_touch_sample *sample) {
 /* The native selector may coexist with the original paper presentation.
  * Its input remains untouched unless the app explicitly begins native UI. */
 if(!timecard_native_enabled)return true;
 if(!sample->valid||sample->cancelled){timecard_native_gesture=false;return false;}
 bool ready=portable_timecard_native_ready(NULL,NULL);
 if(sample->began) {
  bool fresh=ready&&touch.sequence>timecard_native_visible_sequence;
  if(!fresh){timecard_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  timecard_native_gesture=true;timecard_native_gesture_generation=timecard_native_generation;
  timecard_native_gesture_contact=sample->contact_id;
 }
 if(!ready){timecard_native_gesture=false;return false;}
 bool owned=timecard_native_owned()&&
   sample->contact_id==timecard_native_gesture_contact;
 if(sample->released){timecard_native_gesture=false;return owned;}
 if(sample->down)return owned;
 /* A valid neutral sample is not a cancellation. Preserve it for the app's
  * shared picker settling and physical navigation without inventing edges. */
 return true;
}
void portable_timecard_native_present(bool full) {
 if(!timecard_native_enabled||!timecard_native_layout_known||failed)return;
 timecard_native_submit_generation=timecard_native_generation;
 timecard_native_submit_layout=timecard_native_layout_id;
 portable_native_present(14,full);
}

bool portable_timecard_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 /* Completion can establish visible pixels before the reset cancellation and
  * QuickActions neutral gate have drained. Do not advertise action readiness
  * until those inherited-input fences settle. This is observation only. */
 if(generation)*generation=timecard_native_generation;
 if(visible_sequence)*visible_sequence=timecard_native_visible_sequence;
 return timecard_native_enabled&&!failed&&!touch.terminal&&!touch.poll_failed&&
   timecard_native_layout_known&&timecard_native_visible&&timecard_native_sequence_valid&&
   timecard_native_frame_matches()&&!touch.cancel_pending&&!touch.resync
#ifdef PORTABLE_QUICK_ACTIONS
   &&!quick.ui.neutral_gate&&!quick_modal&&!pqa_visible(&quick.ui)&&!pqa_capture(&quick.ui)
#endif
   ;
}
