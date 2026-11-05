/* Points-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortablePointsNative.h"
static uint64_t points_native_generation,points_native_layout_id;
static uint64_t points_native_submit_generation,points_native_submit_layout;
static uint64_t points_native_token_generation,points_native_token_layout;
static uint64_t points_native_visible_sequence,points_native_gesture_generation;
static uint8_t points_native_gesture_contact;
static bool points_native_enabled;
static portable_touch_sample points_native_sample;
bool portable_points_native_contact(portable_points_contact *out) {
 if(!out)return false;
 *out=(portable_points_contact){.valid=points_native_sample.valid,
  .began=points_native_sample.began,.down=points_native_sample.down,
  .released=points_native_sample.released,.cancelled=points_native_sample.cancelled,
  .x=points_native_sample.x,.y=points_native_sample.y};
 return points_native_enabled&&!failed;
}
static bool points_native_layout_known,points_native_visible;
static bool points_native_sequence_valid,points_native_gesture;
#ifdef PORTABLE_QUICK_ACTIONS
static uint64_t points_native_quick_generation,points_native_quick_sequence;
static uint16_t points_native_quick_x,points_native_quick_y;
static uint8_t points_native_quick_contact;
static bool points_native_quick_eligible;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_points_native_generation,raster_points_native_layout;
#endif
bool portable_points_native_available(void) {
 return surface_format==RISC_DISPLAY_FORMAT_RGB565;
}
static void points_native_invalidate(void) {
 points_native_visible=points_native_sequence_valid=points_native_gesture=false;
 points_native_sample=(portable_touch_sample){.cancelled=true};
#ifdef PORTABLE_QUICK_ACTIONS
 points_native_quick_eligible=false;
#endif
}
static void points_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(points_native_generation==UINT64_MAX){failed=true;return;}
 ++points_native_generation;
}
void portable_points_native_begin(void) {
 if(!portable_points_native_available())return;
 points_native_enabled=true;
 points_native_advance();
 points_native_layout_known=false;points_native_invalidate();
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_points_native_layout(uint64_t id) {
 if(!points_native_enabled||failed)return;
 if(points_native_layout_known&&points_native_layout_id==id)return;
 points_native_advance();points_native_layout_id=id;points_native_layout_known=true;
 points_native_invalidate();
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool points_native_frame_matches(void) {
 return !paper_token||(native_token_owner==12&&
   points_native_token_generation==points_native_generation&&
   points_native_token_layout==points_native_layout_id);
}
static bool points_native_owned(void) {
 return points_native_gesture&&points_native_layout_known&&
   points_native_gesture_generation==points_native_generation;
}
static void points_native_frame(unsigned owner) {
 if(!points_native_enabled||owner==12||!points_native_layout_known)return;
 /* Fence foreign/overlay pixels at seal or submission, before an older native
  * completion could authorize a hit map hidden beneath the incoming frame. */
 points_native_advance();points_native_invalidate();
 /* The foreign owner keeps its raw contact stream (notably Quick drags).
  * A later native completion establishes its own neutral/sequence boundary. */
}
#ifdef PORTABLE_QUICK_ACTIONS
static void points_native_quick_observe(const portable_touch_sample *sample) {
 if(!points_native_enabled)return;
 if(!sample->valid||sample->cancelled){points_native_quick_eligible=false;return;}
 if(!sample->began)return;
 /* Capture the actual DOWN before Quick can reserve it. A later release or
  * synthesized began must never substitute its newer provider sequence. */
 points_native_quick_generation=points_native_generation;
 points_native_quick_sequence=touch.sequence;
 points_native_quick_contact=sample->contact_id;
 points_native_quick_x=sample->x;points_native_quick_y=sample->y;
 points_native_quick_eligible=sample->down&&sample->tap_eligible&&
   portable_points_native_ready(NULL,NULL)&&touch.sequence>points_native_visible_sequence;
}
static bool points_native_quick_replay(const portable_touch_sample *sample) {
 bool eligible=points_native_quick_eligible;
 points_native_quick_eligible=false;
 return eligible&&sample->valid&&!sample->cancelled&&(sample->down||sample->released)&&
   sample->contact_id==points_native_quick_contact&&
   points_native_quick_generation==points_native_generation&&
   points_native_quick_sequence>points_native_visible_sequence&&
   portable_points_native_ready(NULL,NULL);
}
#endif
static void points_native_presented(void) {
 if(!points_native_layout_known||failed||
    points_native_token_generation!=points_native_generation||
    points_native_token_layout!=points_native_layout_id)return;
 /* Repainting identical controls cannot make an already visible hit map
  * disappear. Preserve queued starts as well as owned gestures; only a new
  * layout generation establishes a fresh visibility watermark. */
 if(points_native_visible&&!points_native_owned())return;
 risc_touch_snapshot_v1 snapshot={0};
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  points_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
  points_native_visible=points_native_gesture=false;
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 if(!points_native_owned()) {
  points_native_gesture=false;
  /* A held contact first seen behind the frame must still reach neutral. A
   * neutral snapshot needs only a visibility watermark: resetting it on every
   * telemetry repaint would perpetually re-arm QuickActions' cancellation
   * gate when display latency exceeds the analysis refresh interval. Queued
   * pre-visibility edges remain ordered and cannot start a native gesture. */
  if(!touch.initialized||touch.resync||snapshot.contact_count)input_reset();
 }
 points_native_visible_sequence=snapshot.sequence;
 points_native_sequence_valid=touch.initialized&&!touch.resync;
 points_native_visible=points_native_sequence_valid&&!failed;
}
static bool points_native_input_accept(const portable_touch_sample *sample) {
 /* The native selector may coexist with the original paper presentation.
  * Its input remains untouched unless the app explicitly begins native UI. */
 if(!points_native_enabled)return true;
 if(!sample->valid||sample->cancelled){points_native_gesture=false;return false;}
 bool ready=portable_points_native_ready(NULL,NULL);
 if(sample->began) {
  bool fresh=ready&&touch.sequence>points_native_visible_sequence;
  if(!fresh){points_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  points_native_gesture=true;points_native_gesture_generation=points_native_generation;
  points_native_gesture_contact=sample->contact_id;
 }
 if(!ready){points_native_gesture=false;return false;}
 bool owned=points_native_owned()&&
   sample->contact_id==points_native_gesture_contact;
 if(sample->released){points_native_gesture=false;return owned;}
 if(sample->down)return owned;
 /* A valid neutral sample is not a cancellation. Preserve it for the app's
  * shared picker settling and physical navigation without inventing edges. */
 return true;
}
void portable_points_native_present(bool full) {
 if(!points_native_enabled||!points_native_layout_known||failed)return;
 points_native_submit_generation=points_native_generation;
 points_native_submit_layout=points_native_layout_id;
 portable_native_present(12,full);
}

bool portable_points_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 /* Completion can establish visible pixels before the reset cancellation and
  * QuickActions neutral gate have drained. Do not advertise action readiness
  * until those inherited-input fences settle. This is observation only. */
 if(generation)*generation=points_native_generation;
 if(visible_sequence)*visible_sequence=points_native_visible_sequence;
 return points_native_enabled&&!failed&&!touch.terminal&&!touch.poll_failed&&
   points_native_layout_known&&points_native_visible&&points_native_sequence_valid&&
   points_native_frame_matches()&&!touch.cancel_pending&&!touch.resync
#ifdef PORTABLE_QUICK_ACTIONS
   &&!quick.ui.neutral_gate&&!quick_modal&&!pqa_visible(&quick.ui)&&!pqa_capture(&quick.ui)
#endif
   ;
}
