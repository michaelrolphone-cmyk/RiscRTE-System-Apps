/* BLE Buttons-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableBleButtonsNative.h"
static uint64_t ble_buttons_native_generation,ble_buttons_native_layout_id;
static uint64_t ble_buttons_native_submit_generation,ble_buttons_native_submit_layout;
static uint64_t ble_buttons_native_token_generation,ble_buttons_native_token_layout;
static uint64_t ble_buttons_native_visible_sequence,ble_buttons_native_gesture_generation;
static uint8_t ble_buttons_native_gesture_contact;
static bool ble_buttons_native_layout_known,ble_buttons_native_visible;
static bool ble_buttons_native_sequence_valid,ble_buttons_native_gesture;
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_ble_buttons_native_generation,raster_ble_buttons_native_layout;
#endif
static void ble_buttons_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(ble_buttons_native_generation==UINT64_MAX){failed=true;return;}
 ++ble_buttons_native_generation;
}
void portable_ble_buttons_native_begin(void) {
 ble_buttons_native_advance();
 ble_buttons_native_layout_known=ble_buttons_native_visible=false;
 ble_buttons_native_sequence_valid=ble_buttons_native_gesture=false;
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_ble_buttons_native_layout(uint64_t id) {
 if(ble_buttons_native_layout_known&&ble_buttons_native_layout_id==id)return;
 ble_buttons_native_advance();ble_buttons_native_layout_id=id;ble_buttons_native_layout_known=true;
 ble_buttons_native_visible=ble_buttons_native_gesture=false;
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool ble_buttons_native_owned(void) {
 return ble_buttons_native_gesture&&ble_buttons_native_layout_known&&
   ble_buttons_native_gesture_generation==ble_buttons_native_generation;
}
static void ble_buttons_native_presented(void) {
 if(!ble_buttons_native_layout_known||failed||
    ble_buttons_native_token_generation!=ble_buttons_native_generation||
    ble_buttons_native_token_layout!=ble_buttons_native_layout_id)return;
 if(ble_buttons_native_owned()) {
  risc_touch_snapshot_v1 snapshot={0};
  if(!touch.api->snapshot(touch.api->context,&snapshot)) {
   ble_buttons_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
  }
  if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
   ble_buttons_native_visible=ble_buttons_native_gesture=false;
   portable_touch_fault(&touch,"snapshot-data",0);return;
  }
  ble_buttons_native_visible_sequence=snapshot.sequence;
  ble_buttons_native_sequence_valid=true;
 }else{
  ble_buttons_native_gesture=false;input_reset();
  ble_buttons_native_visible_sequence=touch.sequence;
  ble_buttons_native_sequence_valid=touch.initialized&&!touch.resync;
 }
 ble_buttons_native_visible=ble_buttons_native_sequence_valid&&!failed;
}
static bool ble_buttons_native_input_accept(const portable_touch_sample *sample) {
 if(!sample->valid||sample->cancelled){ble_buttons_native_gesture=false;return false;}
 if(sample->began) {
  bool fresh=ble_buttons_native_visible&&!paper_token&&ble_buttons_native_sequence_valid&&
    touch.sequence>ble_buttons_native_visible_sequence;
  if(!fresh){ble_buttons_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  ble_buttons_native_gesture=true;ble_buttons_native_gesture_generation=ble_buttons_native_generation;
  ble_buttons_native_gesture_contact=sample->contact_id;
 }
 bool owned=ble_buttons_native_owned()&&sample->contact_id==ble_buttons_native_gesture_contact;
 if(sample->released){ble_buttons_native_gesture=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_ble_buttons_native_present(bool full) {
 if(!ble_buttons_native_layout_known||failed)return;
 ble_buttons_native_visible=false;
 ble_buttons_native_submit_generation=ble_buttons_native_generation;
 ble_buttons_native_submit_layout=ble_buttons_native_layout_id;
 portable_native_present(5,full);
}

bool portable_ble_buttons_native_ready(uint64_t *generation,uint64_t *visible_sequence) {
 if(generation)*generation=ble_buttons_native_generation;
 if(visible_sequence)*visible_sequence=ble_buttons_native_visible_sequence;
 return !failed&&ble_buttons_native_layout_known&&ble_buttons_native_visible&&
   ble_buttons_native_sequence_valid&&!paper_token;
}
