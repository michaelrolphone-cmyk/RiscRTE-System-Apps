#include "PortableQuickSession.h"
#include "PortableTimeFormat.h"
#ifdef PORTABLE_FRONTLIGHT_TONE
#include "RiscDisplayOutputFrontlightV1.h"
#endif
void pqa_session_init(pqa_session *s) {
 *s=(pqa_session){0};pqa_init(&s->ui);pqa_cancel_input(&s->ui);
#ifdef PORTABLE_LOW_BATTERY
 s->idle_ms=PORTABLE_SLEEP_IDLE_MS;s->deep_ms=PORTABLE_SLEEP_LIGHT_MS;
#endif
 s->brightness=PQA_BRIGHTNESS_DEFAULT;s->volume=s->restore_volume=PQA_VOLUME_DEFAULT;
#ifdef PORTABLE_FRONTLIGHT_TONE
 s->tone=PQA_TONE_DEFAULT;s->ui.tone=(uint8_t)s->tone;
#endif
#ifdef PORTABLE_PAPER_TRANSITIONS
 s->restore_brightness=PQA_BRIGHTNESS_DEFAULT;
#endif
}
static bool acquire(const risc_runtime_api_v1 *rt,risc_runtime_capability_v1 *g,const risc_key_value_v1 **kv) {
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};*kv=NULL;
 if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PQA_STORE_INSTANCE,g))*kv=g->api;
 return *kv && pqa_preferences_valid(*kv);
}
bool pqa_session_load(pqa_session *s,const risc_runtime_api_v1 *rt) {
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(s->retained)return false;
#endif
 risc_runtime_capability_v1 g;const risc_key_value_v1 *kv;
 (void)acquire(rt,&g,&kv);unsigned b=s->brightness,v=s->volume,r=PQA_VOLUME_DEFAULT;
 unsigned minimum=10;
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(s->ui.paper)minimum=0;
#endif
 bool bv=pqa_preference_load(kv,PQA_BRIGHTNESS_KEY,s->brightness,minimum,&b);
#ifdef PORTABLE_PAPER_TRANSITIONS
 unsigned br=PQA_BRIGHTNESS_DEFAULT;
 if(s->ui.paper) {
  bool restored=pqa_preference_load(kv,PQA_RESTORE_BRIGHTNESS_KEY,PQA_BRIGHTNESS_DEFAULT,10,&br);
  if((b && b<10) || (!b && !restored))bv=false;
 }
#endif
 bool vv=pqa_preference_load(kv,PQA_VOLUME_KEY,PQA_VOLUME_DEFAULT,0,&v);
 (void)pqa_preference_load(kv,PQA_RESTORE_VOLUME_KEY,PQA_VOLUME_DEFAULT,1,&r);
 bool dnd=false,dnd_valid=pqa_dnd_load(kv,&dnd);
#ifdef PORTABLE_FRONTLIGHT_TONE
 unsigned tone=s->tone;
 bool tone_valid=pqa_preference_load(kv,PQA_TONE_KEY,PQA_TONE_DEFAULT,0,&tone);
#endif
 unsigned mode=PORTABLE_TIME_FORMAT_12;(void)portable_time_format_load(kv,&mode);s->hour_24=mode==PORTABLE_TIME_FORMAT_24;
#ifdef PORTABLE_LOW_BATTERY
 (void)portable_sleep_timer_load(kv,false,&s->idle_ms);
 (void)portable_sleep_timer_load(kv,true,&s->deep_ms);
#endif
 if(g.api && !rt->release(&g)) {
#ifdef PORTABLE_FRONTLIGHT_TONE
  s->retained=true;
#endif
  return false;
 }
 if(bv)s->brightness=b;
 if(vv)s->volume=v;
 if(dnd_valid)s->dnd_enabled=dnd;
 s->ui.dnd_valid=dnd_valid;s->ui.dnd_enabled=s->dnd_enabled;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(tone_valid)s->tone=tone;
 s->ui.tone=(uint8_t)s->tone;s->ui.tone_valid=tone_valid;
#endif
 s->restore_volume=v?v:r;
 pqa_set_levels(&s->ui,bv,s->brightness,vv,s->volume);s->ui.last_nonzero_volume=(uint8_t)s->restore_volume;
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(s->ui.paper) {
  s->restore_brightness=s->brightness?s->brightness:br;
  s->ui.last_nonzero_brightness=(uint8_t)s->restore_brightness;
 }
#endif
 s->loaded=true;return true;
}
bool pqa_session_restore(const pqa_session *s,const risc_display_output_api_v1 *d) {
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(s->retained)return false;
#endif
 return d && d->set_brightness && d->set_brightness(d->context,(uint16_t)s->brightness,100);
}
#ifdef PORTABLE_FRONTLIGHT_TONE
static bool tone_failed(pqa_session *s) {
 s->ui.applied_tone_valid=false;s->ui.error_flags|=PQA_ERROR_TONE;
 s->retained=true;return false;
}
/* The SDK helper bounds every suffix read. Unsupported/truncated prefixes are
 * absence; a present provider's read refusal or malformed ratio is terminal. */
