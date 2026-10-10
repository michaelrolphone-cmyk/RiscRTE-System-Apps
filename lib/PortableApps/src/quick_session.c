#include "PortableQuickSession.h"
#include "PortableTimeFormat.h"
#include "PortableContextPreferences.h"
void pqa_session_init(pqa_session *s) {
 *s=(pqa_session){0};pqa_init(&s->ui);pqa_cancel_input(&s->ui);
 s->idle_ms=PORTABLE_SLEEP_IDLE_MS;s->deep_ms=PORTABLE_SLEEP_LIGHT_MS;
 #if defined(PORTABLE_CONTEXTS_CLIENT) || defined(WATCH_CONTEXTS_CLIENT)
 s->ui.contexts_controls=true;
#endif
 s->brightness=PQA_BRIGHTNESS_DEFAULT;s->volume=s->restore_volume=PQA_VOLUME_DEFAULT;
}
static bool acquire(const risc_runtime_api_v1 *rt,risc_runtime_capability_v1 *g,const risc_key_value_v1 **kv) {
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};*kv=NULL;
 if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PQA_STORE_INSTANCE,g))*kv=g->api;
 return *kv && pqa_preferences_valid(*kv);
}
bool pqa_session_load(pqa_session *s,const risc_runtime_api_v1 *rt) {
 risc_runtime_capability_v1 g;const risc_key_value_v1 *kv;
 (void)acquire(rt,&g,&kv);unsigned b=s->brightness,v=s->volume,r=PQA_VOLUME_DEFAULT;
 bool bv=pqa_preference_load(kv,PQA_BRIGHTNESS_KEY,s->brightness,10,&b);
 bool vv=pqa_preference_load(kv,PQA_VOLUME_KEY,PQA_VOLUME_DEFAULT,0,&v);
 (void)pqa_preference_load(kv,PQA_RESTORE_VOLUME_KEY,PQA_VOLUME_DEFAULT,1,&r);
 bool dnd=false,dnd_valid=pqa_dnd_load(kv,&dnd);
 if(s->ui.contexts_controls){
  bool enabled=false;s->ui.contexts_valid=portable_context_enabled_load(kv,&enabled);
  s->ui.contexts_enabled=s->ui.contexts_valid&&enabled;
 }
 unsigned mode=PORTABLE_TIME_FORMAT_12;(void)portable_time_format_load(kv,&mode);s->hour_24=mode==PORTABLE_TIME_FORMAT_24;
#ifdef PORTABLE_LOW_BATTERY
 (void)portable_sleep_timer_load(kv,false,&s->idle_ms);
 (void)portable_sleep_timer_load(kv,true,&s->deep_ms);
#endif
 if(g.api && !rt->release(&g))return false;
 if(bv)s->brightness=b;
 if(vv)s->volume=v;
 if(dnd_valid)s->dnd_enabled=dnd;
 s->ui.dnd_valid=dnd_valid;s->ui.dnd_enabled=s->dnd_enabled;
 s->restore_volume=v?v:r;
 pqa_set_levels(&s->ui,bv,s->brightness,vv,s->volume);s->ui.last_nonzero_volume=(uint8_t)s->restore_volume;
 s->loaded=true;return true;
}
bool pqa_session_restore(const pqa_session *s,const risc_display_output_api_v1 *d) {
 return d && d->set_brightness && d->set_brightness(d->context,(uint16_t)s->brightness,100);
}
bool pqa_session_apply(pqa_session *s,const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *d,uint32_t actions,bool *alerts_changed) {
 *alerts_changed=false;
 if(actions&(PQA_BRIGHTNESS_COMMIT|PQA_VOLUME_COMMIT|PQA_SILENT|PQA_DND|PQA_CONTEXTS)) {
  risc_runtime_capability_v1 g;const risc_key_value_v1 *kv;(void)acquire(rt,&g,&kv);
  if(actions&PQA_BRIGHTNESS_COMMIT) {
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
  if((actions&PQA_CONTEXTS)&&s->ui.contexts_controls) {
   if(portable_context_enabled_save(kv,s->ui.action_contexts)){
    s->ui.contexts_enabled=s->ui.action_contexts;s->ui.contexts_valid=true;
    s->ui.error_flags&=~(PQA_ERROR_CONTEXTS|PQA_ERROR_SAVE);
   }else{s->ui.contexts_valid=false;s->ui.error_flags|=PQA_ERROR_CONTEXTS|PQA_ERROR_SAVE;}
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
