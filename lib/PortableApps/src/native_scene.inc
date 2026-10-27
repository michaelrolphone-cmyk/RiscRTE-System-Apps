#include "PortableNativeScene.h"
#include "PortableNativeSceneState.h"
/* This first composition owns complete color frames. Historical selectors are
 * unaffected; snapshot requires its own sealed-frame integration. Quick uses direct frames. */
#if !defined(PORTABLE_APP_OWNS_TOUCH_CHROME) || !defined(PORTABLE_RF_NATIVE_CANVAS) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE)
#error "native scene requires app-owned touch, native canvas and custody fence"
#endif
#if defined(PORTABLE_RASTER_SNAPSHOT)
#error "native scene snapshot composition is not implemented"
#endif
static portable_native_scene_state native_scene;
static portable_native_scene_contact native_scene_contact;
static bool native_scene_submitting,native_scene_token_owned;
static risc_display_present_token_v1 native_scene_token;
#ifdef PORTABLE_QUICK_ACTIONS
static portable_touch_sample native_scene_quick_down;
static uint64_t native_scene_quick_generation,native_scene_quick_sequence;
static bool native_scene_quick_saved;
#endif
static int32_t width(void),height(void);
static void present(bool full);
static void native_scene_cancel_contact(void) {
 native_scene_contact=(portable_native_scene_contact){.cancelled=true};
}
static void native_scene_invalidate(void) {
 if(!native_scene.enabled)return;
 native_scene_cancel_contact();
#ifdef PORTABLE_QUICK_ACTIONS
 native_scene_quick_saved=false;
#endif
 if(!pns_invalidate(&native_scene))failed=true;
}
bool portable_native_scene_available(void) {return !failed&&display&&width()>0&&height()>0&&surface_format==RISC_DISPLAY_FORMAT_RGB565;}
void portable_native_scene_begin(void) {
 if(!portable_native_scene_available())return;
 if(!pns_begin(&native_scene)){failed=true;return;}
#ifdef PORTABLE_QUICK_ACTIONS
 native_scene_quick_saved=false;
#endif
 native_scene_cancel_contact();input_reset();
}
void portable_native_scene_layout(uint64_t id) {
 if(failed||!native_scene.enabled)return;
 uint64_t before=native_scene.generation;
 if(!pns_layout(&native_scene,id)){failed=true;return;}
 if(before!=native_scene.generation){native_scene_cancel_contact();input_pending=false;portable_touch_cancel_gesture(&touch);}
}
/* No snapshot is requested at the completion callback. A later ordinary poll
 * owns recovery; only a successful stream may establish the visible watermark. */
static void native_scene_observe(void) {
 if(!native_scene.awaiting_observation||failed||touch.terminal||touch.poll_failed||touch.resync)return;
 input_reset();
 if(failed||touch.terminal||touch.poll_failed||touch.resync||!touch.initialized)return;
 pns_observe(&native_scene,touch.sequence);
}
static void native_scene_completed(risc_display_present_token_v1 token) {
 if(token!=native_scene_token)return;
 if(native_scene_token_owned)pns_complete(&native_scene);
 native_scene_token_owned=false;native_scene_token=0;
}
bool portable_native_scene_ready(uint64_t *generation,uint64_t *sequence) {
 if(generation)*generation=native_scene.generation;
 if(sequence)*sequence=native_scene.watermark;
 return native_scene.enabled&&native_scene.known&&native_scene.visible&&!native_scene.exhausted&&!failed&&
 !touch.terminal&&!touch.poll_failed&&!touch.resync&&!touch.cancel_pending&&
 !home_pending&&!crown_pending&&!handoff_requested
#ifdef PORTABLE_QUICK_ACTIONS
 &&!quick_modal&&!quick_restore_pending&&!pqa_visible(&quick.ui)&&!pqa_capture(&quick.ui)
#endif
#ifdef PORTABLE_ALARM_CLIENT
 &&!alarm_modal&&!native_sleep_retained
#endif
 ;
}
bool portable_native_scene_contact_read(portable_native_scene_contact *out) {
 if(!out)return false;
 *out=native_scene_contact;return native_scene.enabled&&!failed;
}
#ifdef PORTABLE_QUICK_ACTIONS
/* Preserve only an observed eligible original DOWN on the displayed map. A
 * rejected top-edge capture replays it before its saved current edge. No new
 * provider read or fabricated eligibility is permitted at replay time. */
static void native_scene_quick_capture(const portable_touch_sample *s) {
 if(!native_scene.enabled)return;
#if defined(PORTABLE_QUICK_NATIVE_GEOMETRY) && defined(PORTABLE_SAFE_VIEWPORT)
 portable_touch_sample local=*s;
 if((s->down||s->began||s->released)&&!portable_safe_viewport_touch(&content_view,&local.x,&local.y)){native_scene_quick_saved=false;return;}
 s=&local;
#endif
 if(!s->valid||s->cancelled){native_scene_quick_saved=false;return;}
 if(s->began) {
  native_scene_quick_saved=s->down&&s->tap_eligible&&portable_native_scene_ready(NULL,NULL)&&
   s->x<width()&&s->y<height()&&touch.sequence>native_scene.watermark;
  if(native_scene_quick_saved){native_scene_quick_down=*s;native_scene_quick_generation=native_scene.generation;native_scene_quick_sequence=touch.sequence;}
 }
}
static void native_scene_quick_replay(portable_touch_sample *s) {
 if(!quick_replay_pending)return;
 quick_replay_pending=false;
 bool valid=native_scene_quick_saved&&native_scene_quick_generation==native_scene.generation&&
  native_scene_quick_sequence>native_scene.watermark&&s->contact_id==native_scene_quick_down.contact_id&&
  s->valid&&!s->cancelled&&portable_native_scene_ready(NULL,NULL);
 native_scene_quick_saved=false;
 if(!valid){*s=(portable_touch_sample){.cancelled=true};return;}
 input_sample=*s;input_sample.began=false;input_pending=true;quick_replay_delivery=true;
 *s=native_scene_quick_down;
}
#endif
static void native_scene_input(portable_touch_sample *sample) {
 if(!native_scene.enabled)return;
#ifdef PORTABLE_QUICK_ACTIONS
 native_scene_quick_replay(sample);
#endif
 bool valid=sample->valid&&(!(sample->down||sample->began||sample->released)||
 (width()>0&&height()>0&&sample->x<width()&&sample->y<height()))&&(!sample->began||sample->tap_eligible);
 if(!pns_accept(&native_scene,portable_native_scene_ready(NULL,NULL),valid,sample->cancelled,
 sample->began,sample->down,sample->released,sample->contact_id,touch.sequence))
  *sample=(portable_touch_sample){.cancelled=true};
 native_scene_contact=(portable_native_scene_contact){sample->valid,sample->began,sample->down,sample->released,sample->cancelled,sample->x,sample->y};
}
void portable_native_scene_present(bool full) {
 if(failed||!native_scene.enabled||!native_scene.known)return;
 /* Existing frame readiness prevents more than one provider token. */
 if(paper_token){failed=true;return;}
 native_scene_submitting=true;present(full);native_scene_submitting=false;
}
