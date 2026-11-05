/* LoRa-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableLoRaNative.h"
static uint64_t lora_native_generation,lora_native_layout_id;
static uint64_t lora_native_submit_generation,lora_native_submit_layout;
static uint64_t lora_native_token_generation,lora_native_token_layout;
static uint64_t lora_native_visible_sequence,lora_native_gesture_generation;
static uint8_t lora_native_gesture_contact;
static bool lora_native_enabled;
static bool lora_native_raw_valid,lora_native_release_valid;
static uint64_t lora_native_raw_generation;
static uint8_t lora_native_raw_contact;
static uint16_t lora_native_raw_x,lora_native_raw_y,lora_native_start_x,lora_native_start_y;
static bool lora_native_layout_known,lora_native_visible;
static bool lora_native_sequence_valid,lora_native_gesture;
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_lora_native_generation,raster_lora_native_layout;
#endif
static void lora_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(lora_native_generation==UINT64_MAX){failed=true;return;}
 ++lora_native_generation;
}
bool portable_lora_native_begin(void) {
 if(failed||surface_format!=RISC_DISPLAY_FORMAT_RGB565)return false;
 lora_native_enabled=true;
 lora_native_advance();
 lora_native_layout_known=lora_native_visible=false;
 lora_native_sequence_valid=lora_native_gesture=false;
 lora_native_raw_valid=lora_native_release_valid=false;
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
 return !failed;
}
void portable_lora_native_layout(uint64_t id) {
 if(lora_native_layout_known&&lora_native_layout_id==id)return;
 lora_native_advance();lora_native_layout_id=id;lora_native_layout_known=true;
 lora_native_visible=lora_native_gesture=false;
 lora_native_raw_valid=lora_native_release_valid=false;
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool lora_native_frame_matches(void) {
 return !paper_token||(native_token_owner==11&&
   lora_native_token_generation==lora_native_generation&&
   lora_native_token_layout==lora_native_layout_id);
}
/* Observe the original ordered sample before Quick reserves its top edge.
 * Replayed releases must prove that their original DOWN belonged to this
 * already-visible generation; current release coordinates alone cannot do so. */
static void lora_native_observe(const portable_touch_sample *sample) {
 lora_native_release_valid=false;
 if(!lora_native_enabled)return;
 if(!sample->valid||sample->cancelled){lora_native_raw_valid=false;return;}
 if(sample->began){
  lora_native_raw_valid=sample->tap_eligible&&lora_native_visible&&lora_native_frame_matches()&&lora_native_sequence_valid&&touch.sequence>lora_native_visible_sequence;
  lora_native_raw_generation=lora_native_generation;lora_native_raw_contact=sample->contact_id;
  lora_native_raw_x=sample->x;lora_native_raw_y=sample->y;
 }
 if(sample->moved)lora_native_raw_valid=false;
}
static bool lora_native_owned(void) {
 return lora_native_gesture&&lora_native_layout_known&&
   lora_native_gesture_generation==lora_native_generation;
}
static void lora_native_presented(void) {
 if(!lora_native_layout_known||failed||
    lora_native_token_generation!=lora_native_generation||
    lora_native_token_layout!=lora_native_layout_id)return;
 /* Repainting identical controls cannot make an already visible hit map
  * disappear. Preserve queued starts as well as owned gestures; only a new
  * layout generation establishes a fresh visibility watermark. */
 if(lora_native_visible&&!lora_native_owned())return;
 risc_touch_snapshot_v1 snapshot={0};
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  lora_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
  lora_native_visible=lora_native_gesture=false;
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 if(!lora_native_owned()) {
  lora_native_gesture=false;
  /* A held contact first seen behind the frame must still reach neutral. A
   * neutral snapshot needs only a visibility watermark: resetting it on every
   * telemetry repaint would perpetually re-arm QuickActions' cancellation
   * gate when display latency exceeds the scanner refresh interval. Queued
   * pre-visibility edges remain ordered and cannot start a native gesture. */
  if(!touch.initialized||touch.resync||snapshot.contact_count)input_reset();
 }
 lora_native_visible_sequence=snapshot.sequence;
 lora_native_sequence_valid=touch.initialized&&!touch.resync;
 lora_native_visible=lora_native_sequence_valid&&!failed;
}
static bool lora_native_input_accept(const portable_touch_sample *sample) {
 /* The native selector may coexist with the original paper presentation.
  * Its input remains untouched unless the app explicitly begins native UI. */
 if(!lora_native_enabled)return true;
 if(!sample->valid||sample->cancelled){lora_native_gesture=false;return false;}
 if(sample->began) {
  bool fresh=lora_native_visible&&lora_native_frame_matches()&&lora_native_sequence_valid&&
    touch.sequence>lora_native_visible_sequence;
  if(sample->released)fresh=fresh&&lora_native_raw_valid&&lora_native_raw_generation==lora_native_generation&&lora_native_raw_contact==sample->contact_id;
  if(!fresh){lora_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  lora_native_start_x=sample->released?lora_native_raw_x:sample->x;
  lora_native_start_y=sample->released?lora_native_raw_y:sample->y;
  lora_native_gesture=true;lora_native_gesture_generation=lora_native_generation;
  lora_native_gesture_contact=sample->contact_id;
 }
 bool owned=lora_native_owned()&&sample->contact_id==lora_native_gesture_contact;
 if(sample->released){lora_native_gesture=false;lora_native_release_valid=owned&&sample->tap_eligible&&!sample->moved;lora_native_raw_valid=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_lora_native_present(bool full) {
 if(!lora_native_enabled||!lora_native_layout_known||failed)return;
 lora_native_submit_generation=lora_native_generation;
 lora_native_submit_layout=lora_native_layout_id;
 portable_native_present(11,full);
}

bool portable_lora_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 /* Completion can establish visible pixels before the reset cancellation and
  * QuickActions neutral gate have drained. Do not advertise action readiness
  * until those inherited-input fences settle. This is observation only. */
 if(generation)*generation=lora_native_generation;
 if(visible_sequence)*visible_sequence=lora_native_visible_sequence;
 return lora_native_enabled&&!failed&&lora_native_layout_known&&lora_native_visible&&
   lora_native_sequence_valid&&lora_native_frame_matches()&&!touch.cancel_pending&&!touch.resync
#ifdef PORTABLE_QUICK_ACTIONS
   &&!quick.ui.neutral_gate
#endif
   ;
}

/* One delivered release only; this accessor reads no provider or event. */
bool portable_lora_native_tap_origin(uint16_t *x,uint16_t *y) {
 if(!lora_native_release_valid||!lora_native_layout_known||failed)return false;
 if(x)*x=lora_native_start_x;
 if(y)*y=lora_native_start_y;
 return true;
}
