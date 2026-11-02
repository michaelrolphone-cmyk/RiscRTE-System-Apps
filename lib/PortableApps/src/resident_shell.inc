#ifdef PORTABLE_RESIDENT_SHELL_HOST
#include "RiscDisplayOutputSnapshotV1.h"
#include "PortableResidentShell.h"
static bool resident_registered,resident_child_active,resident_dispatching,resident_sleep_pending;
static const char *resident_launch_pending;
static bool resident_launch_busy;
bool portable_resident_launch_busy(void){return resident_launch_busy;}
static bool resident_queue_launch(const char *path) {
 if(native_custody_retained || !path || resident_launch_pending)return false;
 if(resident_dispatching) {
  if(resident.request_foreground_exit(resident.invocation)!=RISC_RESIDENT_OK)return false;
 }
 resident_launch_pending=path;return true;
}
const char *portable_resident_take_launch(void) {
 if(native_custody_retained || resident_child_active)return NULL;
 const char *path=resident_launch_pending;resident_launch_pending=NULL;return path;
}
bool portable_resident_take_sleep(void) {
 if(native_custody_retained || resident_child_active)return false;
 bool pending=resident_sleep_pending;resident_sleep_pending=false;return pending;
}
static bool resident_refresh_preferences(void) {
 if(native_custody_retained)return false;
#ifdef PORTABLE_PAPER_PREFERENCES
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_READER_STORE_INSTANCE,&grant))return !native_custody_retained;
 unsigned value=0;int status=portable_reader_preference_load(grant.api,false,&value);
 if(native_custody_retained)return false;
 if(!rt->release(&grant)){portable_adapter_retain();return false;}
 if(status==PORTABLE_READER_LOADED && value<=1)paper_flip_ui=value!=0;
