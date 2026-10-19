/* Stopwatch-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableStopwatchNative.h"
static uint64_t stopwatch_native_generation,stopwatch_native_layout_id;
static uint64_t stopwatch_native_submit_generation,stopwatch_native_submit_layout;
static uint64_t stopwatch_native_token_generation,stopwatch_native_token_layout;
static uint64_t stopwatch_native_visible_sequence,stopwatch_native_gesture_generation;
static uint8_t stopwatch_native_gesture_contact;
static bool stopwatch_native_enabled;
static bool stopwatch_native_layout_known,stopwatch_native_visible;
static bool stopwatch_native_sequence_valid,stopwatch_native_gesture;
#ifdef PORTABLE_QUICK_ACTIONS
static uint64_t stopwatch_native_quick_generation,stopwatch_native_quick_sequence;
static uint16_t stopwatch_native_quick_x,stopwatch_native_quick_y;
static uint8_t stopwatch_native_quick_contact;
static bool stopwatch_native_quick_eligible;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_stopwatch_native_generation,raster_stopwatch_native_layout;
#endif
bool portable_stopwatch_native_available(void) {
 return surface_format==RISC_DISPLAY_FORMAT_RGB565;
}
static void stopwatch_native_invalidate(void) {
 stopwatch_native_visible=stopwatch_native_sequence_valid=stopwatch_native_gesture=false;
#ifdef PORTABLE_QUICK_ACTIONS
 stopwatch_native_quick_eligible=false;
#endif
}
static void stopwatch_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(stopwatch_native_generation==UINT64_MAX){failed=true;return;}
 ++stopwatch_native_generation;
}
void portable_stopwatch_native_begin(void) {
 if(!portable_stopwatch_native_available())return;
 stopwatch_native_enabled=true;
 stopwatch_native_advance();
 stopwatch_native_layout_known=false;stopwatch_native_invalidate();
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_stopwatch_native_layout(uint64_t id) {
 if(!stopwatch_native_enabled||failed)return;
 if(stopwatch_native_layout_known&&stopwatch_native_layout_id==id)return;
 stopwatch_native_advance();stopwatch_native_layout_id=id;stopwatch_native_layout_known=true;
 stopwatch_native_invalidate();
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool stopwatch_native_frame_matches(void) {
 return !paper_token||(native_token_owner==13&&
   stopwatch_native_token_generation==stopwatch_native_generation&&
   stopwatch_native_token_layout==stopwatch_native_layout_id);
}
static bool stopwatch_native_owned(void) {
 return stopwatch_native_gesture&&stopwatch_native_layout_known&&
   stopwatch_native_gesture_generation==stopwatch_native_generation;
}
static void stopwatch_native_frame(unsigned owner) {
 if(!stopwatch_native_enabled||owner==13||!stopwatch_native_layout_known)return;
 /* Fence foreign/overlay pixels at seal or submission, before an older native
  * completion could authorize a hit map hidden beneath the incoming frame. */
 stopwatch_native_advance();stopwatch_native_invalidate();
 /* The foreign owner keeps its raw contact stream (notably Quick drags).
  * A later native completion establishes its own neutral/sequence boundary. */
}
#ifdef PORTABLE_QUICK_ACTIONS
static void stopwatch_native_quick_observe(const portable_touch_sample *sample) {
 if(!stopwatch_native_enabled)return;
 if(!sample->valid||sample->cancelled){stopwatch_native_quick_eligible=false;return;}
 if(!sample->began)return;
 /* Capture the actual DOWN before Quick can reserve it. A later release or
  * synthesized began must never substitute its newer provider sequence. */
 stopwatch_native_quick_generation=stopwatch_native_generation;
 stopwatch_native_quick_sequence=touch.sequence;
 stopwatch_native_quick_contact=sample->contact_id;
 stopwatch_native_quick_x=sample->x;stopwatch_native_quick_y=sample->y;
 stopwatch_native_quick_eligible=sample->down&&sample->tap_eligible&&
   portable_stopwatch_native_ready(NULL,NULL)&&touch.sequence>stopwatch_native_visible_sequence;
}
static bool stopwatch_native_quick_replay(const portable_touch_sample *sample) {
 bool eligible=stopwatch_native_quick_eligible;
 stopwatch_native_quick_eligible=false;
 return eligible&&sample->valid&&!sample->cancelled&&(sample->down||sample->released)&&
   sample->contact_id==stopwatch_native_quick_contact&&
   stopwatch_native_quick_generation==stopwatch_native_generation&&
   stopwatch_native_quick_sequence>stopwatch_native_visible_sequence&&
   portable_stopwatch_native_ready(NULL,NULL);
}
#endif
static void stopwatch_native_presented(void) {
 if(!stopwatch_native_layout_known||failed||
    stopwatch_native_token_generation!=stopwatch_native_generation||
    stopwatch_native_token_layout!=stopwatch_native_layout_id)return;
 /* Repainting identical controls cannot make an already visible hit map
  * disappear. Preserve queued starts as well as owned gestures; only a new
  * layout generation establishes a fresh visibility watermark. */
 if(stopwatch_native_visible&&!stopwatch_native_owned())return;
 risc_touch_snapshot_v1 snapshot={0};
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  stopwatch_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
  stopwatch_native_visible=stopwatch_native_gesture=false;
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 if(!stopwatch_native_owned()) {
  stopwatch_native_gesture=false;
  /* A held contact first seen behind the frame must still reach neutral. A
   * neutral snapshot needs only a visibility watermark: resetting it on every
   * timer repaint would perpetually re-arm QuickActions' cancellation
   * gate when display latency exceeds the timer refresh interval. Queued
   * pre-visibility edges remain ordered and cannot start a native gesture. */
  if(!touch.initialized||touch.resync||snapshot.contact_count)input_reset();
 }
 stopwatch_native_visible_sequence=snapshot.sequence;
 stopwatch_native_sequence_valid=touch.initialized&&!touch.resync;
 stopwatch_native_visible=stopwatch_native_sequence_valid&&!failed;
}
static bool stopwatch_native_input_accept(const portable_touch_sample *sample) {
 /* The native selector may coexist with the original paper presentation.
  * Its input remains untouched unless the app explicitly begins native UI. */
 if(!stopwatch_native_enabled)return true;
 if(!sample->valid||sample->cancelled){stopwatch_native_gesture=false;return false;}
 bool ready=portable_stopwatch_native_ready(NULL,NULL);
 if(sample->began) {
  bool fresh=ready&&touch.sequence>stopwatch_native_visible_sequence;
  if(!fresh){stopwatch_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  stopwatch_native_gesture=true;stopwatch_native_gesture_generation=stopwatch_native_generation;
  stopwatch_native_gesture_contact=sample->contact_id;
 }
 if(!ready){stopwatch_native_gesture=false;return false;}
 bool owned=stopwatch_native_owned()&&
   sample->contact_id==stopwatch_native_gesture_contact;
 if(sample->released){stopwatch_native_gesture=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_stopwatch_native_present(bool full) {
 if(!stopwatch_native_enabled||!stopwatch_native_layout_known||failed)return;
 stopwatch_native_submit_generation=stopwatch_native_generation;
 stopwatch_native_submit_layout=stopwatch_native_layout_id;
 portable_native_present(13,full);
}

bool portable_stopwatch_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 /* Completion can establish visible pixels before the reset cancellation and
  * QuickActions neutral gate have drained. Do not advertise action readiness
  * until those inherited-input fences settle. This is observation only. */
 if(generation)*generation=stopwatch_native_generation;
 if(visible_sequence)*visible_sequence=stopwatch_native_visible_sequence;
 return stopwatch_native_enabled&&!failed&&!touch.terminal&&!touch.poll_failed&&
   stopwatch_native_layout_known&&stopwatch_native_visible&&stopwatch_native_sequence_valid&&
   stopwatch_native_frame_matches()&&!touch.cancel_pending&&!touch.resync
#ifdef PORTABLE_QUICK_ACTIONS
   &&!quick.ui.neutral_gate&&!quick_modal&&!pqa_visible(&quick.ui)&&!pqa_capture(&quick.ui)
#endif
   ;
}