static bool tone_probe(pqa_session *s,const risc_display_output_api_v1 *d,
                       const risc_display_output_api_v1_frontlight **tone) {
 *tone=risc_display_output_frontlight(d);
 if(!*tone) {
  s->ui.tone_controls=false;s->ui.applied_tone_valid=false;
  s->ui.error_flags&=~PQA_ERROR_TONE;return true;
 }
 s->ui.tone_controls=true;
 uint16_t warm=0,maximum=0;
 int32_t status=(*tone)->get_tone(d->context,&warm,&maximum);
 if(status==RISC_DISPLAY_TONE_UNAVAILABLE) {
  *tone=NULL;s->ui.tone_controls=false;s->ui.applied_tone_valid=false;
  s->ui.error_flags&=~PQA_ERROR_TONE;return true;
 }
 if(status!=RISC_DISPLAY_TONE_OK || !maximum || warm>maximum)return tone_failed(s);
 /* uint16 inputs fit comfortably in uint32 even with round-to-nearest. */
 s->ui.applied_tone=(uint8_t)(((uint32_t)warm*100u+maximum/2u)/maximum);
 s->ui.applied_tone_valid=true;s->ui.error_flags&=~PQA_ERROR_TONE;return true;
}
static bool tone_set(pqa_session *s,const risc_display_output_api_v1 *d,
                     const risc_display_output_api_v1_frontlight *tone,unsigned value) {
 if(value>100 || !tone->set_tone(d->context,(uint16_t)value,100))return tone_failed(s);
 s->ui.applied_tone=(uint8_t)value;s->ui.applied_tone_valid=true;
 s->ui.error_flags&=~PQA_ERROR_TONE;return true;
}
bool pqa_session_sync_tone(pqa_session *s,const risc_display_output_api_v1 *d) {
 if(s->retained)return false;
 if(!s->ui.paper)return true;
 const risc_display_output_api_v1_frontlight *tone;
 if(!tone_probe(s,d,&tone))return false;
 return !tone || !s->ui.tone_valid || tone_set(s,d,tone,s->tone);
}
static bool paper_tone_apply(pqa_session *s,const risc_runtime_api_v1 *rt,
                             const risc_display_output_api_v1 *d,uint32_t actions) {
 if(!s->ui.paper || !(actions&(PQA_TONE_PREVIEW|PQA_TONE_COMMIT)))return true;
 const risc_display_output_api_v1_frontlight *tone;
 if(!tone_probe(s,d,&tone))return false;
 if(!tone)return true;
 /* Corrupt/unreadable storage disables editing rather than inventing a saved
  * rollback target. The provider's observed value remains separately valid. */
 if(!s->ui.tone_valid) {
  s->ui.tone=(uint8_t)s->tone;s->ui.error_flags|=PQA_ERROR_SAVE;return true;
 }
 unsigned wanted=s->ui.action_tone;
 /* A refused set may leave uncertain output. Stop before storage or any other
  * provider operation; no rollback is safe once custody is terminal. */
 if(!tone_set(s,d,tone,wanted))return false;
 if(!(actions&PQA_TONE_COMMIT))return true;
 risc_runtime_capability_v1 g;const risc_key_value_v1 *kv;(void)acquire(rt,&g,&kv);
 bool saved=pqa_preference_save(kv,PQA_TONE_KEY,wanted,0);
 if(g.api && !rt->release(&g)) {
  s->ui.error_flags|=PQA_ERROR_SAVE;return tone_failed(s);
 }
 if(saved) {
  s->tone=wanted;s->ui.tone=(uint8_t)wanted;s->ui.tone_valid=true;
  s->ui.error_flags&=~PQA_ERROR_SAVE;return true;
 }
 s->ui.tone=(uint8_t)s->tone;s->ui.error_flags|=PQA_ERROR_SAVE;
 return tone_set(s,d,tone,s->tone);
}
#endif
#ifdef PORTABLE_PAPER_TRANSITIONS
static bool paper_brightness(pqa_session *s,const risc_display_output_api_v1 *d,unsigned value) {
 if(!d || !d->set_brightness || !d->set_brightness(d->context,(uint16_t)value,100)) {
  s->ui.applied_brightness_valid=false;s->ui.error_flags|=PQA_ERROR_BRIGHTNESS;return false;
 }
 s->ui.applied_brightness=(uint8_t)value;s->ui.applied_brightness_valid=true;
 s->ui.error_flags&=~PQA_ERROR_BRIGHTNESS;return true;
}
bool pqa_session_sync_brightness(pqa_session *s,const risc_display_output_api_v1 *d) {
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(!pqa_session_sync_tone(s,d))return false;
 bool ok=paper_brightness(s,d,s->brightness);
 if(!ok)s->retained=true;
 return ok;
#else
 return paper_brightness(s,d,s->brightness);
#endif
}
static bool paper_light_apply(pqa_session *s,const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *d,uint32_t actions) {
 if(!(actions&(PQA_BRIGHTNESS_PREVIEW|PQA_BRIGHTNESS_COMMIT|PQA_TORCH)))return true;
 if(actions&PQA_BRIGHTNESS_COMMIT) {
  unsigned wanted=s->ui.action_brightness,restore=s->brightness?s->brightness:s->restore_brightness;
  risc_runtime_capability_v1 g;const risc_key_value_v1 *kv;(void)acquire(rt,&g,&kv);
  /* Preserve a confirmed way back before committing OFF. A save failure
   * restores the previous hardware level; release failure retains custody. */
  bool ready=(wanted==0 || wanted>=10) &&
      (wanted!=0 || pqa_preference_save(kv,PQA_RESTORE_BRIGHTNESS_KEY,restore,10));
  bool saved=ready&&pqa_preference_save(kv,PQA_BRIGHTNESS_KEY,wanted,0);
  if(g.api&&!rt->release(&g))return false;
  if(saved) {
   s->brightness=wanted;s->restore_brightness=wanted?wanted:restore;
   s->ui.last_nonzero_brightness=(uint8_t)s->restore_brightness;
   s->ui.brightness_valid=true;s->ui.error_flags&=~PQA_ERROR_SAVE;
  } else {s->ui.brightness=(uint8_t)s->brightness;s->ui.error_flags|=PQA_ERROR_SAVE;}
 }
 unsigned value=s->brightness;
 if((actions&PQA_BRIGHTNESS_PREVIEW)&&!(actions&PQA_BRIGHTNESS_COMMIT))value=s->ui.action_brightness;
 if(s->ui.torch)value=100;
 return paper_brightness(s,d,value);
}
#endif
static bool session_apply(pqa_session *s,const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *d,uint32_t actions,bool *alerts_changed) {
 *alerts_changed=false;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(s->retained)return false;
 if(!paper_tone_apply(s,rt,d,actions))return false;
#endif
 uint32_t storage_actions=actions;
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(s->ui.paper)storage_actions&=~PQA_BRIGHTNESS_COMMIT;
#endif
 if(storage_actions&(PQA_BRIGHTNESS_COMMIT|PQA_VOLUME_COMMIT|PQA_SILENT|PQA_DND)) {
  risc_runtime_capability_v1 g;const risc_key_value_v1 *kv;(void)acquire(rt,&g,&kv);
  if(storage_actions&PQA_BRIGHTNESS_COMMIT) {
   unsigned wanted=s->ui.action_brightness;
   if(pqa_preference_save(kv,PQA_BRIGHTNESS_KEY,wanted,10)) {
    s->brightness=wanted;s->ui.brightness_valid=true;s->ui.error_flags&=~(PQA_ERROR_BRIGHTNESS|PQA_ERROR_SAVE);
   } else {s->ui.brightness=(uint8_t)s->brightness;s->ui.error_flags|=PQA_ERROR_SAVE;}
  }
  if(actions&(PQA_VOLUME_COMMIT|PQA_SILENT)) {
   unsigned wanted=s->ui.action_volume;
   /* Save the prior audible level first. If its write cannot be confirmed,
    * leave the actual volume untouched; silent must always have a way back. */
   bool ready=wanted!=0 || s->volume==0 || pqa_preference_save(kv,PQA_RESTORE_VOLUME_KEY,s->volume,1);
   if(ready && pqa_preference_save(kv,PQA_VOLUME_KEY,wanted,0)) {
    if(s->volume)s->restore_volume=s->volume;
    s->volume=wanted;if(wanted)s->restore_volume=wanted;
    s->ui.volume_valid=true;s->ui.error_flags&=~(PQA_ERROR_VOLUME|PQA_ERROR_SAVE);*alerts_changed=true;
   } else {s->ui.volume=(uint8_t)s->volume;s->ui.last_nonzero_volume=(uint8_t)s->restore_volume;s->ui.error_flags|=PQA_ERROR_SAVE;}
  }
  if(actions&PQA_DND) {
   bool wanted=s->ui.action_dnd;
   if(pqa_dnd_save(kv,wanted)) {
    *alerts_changed|=s->dnd_enabled!=wanted;
    s->dnd_enabled=wanted;s->ui.dnd_enabled=wanted;s->ui.dnd_valid=true;
    s->ui.error_flags&=~(PQA_ERROR_DND|PQA_ERROR_SAVE);
   } else {
    s->ui.dnd_enabled=s->dnd_enabled;s->ui.dnd_valid=false;
    s->ui.error_flags|=PQA_ERROR_DND|PQA_ERROR_SAVE;
   }
  }
  if(g.api && !rt->release(&g))return false;
 }
#ifdef PORTABLE_PAPER_TRANSITIONS
 if(s->ui.paper)return paper_light_apply(s,rt,d,actions);
#endif
 unsigned brightness=s->brightness;
 if((actions&PQA_BRIGHTNESS_PREVIEW) && !(actions&PQA_BRIGHTNESS_COMMIT))brightness=s->ui.action_brightness;
 if(s->ui.torch)brightness=100;
 if(actions&(PQA_BRIGHTNESS_PREVIEW|PQA_BRIGHTNESS_COMMIT|PQA_TORCH)) {
  if(!d || !d->set_brightness || !d->set_brightness(d->context,(uint16_t)brightness,100)) {
   s->ui.error_flags|=PQA_ERROR_BRIGHTNESS;return false;
  }
 }
 return true;
}
bool pqa_session_apply(pqa_session *s,const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *d,uint32_t actions,bool *alerts_changed) {
 bool ok=session_apply(s,rt,d,actions,alerts_changed);
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(!ok)s->retained=true;
#endif
 return ok;
}