#endif
 return !native_custody_retained;
}
static bool resident_touch_close(void) {
 if(!portable_touch_close(&touch,rt)){portable_adapter_retain();return false;}
 resident_neutral();return true;
}
static bool resident_touch_open(void) {
 if(!portable_touch_open(&touch,rt)){portable_adapter_retain();return false;}
 return true;
}
#include "resident_failure.inc"
#include "resident_policy.inc"
#ifdef PORTABLE_RESIDENT_LOADING
#include "resident_loading.inc"
#endif
#ifdef PORTABLE_SHARED_HOME_CANVAS
#include "PortableHomeContextReadiness.h"
extern unsigned portable_home_sleep_modes(void);
static bool shared_home_contexts_safe(void *unused) {
 (void)unused;return !native_custody_retained&&!failed&&!quick.retained;
}
static int shared_home_contexts_query(uint32_t reason) {
 const alarm_service_v1 *alarm=alarm_sleep_api();
 unsigned output=alarm_output_modes();
 portable_home_readiness state={.power_modes=portable_home_sleep_modes(),.sleep_settings=quick.loaded,
  .brightness=shared_outputs&&shared_outputs->set_brightness&&quick.ui.brightness_valid,
  .volume=quick.ui.audio_controls&&quick.ui.volume_valid,.dnd=quick.ui.dnd_valid&&alarm!=NULL,
  .sound=!!(output&ALARM_MODE_SOUND),.vibrate=!!(output&ALARM_MODE_VIBRATE)};
#if defined(PORTABLE_QUICK_RADIO_AVAILABILITY) || defined(PORTABLE_QUICK_CAPABILITY_SAFE)
 state.wifi=quick.ui.wifi_available&&quick.ui.wifi_valid;
 state.bluetooth=quick.ui.bluetooth_available&&quick.ui.bluetooth_valid;
#endif
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!portable_home_settings_available())state=(portable_home_readiness){0};
#endif
 const alarm_service_descriptor_v2*descriptor=alarm_service_descriptor(alarm);
 if(!descriptor)state.power_modes=0;
 else if(!(descriptor->features&ALARM_DESCRIPTOR_RESUME_SLEEP))state.power_modes&=~(PORTABLE_HOME_MODE_LIGHT|PORTABLE_HOME_MODE_HYBRID);
 /* FACE addresses watch_face, not the independent desk_clock_face key.
  * This overview controller has no active canonical native-face owner. */
 return portable_home_readiness_query(&state,reason);
}
#endif
static int32_t resident_dispatch(void *context,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply) {
 (void)context;
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
 if(quick_atomic_owned()||home_automatic_input_busy())return RISC_RESIDENT_BUSY;
#endif
 if(!resident_child_active || resident_dispatching || !request || request->struct_size<sizeof(*request) ||
    !reply || reply->struct_size<sizeof(*reply))return RISC_RESIDENT_INVALID;
 reply->flags=0;
#ifdef PORTABLE_SHARED_HOME_CANVAS
 if(request->reason>=RISC_CONTEXTS_SETTINGS_PREPARE&&request->reason<UINT32_C(0x43530300)&&
    (request->flags||request->reserved))return RISC_RESIDENT_INVALID;
 if(request->reason==RISC_CONTEXTS_SETTINGS_PREPARE){
  if(failed||surface.frame||paper_token||!display_settled||alarm_modal||quick_modal)return RISC_RESIDENT_BUSY;
  /* Admission is explicit and separate from the pure field queries below.
   * Do not call radio policy loaders here: those may toggle hardware. */
  if(portable_home_sleep_admit(rt)!=1)return native_custody_retained?RISC_RESIDENT_RETAINED:RISC_RESIDENT_BUSY;
  if(!pqa_session_load(&quick,rt)||!shared_home_contexts_safe(NULL))return RISC_RESIDENT_RETAINED;
#ifdef PORTABLE_QUICK_RADIOS
  if(!pqa_radios_inspect(&quick_radios,&quick.ui,rt,shared_home_contexts_safe,NULL)){
   if(!native_custody_retained)portable_adapter_retain_silent();
   return RISC_RESIDENT_RETAINED;
  }
#endif
  quick_paper_capabilities();
#ifdef PORTABLE_SETTINGS_AVAILABILITY
  if(!portable_home_settings_poll())return RISC_RESIDENT_RETAINED;
  home_settings_readonly_controls();
#endif
  return shared_home_contexts_safe(NULL)?RISC_RESIDENT_OK:RISC_RESIDENT_RETAINED;
 }
 if(request->reason>=UINT32_C(0x43530100)&&request->reason<UINT32_C(0x43530300)){
  int ready=shared_home_contexts_query(request->reason);
  return ready<0?RISC_RESIDENT_INVALID:ready?RISC_RESIDENT_OK:RISC_RESIDENT_BUSY;
 }
#endif
#ifdef PORTABLE_RESIDENT_POLICY
 if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL || request->reason==RISC_RESIDENT_CHECKPOINT_POLICY)
  return resident_policy_dispatch(request,reply);
#else
 if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL)return RISC_RESIDENT_OK;
#endif
 if(request->reason==RISC_RESIDENT_CHECKPOINT_SLEEP || request->reason==RISC_RESIDENT_CHECKPOINT_EXIT ||
    request->reason==RISC_RESIDENT_CHECKPOINT_USB) {
  int status=resident.request_foreground_exit(resident.invocation);
  if(status!=RISC_RESIDENT_OK)return status;
  if(request->reason==RISC_RESIDENT_CHECKPOINT_SLEEP)resident_sleep_pending=true;
  if(request->reason==RISC_RESIDENT_CHECKPOINT_USB)resident_launch_pending="usb_sd_transfer.elf";
  return RISC_RESIDENT_EXIT;
 }
 if(request->reason!=RISC_RESIDENT_CHECKPOINT_CONTROLS)return RISC_RESIDENT_BUSY;
 if(failed || surface.frame || paper_token || !display_settled || alarm_modal || quick_modal)return RISC_RESIDENT_BUSY;
