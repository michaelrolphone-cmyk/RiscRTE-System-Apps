#include "PortableContextActions.h"
#include "PortableRtcClock.h"
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
#include "PortableNativeTimeToolbar.h"
#else
#include "PortableTime.h"
#endif
static uint32_t context_action_checked;
static char context_message[33];
static bool context_message_open,context_launch_pending;
static bool contexts_rules_tick(void){
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(contexts_client.api);if(!fp||!contexts_client.rules_available)return true;
 uint32_t now=millis_now();if(context_action_checked&&(uint32_t)(now-context_action_checked)<500u)return true;context_action_checked=now;
 contexts_evaluate_v1 q={.struct_size=sizeof(q)};q.observation.enabled=contexts_client.policy.enabled&&contexts_client.settings_valid;
 risc_battery_sample_v1 battery={0};if(gauge&&gauge->read(gauge->context,&battery)&&battery.percent<=100&&!(battery.flags&RISC_BATTERY_PROFILE_MISSING)){q.observation.battery_valid=true;q.observation.battery=battery.percent;q.observation.charging=!!(battery.flags&RISC_BATTERY_CHARGING);}
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 twatch_rtc_time_v1 local={0};if(portable_app_native_local_time(&local)){q.observation.time_valid=true;q.observation.hour=local.hour;}
#else
 risc_runtime_capability_v1 clock={.struct_size=sizeof(clock)};
 if(rt->acquire("rtc.clock",2,0,&clock)){
  const twatch_rtc_api_v1*rtc=clock.api;twatch_rtc_time_v1 t={0};if(rtc&&rtc->api_version==2&&rtc->struct_size>=sizeof(*rtc)&&rtc->read&&rtc->read(rtc->context,&t)&&t.hour<24){twatch_rtc_time_v1 local={0};if(portable_time_forward(&t,&local)){q.observation.time_valid=true;q.observation.hour=local.hour;}}
  if(!rt->release(&clock))return false;
 }
#endif
 if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_EVALUATE,&q))return false;
 if(q.result.changed){
  if(!portable_contexts_pause(&contexts_client))return false;
  contexts_client.storage=(risc_runtime_capability_v1){.struct_size=sizeof(contexts_client.storage)};
  bool opened=rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&contexts_client.storage),ok=opened;
  uint16_t success=0;
  for(unsigned f=0;f<CR_SETTINGS;f++)if(q.result.changed&(1u<<f)){
   bool applied=opened&&portable_context_setting(contexts_client.storage.api,f,q.result.settings[f],contexts_client.low_battery,contexts_face_save);if(applied)success|=1u<<f;else ok=false;
  }
  if(!portable_contexts_release_storage(&contexts_client))return false;
  for(unsigned i=0;i<CR_RULES;i++){bool changed=false,complete=true;for(unsigned f=0;f<CR_SETTINGS;f++)if(q.result.winners[f]==(int)i&&(q.result.changed&(1u<<f))){changed=true;complete&=(success&(1u<<f))!=0;}if(changed){contexts_workflow_v1 report={.struct_size=sizeof(report),.rule=i,.result=complete?CR_APPLIED:CR_PARTIAL};if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_WORKFLOW,&report))return false;}}
#ifdef PORTABLE_QUICK_ACTIONS
  if(opened&&(!pqa_session_load(&quick,rt)||!pqa_session_restore(&quick,display)))return false;
#ifdef PORTABLE_QUICK_RADIOS
  if(opened&&!pqa_radios_resume(&quick_radios,&quick.ui,rt))return false;
#endif
#endif
  (void)ok;const alarm_service_v1*a=alarm_sleep_api();if(a)(void)a->refresh(a->context);
 }
 if(context_message_open||context_launch_pending)return true;
 contexts_workflow_v1 action={.struct_size=sizeof(action),.claim=true};
 if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_WORKFLOW,&action))return false;
 if(action.available){
  if(!portable_contexts_pause(&contexts_client))return false;
  bool ok=false;
  if(action.step.kind==CR_MESSAGE){memcpy(context_message,action.step.text,sizeof(context_message));context_message_open=true;ok=true;}
  else if(action.step.kind==CR_TIMER){ok=portable_context_timer(rt,alarm_sleep_api(),action.step.value,&contexts_client.storage);if(contexts_client.storage.api)return false;}
  else if(action.step.kind==CR_LAUNCH){
   bool installed=false;for(unsigned i=0;i<portable_catalog_count;i++)if(portable_catalog[i].compatible&&!strcmp(portable_catalog[i].file_name,action.step.text))installed=true;
   if(installed&&app_allows_launch(action.step.text)){
#ifdef PORTABLE_RESIDENT_SHELL_HOST
    static char context_launch_path[33];memcpy(context_launch_path,action.step.text,33);ok=resident_queue_launch(context_launch_path);
#else
    ok=rt->request_launch(action.step.text);if(ok)context_launch_pending=true;
#endif
}
  }
  action.claim=false;action.result=ok?CR_RAN:CR_FAILED;if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_WORKFLOW,&action))return false;
 }
 return true;
}

/* Own the display and input until dismissal, just as a foreground alarm does.
 * No application receives the dismiss tap as an unrelated app action. */
static bool contexts_message_foreground(void){
 if(!context_message_open)return true;
 if(!portable_contexts_pause(&contexts_client))return false;
 bool painted=false;
 while(context_message_open&&!failed){
  if(!paper_present_progress())return false;
  if(display_settled&&!surface.frame&&!painted){clear();label(8,height()/3,width()-16,"CONTEXT MESSAGE");label(8,height()/2,width()-16,context_message);label(8,height()-45,width()-16,"TAP TO DISMISS");present(true);painted=true;}
  bool consumed=false;if(!alarm_foreground(&consumed))return false;if(consumed){painted=false;continue;}
  rt->yield_ms(8);input_service();
  t5_app_input_t nav={0};input_navigation_take(&nav);portable_touch_sample touch_event;input_take(&touch_event);
  if(display_settled&&(nav.buttons&T5_APP_BUTTON_BACK||touch_event.released)){context_message_open=false;}
 }
 return !failed;
}
