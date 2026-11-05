/* BLE Scanner-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableBleScannerNative.h"
static uint64_t ble_scanner_native_generation,ble_scanner_native_layout_id;
static uint64_t ble_scanner_native_submit_generation,ble_scanner_native_submit_layout;
static uint64_t ble_scanner_native_token_generation,ble_scanner_native_token_layout;
static uint64_t ble_scanner_native_visible_sequence,ble_scanner_native_gesture_generation;
static uint8_t ble_scanner_native_gesture_contact;
static bool ble_scanner_native_enabled;
static bool ble_scanner_native_layout_known,ble_scanner_native_visible;
static bool ble_scanner_native_sequence_valid,ble_scanner_native_gesture;
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_ble_scanner_native_generation,raster_ble_scanner_native_layout;
#endif
static void ble_scanner_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(ble_scanner_native_generation==UINT64_MAX){failed=true;return;}
 ++ble_scanner_native_generation;
}
void portable_ble_scanner_native_begin(void) {
 ble_scanner_native_enabled=true;
 ble_scanner_native_advance();
 ble_scanner_native_layout_known=ble_scanner_native_visible=false;
 ble_scanner_native_sequence_valid=ble_scanner_native_gesture=false;
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_ble_scanner_native_layout(uint64_t id) {
 if(ble_scanner_native_layout_known&&ble_scanner_native_layout_id==id)return;
 ble_scanner_native_advance();ble_scanner_native_layout_id=id;ble_scanner_native_layout_known=true;
 ble_scanner_native_visible=ble_scanner_native_gesture=false;
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool ble_scanner_native_frame_matches(void) {
 return !paper_token||(native_token_owner==6&&
   ble_scanner_native_token_generation==ble_scanner_native_generation&&
   ble_scanner_native_token_layout==ble_scanner_native_layout_id);
}
static bool ble_scanner_native_owned(void) {
 return ble_scanner_native_gesture&&ble_scanner_native_layout_known&&
   ble_scanner_native_gesture_generation==ble_scanner_native_generation;
}
static void ble_scanner_native_presented(void) {
 if(!ble_scanner_native_layout_known||failed||
    ble_scanner_native_token_generation!=ble_scanner_native_generation||
    ble_scanner_native_token_layout!=ble_scanner_native_layout_id)return;
 /* Repainting identical controls cannot make an already visible hit map
  * disappear. Preserve queued starts as well as owned gestures; only a new
  * layout generation establishes a fresh visibility watermark. */
 if(ble_scanner_native_visible&&!ble_scanner_native_owned())return;
 risc_touch_snapshot_v1 snapshot={0};
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  ble_scanner_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
  ble_scanner_native_visible=ble_scanner_native_gesture=false;
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 if(!ble_scanner_native_owned()) {
  ble_scanner_native_gesture=false;
  /* A held contact first seen behind the frame must still reach neutral. A
   * neutral snapshot needs only a visibility watermark: resetting it on every
   * telemetry repaint would perpetually re-arm QuickActions' cancellation
   * gate when display latency exceeds the scanner refresh interval. Queued
   * pre-visibility edges remain ordered and cannot start a native gesture. */
  if(!touch.initialized||touch.resync||snapshot.contact_count)input_reset();
 }
 ble_scanner_native_visible_sequence=snapshot.sequence;
 ble_scanner_native_sequence_valid=touch.initialized&&!touch.resync;
 ble_scanner_native_visible=ble_scanner_native_sequence_valid&&!failed;
}
static bool ble_scanner_native_input_accept(const portable_touch_sample *sample) {
 /* The native selector may coexist with the original paper presentation.
  * Its input remains untouched unless the app explicitly begins native UI. */
 if(!ble_scanner_native_enabled)return true;
 if(!sample->valid||sample->cancelled){ble_scanner_native_gesture=false;return false;}
 if(sample->began) {
  bool fresh=ble_scanner_native_visible&&ble_scanner_native_frame_matches()&&ble_scanner_native_sequence_valid&&
    touch.sequence>ble_scanner_native_visible_sequence;
  if(!fresh){ble_scanner_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  ble_scanner_native_gesture=true;ble_scanner_native_gesture_generation=ble_scanner_native_generation;
  ble_scanner_native_gesture_contact=sample->contact_id;
 }
 bool owned=ble_scanner_native_owned()&&sample->contact_id==ble_scanner_native_gesture_contact;
 if(sample->released){ble_scanner_native_gesture=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_ble_scanner_native_present(bool full) {
 if(!ble_scanner_native_enabled||!ble_scanner_native_layout_known||failed)return;
 ble_scanner_native_submit_generation=ble_scanner_native_generation;
 ble_scanner_native_submit_layout=ble_scanner_native_layout_id;
 portable_native_present(6,full);
}

bool portable_ble_scanner_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 /* Completion can establish visible pixels before the reset cancellation and
  * QuickActions neutral gate have drained. Do not advertise action readiness
  * until those inherited-input fences settle. This is observation only. */
 if(generation)*generation=ble_scanner_native_generation;
 if(visible_sequence)*visible_sequence=ble_scanner_native_visible_sequence;
 return ble_scanner_native_enabled&&!failed&&ble_scanner_native_layout_known&&ble_scanner_native_visible&&
   ble_scanner_native_sequence_valid&&ble_scanner_native_frame_matches()&&!touch.cancel_pending&&!touch.resync
#ifdef PORTABLE_QUICK_ACTIONS
   &&!quick.ui.neutral_gate
#endif
   ;
}
