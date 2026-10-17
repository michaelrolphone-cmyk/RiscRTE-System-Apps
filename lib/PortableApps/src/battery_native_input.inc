/* Battery-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableBatteryNative.h"
static uint64_t battery_native_generation,battery_native_layout_id;
static uint64_t battery_native_submit_generation,battery_native_submit_layout;
static uint64_t battery_native_token_generation,battery_native_token_layout;
static uint64_t battery_native_visible_sequence,battery_native_gesture_generation;
static uint8_t battery_native_gesture_contact;
static bool battery_native_layout_known,battery_native_visible;
static bool battery_native_sequence_valid,battery_native_gesture;
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_battery_native_generation,raster_battery_native_layout;
#endif
static void battery_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(battery_native_generation==UINT64_MAX){failed=true;return;}
 ++battery_native_generation;
}
void portable_battery_native_begin(void) {
 battery_native_advance();
 battery_native_layout_known=battery_native_visible=false;
 battery_native_sequence_valid=battery_native_gesture=false;
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_battery_native_layout(uint64_t id) {
 if(battery_native_layout_known&&battery_native_layout_id==id)return;
 battery_native_advance();battery_native_layout_id=id;battery_native_layout_known=true;
 battery_native_visible=battery_native_gesture=false;
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool battery_native_owned(void) {
 return battery_native_gesture&&battery_native_layout_known&&
   battery_native_gesture_generation==battery_native_generation;
}
static void battery_native_presented(void) {
 if(!battery_native_layout_known||failed||
    battery_native_token_generation!=battery_native_generation||
    battery_native_token_layout!=battery_native_layout_id)return;
 if(battery_native_owned()) {
  risc_touch_snapshot_v1 snapshot={0};
  if(!touch.api->snapshot(touch.api->context,&snapshot)) {
   battery_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
  }
  if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
   battery_native_visible=battery_native_gesture=false;
   portable_touch_fault(&touch,"snapshot-data",0);return;
  }
  battery_native_visible_sequence=snapshot.sequence;
  battery_native_sequence_valid=true;
 }else{
  battery_native_gesture=false;input_reset();
  battery_native_visible_sequence=touch.sequence;
  battery_native_sequence_valid=touch.initialized&&!touch.resync;
 }
 battery_native_visible=battery_native_sequence_valid&&!failed;
}
static bool battery_native_input_accept(const portable_touch_sample *sample) {
 if(!sample->valid||sample->cancelled){battery_native_gesture=false;return false;}
 if(sample->began) {
  bool fresh=battery_native_visible&&!paper_token&&battery_native_sequence_valid&&
    touch.sequence>battery_native_visible_sequence;
  if(!fresh){battery_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  battery_native_gesture=true;battery_native_gesture_generation=battery_native_generation;
  battery_native_gesture_contact=sample->contact_id;
 }
 bool owned=battery_native_owned()&&sample->contact_id==battery_native_gesture_contact;
 if(sample->released){battery_native_gesture=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_battery_native_present(bool full) {
 if(!battery_native_layout_known||failed)return;
 battery_native_visible=false;
 battery_native_submit_generation=battery_native_generation;
 battery_native_submit_layout=battery_native_layout_id;
 portable_native_present(3,full);
}
