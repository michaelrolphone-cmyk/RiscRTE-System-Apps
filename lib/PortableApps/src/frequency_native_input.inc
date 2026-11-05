/* Frequency-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableFrequencyNative.h"
static uint64_t frequency_native_generation,frequency_native_layout_id;
static uint64_t frequency_native_submit_generation,frequency_native_submit_layout;
static uint64_t frequency_native_token_generation,frequency_native_token_layout;
static uint64_t frequency_native_visible_sequence,frequency_native_gesture_generation;
static uint8_t frequency_native_gesture_contact;
static bool frequency_native_enabled;
static bool frequency_native_layout_known,frequency_native_visible;
static bool frequency_native_sequence_valid,frequency_native_gesture;
#ifdef PORTABLE_QUICK_ACTIONS
static uint64_t frequency_native_quick_generation,frequency_native_quick_sequence;
static uint16_t frequency_native_quick_x,frequency_native_quick_y;
static uint8_t frequency_native_quick_contact;
static bool frequency_native_quick_eligible;
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_frequency_native_generation,raster_frequency_native_layout;
#endif
bool portable_frequency_native_available(void) {
 return surface_format==RISC_DISPLAY_FORMAT_RGB565;
}
static void frequency_native_invalidate(void) {
 frequency_native_visible=frequency_native_sequence_valid=frequency_native_gesture=false;
#ifdef PORTABLE_QUICK_ACTIONS
 frequency_native_quick_eligible=false;
#endif
}
static void frequency_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(frequency_native_generation==UINT64_MAX){failed=true;return;}
 ++frequency_native_generation;
}
void portable_frequency_native_begin(void) {
 if(!portable_frequency_native_available())return;
 frequency_native_enabled=true;
 frequency_native_advance();
 frequency_native_layout_known=false;frequency_native_invalidate();
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_frequency_native_layout(uint64_t id) {
 if(!frequency_native_enabled||failed)return;
 if(frequency_native_layout_known&&frequency_native_layout_id==id)return;
 frequency_native_advance();frequency_native_layout_id=id;frequency_native_layout_known=true;
 frequency_native_invalidate();
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool frequency_native_frame_matches(void) {
 return !paper_token||(native_token_owner==10&&
   frequency_native_token_generation==frequency_native_generation&&
   frequency_native_token_layout==frequency_native_layout_id);
}
static bool frequency_native_owned(void) {
 return frequency_native_gesture&&frequency_native_layout_known&&
   frequency_native_gesture_generation==frequency_native_generation;
}
static void frequency_native_frame(unsigned owner) {
 if(!frequency_native_enabled||owner==10||!frequency_native_layout_known)return;
 /* Fence foreign/overlay pixels at seal or submission, before an older native
  * completion could authorize a hit map hidden beneath the incoming frame. */
 frequency_native_advance();frequency_native_invalidate();
 /* The foreign owner keeps its raw contact stream (notably Quick drags).
  * A later native completion establishes its own neutral/sequence boundary. */
}
#ifdef PORTABLE_QUICK_ACTIONS
static void frequency_native_quick_observe(const portable_touch_sample *sample) {
 if(!frequency_native_enabled)return;
 if(!sample->valid||sample->cancelled){frequency_native_quick_eligible=false;return;}
 if(!sample->began)return;
 /* Capture the actual DOWN before Quick can reserve it. A later release or
  * synthesized began must never substitute its newer provider sequence. */
 frequency_native_quick_generation=frequency_native_generation;
 frequency_native_quick_sequence=touch.sequence;
 frequency_native_quick_contact=sample->contact_id;
 frequency_native_quick_x=sample->x;frequency_native_quick_y=sample->y;
 frequency_native_quick_eligible=sample->down&&sample->tap_eligible&&
   portable_frequency_native_ready(NULL,NULL)&&touch.sequence>frequency_native_visible_sequence;
}
static bool frequency_native_quick_replay(const portable_touch_sample *sample) {
 bool eligible=frequency_native_quick_eligible;
 frequency_native_quick_eligible=false;
 return eligible&&sample->valid&&!sample->cancelled&&(sample->down||sample->released)&&
   sample->contact_id==frequency_native_quick_contact&&
   frequency_native_quick_generation==frequency_native_generation&&
   frequency_native_quick_sequence>frequency_native_visible_sequence&&
   portable_frequency_native_ready(NULL,NULL);
}
#endif
static void frequency_native_presented(void) {
 if(!frequency_native_layout_known||failed||
    frequency_native_token_generation!=frequency_native_generation||
    frequency_native_token_layout!=frequency_native_layout_id)return;
 /* Repainting identical controls cannot make an already visible hit map
  * disappear. Preserve queued starts as well as owned gestures; only a new
  * layout generation establishes a fresh visibility watermark. */
 if(frequency_native_visible&&!frequency_native_owned())return;
 risc_touch_snapshot_v1 snapshot={0};
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  frequency_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
  frequency_native_visible=frequency_native_gesture=false;
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 if(!frequency_native_owned()) {
  frequency_native_gesture=false;
  /* A held contact first seen behind the frame must still reach neutral. A
   * neutral snapshot needs only a visibility watermark: resetting it on every
   * telemetry repaint would perpetually re-arm QuickActions' cancellation
   * gate when display latency exceeds the analysis refresh interval. Queued
   * pre-visibility edges remain ordered and cannot start a native gesture. */
  if(!touch.initialized||touch.resync||snapshot.contact_count)input_reset();
 }
 frequency_native_visible_sequence=snapshot.sequence;
 frequency_native_sequence_valid=touch.initialized&&!touch.resync;
 frequency_native_visible=frequency_native_sequence_valid&&!failed;
}
static bool frequency_native_input_accept(const portable_touch_sample *sample) {
 /* The native selector may coexist with the original paper presentation.
  * Its input remains untouched unless the app explicitly begins native UI. */
 if(!frequency_native_enabled)return true;
 if(!sample->valid||sample->cancelled){frequency_native_gesture=false;return false;}
 bool ready=portable_frequency_native_ready(NULL,NULL);
 if(sample->began) {
  bool fresh=ready&&touch.sequence>frequency_native_visible_sequence;
  if(!fresh){frequency_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  frequency_native_gesture=true;frequency_native_gesture_generation=frequency_native_generation;
  frequency_native_gesture_contact=sample->contact_id;
 }
 if(!ready){frequency_native_gesture=false;return false;}
 bool owned=frequency_native_owned()&&
   sample->contact_id==frequency_native_gesture_contact;
 if(sample->released){frequency_native_gesture=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_frequency_native_present(bool full) {
 if(!frequency_native_enabled||!frequency_native_layout_known||failed)return;
 frequency_native_submit_generation=frequency_native_generation;
 frequency_native_submit_layout=frequency_native_layout_id;
 portable_native_present(10,full);
}

bool portable_frequency_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 /* Completion can establish visible pixels before the reset cancellation and
  * QuickActions neutral gate have drained. Do not advertise action readiness
  * until those inherited-input fences settle. This is observation only. */
 if(generation)*generation=frequency_native_generation;
 if(visible_sequence)*visible_sequence=frequency_native_visible_sequence;
 return frequency_native_enabled&&!failed&&!touch.terminal&&!touch.poll_failed&&
   frequency_native_layout_known&&frequency_native_visible&&frequency_native_sequence_valid&&
   frequency_native_frame_matches()&&!touch.cancel_pending&&!touch.resync
#ifdef PORTABLE_QUICK_ACTIONS
   &&!quick.ui.neutral_gate&&!quick_modal&&!pqa_visible(&quick.ui)&&!pqa_capture(&quick.ui)
#endif
   ;
}
