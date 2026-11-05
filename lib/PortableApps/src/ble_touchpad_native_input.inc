/* Independent owner7. The app owns its multi-contact raw subscription; this
 * reducer owns only adapter chrome and completed-frame identity. No raw event
 * is read ahead, and an unchanged repaint never adopts a newer watermark. */
#include "PortableBleTouchpadNative.h"
static uint64_t ble_touchpad_native_generation,ble_touchpad_native_layout_id;
static uint64_t ble_touchpad_native_submit_generation,ble_touchpad_native_submit_layout;
static uint64_t ble_touchpad_native_token_generation,ble_touchpad_native_token_layout;
static uint64_t ble_touchpad_native_visible_sequence,ble_touchpad_native_gesture_generation;
static uint8_t ble_touchpad_native_gesture_contact;
static bool ble_touchpad_native_layout_known,ble_touchpad_native_visible;
static bool ble_touchpad_native_sequence_valid,ble_touchpad_native_gesture;
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_ble_touchpad_native_generation,raster_ble_touchpad_native_layout;
#endif
static bool ble_touchpad_native_enabled(void) {
 return surface_format==RISC_DISPLAY_FORMAT_RGB565;
}
static void ble_touchpad_native_invalidate(void) {
 ble_touchpad_native_visible=ble_touchpad_native_sequence_valid=ble_touchpad_native_gesture=false;
}
static void ble_touchpad_native_advance(void) {
 /* Never recycle an identity while an older immutable frame can still own it. */
 if(ble_touchpad_native_generation==UINT64_MAX){failed=true;return;}
 ++ble_touchpad_native_generation;
}
void portable_ble_touchpad_native_begin(void) {
 if(!ble_touchpad_native_enabled())return;
 ble_touchpad_native_advance();ble_touchpad_native_invalidate();
 ble_touchpad_native_layout_known=false;input_reset();
 /* Preserve paper/native tokens and sealed snapshots. Their original owner,
  * generation and layout must drain even after an app reset. */
}
void portable_ble_touchpad_native_layout(uint64_t id) {
 if(!ble_touchpad_native_enabled()||failed)return;
 if(ble_touchpad_native_layout_known&&ble_touchpad_native_layout_id==id)return;
 ble_touchpad_native_advance();ble_touchpad_native_invalidate();
 ble_touchpad_native_layout_id=id;ble_touchpad_native_layout_known=true;
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool ble_touchpad_native_owned(void) {
 return ble_touchpad_native_gesture&&ble_touchpad_native_layout_known&&
   ble_touchpad_native_gesture_generation==ble_touchpad_native_generation;
}
static void ble_touchpad_native_frame(unsigned owner) {
 if(!ble_touchpad_native_enabled()||owner==7||!ble_touchpad_native_layout_known)return;
 /* An overlay/foreign owner cannot inherit this hit mapping. Fence older
  * completions as soon as the foreign image is sealed/submitted. */
 ble_touchpad_native_advance();ble_touchpad_native_invalidate();
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static void ble_touchpad_native_presented(void) {
 if(!ble_touchpad_native_enabled()||!ble_touchpad_native_layout_known||failed||
    ble_touchpad_native_token_generation!=ble_touchpad_native_generation||
    ble_touchpad_native_token_layout!=ble_touchpad_native_layout_id)return;
 risc_touch_snapshot_v1 snapshot={0};
 if(touch.terminal||!touch.subscription||!touch.api){ble_touchpad_native_invalidate();return;}
 if(!touch.api->snapshot(touch.api->context,&snapshot)) {
  ble_touchpad_native_invalidate();portable_touch_terminal(&touch,"snapshot",0);return;
 }
 if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence||
    snapshot.timestamp_ms<touch.timestamp_ms||
    (touch.initialized&&(snapshot.width!=touch.width||snapshot.height!=touch.height))||
    (snapshot.sequence==touch.sequence&&!portable_touch_snapshot_matches(&touch,&snapshot))) {
  ble_touchpad_native_advance();ble_touchpad_native_invalidate();
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 /* Identical visible mappings remain actionable during repaint. In particular,
  * do not reset/reseed the reducer, clear pending input, or advance the app's
  * fence: a complete contact may already be queued at this newer snapshot. */
 if(ble_touchpad_native_visible&&ble_touchpad_native_sequence_valid)return;
 uint64_t minimum_sequence=snapshot.sequence;
 input_reset();
 if(touch.terminal||touch.resync||touch.poll_failed||!touch.initialized)return;
 if(touch.sequence<minimum_sequence) {
  ble_touchpad_native_advance();ble_touchpad_native_invalidate();
  portable_touch_fault(&touch,"snapshot-data",0);return;
 }
 ble_touchpad_native_visible_sequence=touch.sequence;
 ble_touchpad_native_sequence_valid=ble_touchpad_native_visible=!failed;
}
bool portable_ble_touchpad_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 if(generation)*generation=ble_touchpad_native_generation;
 if(visible_sequence)*visible_sequence=ble_touchpad_native_visible_sequence;
 return ble_touchpad_native_enabled()&&!failed&&!touch.terminal&&!touch.resync&&!touch.poll_failed&&
   ble_touchpad_native_layout_known&&ble_touchpad_native_visible&&ble_touchpad_native_sequence_valid;
}
static bool ble_touchpad_native_input_accept(const portable_touch_sample *sample) {
 if(!ble_touchpad_native_enabled())return true;
 if(!sample->valid||sample->cancelled){ble_touchpad_native_gesture=false;return false;}
 if(sample->began) {
  bool fresh=portable_ble_touchpad_native_ready(NULL,NULL)&&sample->tap_eligible&&
    touch.sequence>ble_touchpad_native_visible_sequence;
  if(!fresh){ble_touchpad_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  ble_touchpad_native_gesture=true;ble_touchpad_native_gesture_generation=ble_touchpad_native_generation;
  ble_touchpad_native_gesture_contact=sample->contact_id;
 }
 bool owned=portable_ble_touchpad_native_ready(NULL,NULL)&&ble_touchpad_native_owned()&&
   sample->contact_id==ble_touchpad_native_gesture_contact;
 if(sample->released){ble_touchpad_native_gesture=false;return owned;}
 return sample->down&&owned;
}
void portable_ble_touchpad_native_present(bool full) {
 if(!ble_touchpad_native_enabled()){portable_native_present(0,full);return;}
 if(!ble_touchpad_native_layout_known||failed)return;
 ble_touchpad_native_submit_generation=ble_touchpad_native_generation;
 ble_touchpad_native_submit_layout=ble_touchpad_native_layout_id;
 portable_native_present(7,full);
}