#ifdef PORTABLE_SHARED_HOME_CANVAS
 if(!alarm_pixels||surface_format!=RISC_DISPLAY_FORMAT_MONO1)return RISC_RESIDENT_BUSY;
 if(!home_canvas_copy_completed(&shared_canvas,alarm_pixels,quick_frame_bytes(),(uint32_t)quick_row_bytes()))
  return native_custody_retained?RISC_RESIDENT_RETAINED:RISC_RESIDENT_BUSY;
#else
 const risc_display_output_api_v1_snapshot *snapshot=risc_display_output_snapshot(display);
 if(!snapshot || !alarm_pixels || surface_format!=RISC_DISPLAY_FORMAT_MONO1)return RISC_RESIDENT_BUSY;
 /* A refused snapshot does not acquire focus, mutate the panel or start UI. */
 if(!snapshot->copy_completed(display->context,surface_format,alarm_pixels,quick_frame_bytes(),(uint32_t)quick_row_bytes()))return RISC_RESIDENT_BUSY;
#endif
 if(!resident_refresh_preferences())return RISC_RESIDENT_RETAINED;
 alarm_pixels_valid=true;paper_previous_valid=previous_valid=false;
 resident_dispatching=true;
 if(!resident_touch_open() || !resident_focus(true))return RISC_RESIDENT_RETAINED;
 pqa_cancel(&quick.ui);(void)pqa_take_action(&quick.ui);quick.ui.neutral_gate=false;
 portable_touch_sample sample;portable_touch_read(&touch,&sample);
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
#ifdef PORTABLE_PAPER_PREFERENCES
 paper_orient_input(&sample);
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 desk_touch_neutral=true;
#endif
 if(sample.valid && sample.down && !sample.cancelled) {
  int x=(int)sample.x*480/width(),y=(int)sample.y*800/height();
  if(!portable_touch_adopt_drag(&touch)){resident_terminal();return RISC_RESIDENT_RETAINED;}
  /* The foreground only asks after a deliberate top pull. Adopt that held
   * contact as a drag; its inherited down can never activate a control. */
  pqa_input(&quick.ui,millis_now(),true,1,touch.contact_id,x,0,true);
  pqa_input(&quick.ui,millis_now(),true,1,touch.contact_id,x,y,true);
 } else {
  /* A fast released pull still opens the same host sheet. */
  quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.clock_valid=false;
 }
 bool consumed=false;
 bool ok=quick_foreground(&consumed);
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
 if(!ok || !portable_paper_frame_drain() || surface.frame || !display_settled) {
  portable_adapter_retain();return RISC_RESIDENT_RETAINED;
 }
 /* Physical Home belongs to the running child even while its host drawer
  * owns input. Ask for a clean child return before neutralizing that input. */
 bool return_home=home_pending||crown_pending;
 int home_status=RISC_RESIDENT_OK;
 if(return_home) {
  home_status=resident.request_foreground_exit(resident.invocation);
  if(home_status==RISC_RESIDENT_RETAINED){resident_terminal();return home_status;}
  if(native_custody_retained)return RISC_RESIDENT_RETAINED;
 }
 if(!resident_focus(false) || !resident_touch_close())return RISC_RESIDENT_RETAINED;
 resident_dispatching=false;
 if(home_status!=RISC_RESIDENT_OK)return home_status;
 reply->flags=RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
 return return_home || resident_launch_pending || resident_sleep_pending?RISC_RESIDENT_EXIT:RISC_RESIDENT_OK;
}
static int resident_launch_refused(void) {
 if(native_custody_retained)return -1;
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
 contexts_clock_owner_returned();
#endif
 return 0;
}
int portable_resident_run_foreground(const char *path) {
 resident_launch_busy=false;
 if(native_custody_retained)return -1;
 if(resident_child_active || resident_dispatching)return 0;
#ifdef PORTABLE_INSTALLED_CATALOG_HOST
 int catalog_close=portable_installed_prepare_launch();
 if(catalog_close!=INSTALLED_APPS_OK){
  if(catalog_close==INSTALLED_APPS_RETAINED)return -1;
  if(path&&!resident_launch_pending)resident_launch_pending=path;
  return 0;
 }
#endif
 if(
#ifndef PORTABLE_RESIDENT_LEGACY_HANDOFF
    !path ||
#endif
    !resident_bind())return resident_launch_refused();
 if(!resident_registered) {
  const risc_resident_callbacks_v1 callbacks={.api_version=RISC_RESIDENT_SHELL_API_V1,
   .struct_size=sizeof(callbacks),.context=NULL,.dispatch=resident_dispatch,.failed=NULL,
#ifdef PORTABLE_RESIDENT_LOADING
   .loading=resident_loading,
#endif
  };
  int status=resident.register_shell(resident.invocation,&callbacks);
  if(status==RISC_RESIDENT_RETAINED){resident_terminal();return -1;}
  if(native_custody_retained)return -1;
  if(status!=RISC_RESIDENT_OK)return resident_launch_refused();
  resident_registered=true;
 }
 if(!portable_paper_frame_drain() || surface.frame || !display_settled || alarm_modal || quick_modal)return resident_launch_refused();
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if(!portable_background_stop())return -1;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_suspend())return -1;
#endif
 if(!resident_prior_failure())return -1;
 if(!resident_focus(false) || !resident_touch_close())return -1;
 pqa_cancel(&quick.ui);(void)pqa_take_action(&quick.ui);
 int result=1;
 risc_resident_failure_v1 launch_failure={0};
 do {
  resident_launch_pending=NULL;resident_child_active=true;
#ifdef PORTABLE_RESIDENT_POLICY
  resident_policy_requested=resident_policy_input_continuity=false;last_activity=millis_now();
  quick_launch_pending=handoff_requested=false;
  if(native_custody_retained)return -1;
#endif
  risc_resident_result_v1 outcome={.struct_size=sizeof(outcome)};
  int status=resident.run_foreground(resident.invocation,path,&outcome);
  /* Runtime retains both invocations on uncertain child cleanup. Absolutely
   * no release, focus, touch, UI or provider calls follow that outcome. */
  if(status==RISC_RESIDENT_RETAINED || outcome.status==RISC_RESIDENT_RETAINED) {
   resident_terminal();return -1;
  }
  if(native_custody_retained)return -1;
  resident_child_active=false;
  if(failed)return -1;
#ifdef PORTABLE_RESIDENT_LEGACY_HANDOFF
  if(status==RISC_RESIDENT_HANDOFF) {
   /* Runtime has queued a legacy invocation. Its launch requires this host's
    * clean return, so do not reopen touch, focus, preferences or Home. */
   resident_launch_pending=NULL;resident_sleep_pending=false;
   return PORTABLE_RESIDENT_HANDOFF;
  }
  if(!path && status==RISC_RESIDENT_NO_PENDING && !outcome.invocation) {
   result=PORTABLE_RESIDENT_NO_PENDING;break;
  }
#endif
  if(status!=RISC_RESIDENT_OK){
   /* Refusal can carry no current failure. Never label a denied request with
    * an earlier invocation's app or the unrelated previous-reset record. */
   if(outcome.failure.status==status && outcome.failure.invocation==outcome.invocation)
    (void)resident_failure_copy(&launch_failure,&outcome.failure);
   resident_launch_busy=status==RISC_RESIDENT_BUSY;
   result=0;resident_launch_pending=NULL;resident_sleep_pending=false;break;
  }
  path=resident_launch_pending;
 } while(path);
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
 contexts_clock_owner_returned();
#endif
 if(!resident_refresh_preferences() || !resident_touch_open() || !resident_focus(true))return -1;
 resident_neutral();paper_previous_valid=previous_valid=alarm_pixels_valid=false;
 quick_launch_pending=handoff_requested=false;
 if(launch_failure.kind && !resident_failure_show(&launch_failure))return -1;
#ifdef PORTABLE_APP_SLEEP_LOCAL
 last_activity=millis_now();
#endif
#ifdef PORTABLE_INSTALLED_CATALOG_HOST
 portable_installed_resume_discovery();
#endif
 return result;
}
#endif
