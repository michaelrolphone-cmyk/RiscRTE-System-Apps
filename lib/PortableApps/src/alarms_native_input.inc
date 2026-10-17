/* Alarms-only presentation/input ownership. Physical navigation and provider
 * custody stay with the existing adapter. No provider event is read ahead. */
#include "PortableAlarmsNative.h"
static uint64_t alarms_native_generation,alarms_native_layout_id;
static uint64_t alarms_native_submit_generation,alarms_native_submit_layout;
static uint64_t alarms_native_token_generation,alarms_native_token_layout;
static uint64_t alarms_native_visible_sequence,alarms_native_gesture_generation;
static uint8_t alarms_native_gesture_contact;
static bool alarms_native_layout_known,alarms_native_visible;
static bool alarms_native_sequence_valid,alarms_native_gesture;
#ifdef PORTABLE_RASTER_SNAPSHOT
static uint64_t raster_alarms_native_generation,raster_alarms_native_layout;
#endif
static void alarms_native_advance(void) {
 /* Never recycle a generation while an older frame could still own it. */
 if(alarms_native_generation==UINT64_MAX){failed=true;return;}
 ++alarms_native_generation;
}
void portable_alarms_native_begin(void) {
 alarms_native_advance();
 alarms_native_layout_known=alarms_native_visible=false;
 alarms_native_sequence_valid=alarms_native_gesture=false;
 input_reset();
 /* In particular, do not clear native_present_token, paper_token, or a sealed
  * raster snapshot here. Their original owner/generation must drain normally. */
}
void portable_alarms_native_layout(uint64_t id) {
 if(alarms_native_layout_known&&alarms_native_layout_id==id)return;
 alarms_native_advance();alarms_native_layout_id=id;alarms_native_layout_known=true;
 alarms_native_visible=alarms_native_gesture=false;
 input_pending=false;input_sample=(portable_touch_sample){0};
 portable_touch_cancel_gesture(&touch);
}
static bool alarms_native_owned(void) {
 return alarms_native_gesture&&alarms_native_layout_known&&
   alarms_native_gesture_generation==alarms_native_generation;
}
static void alarms_native_presented(void) {
 if(!alarms_native_layout_known||failed||
    alarms_native_token_generation!=alarms_native_generation||
    alarms_native_token_layout!=alarms_native_layout_id)return;
 if(alarms_native_owned()) {
  risc_touch_snapshot_v1 snapshot={0};
  if(!touch.api->snapshot(touch.api->context,&snapshot)) {
   alarms_native_visible=false;portable_touch_terminal(&touch,"snapshot",0);return;
  }
  if(!portable_touch_snapshot_valid(&snapshot)||snapshot.sequence<touch.sequence) {
   alarms_native_visible=alarms_native_gesture=false;
   portable_touch_fault(&touch,"snapshot-data",0);return;
  }
  alarms_native_visible_sequence=snapshot.sequence;
  alarms_native_sequence_valid=true;
 }else{
  alarms_native_gesture=false;input_reset();
  alarms_native_visible_sequence=touch.sequence;
  alarms_native_sequence_valid=touch.initialized&&!touch.resync;
 }
 alarms_native_visible=alarms_native_sequence_valid&&!failed;
}
static bool alarms_native_input_accept(const portable_touch_sample *sample) {
 if(!sample->valid||sample->cancelled){alarms_native_gesture=false;return false;}
 if(sample->began) {
  bool fresh=alarms_native_visible&&!paper_token&&alarms_native_sequence_valid&&
    touch.sequence>alarms_native_visible_sequence;
  if(!fresh){alarms_native_gesture=false;portable_touch_cancel_gesture(&touch);return false;}
  alarms_native_gesture=true;alarms_native_gesture_generation=alarms_native_generation;
  alarms_native_gesture_contact=sample->contact_id;
 }
 bool owned=alarms_native_owned()&&sample->contact_id==alarms_native_gesture_contact;
 if(sample->released){alarms_native_gesture=false;return owned;}
 if(sample->down)return owned;
 return false;
}
void portable_alarms_native_present(bool full) {
 if(!alarms_native_layout_known||failed)return;
 alarms_native_visible=false;
 alarms_native_submit_generation=alarms_native_generation;
 alarms_native_submit_layout=alarms_native_layout_id;
 portable_native_present(4,full);
}
