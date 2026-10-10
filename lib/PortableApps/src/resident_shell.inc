#ifdef PORTABLE_RESIDENT_SHELL_HOST
#include "RiscDisplayOutputSnapshotV1.h"
#include "PortableResidentShell.h"
static bool resident_registered,resident_child_active,resident_dispatching,resident_sleep_pending;
static const char *resident_launch_pending;
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
static int32_t resident_dispatch(void *context,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply) {
 (void)context;
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
 if(!resident_child_active || resident_dispatching || !request || request->struct_size<sizeof(*request) ||
    !reply || reply->struct_size<sizeof(*reply))return RISC_RESIDENT_INVALID;
 reply->flags=0;
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
 const risc_display_output_api_v1_snapshot *snapshot=risc_display_output_snapshot(display);
 if(!snapshot || !alarm_pixels || surface_format!=RISC_DISPLAY_FORMAT_MONO1)return RISC_RESIDENT_BUSY;
 /* A refused snapshot does not acquire focus, mutate the panel or start UI. */
 if(!snapshot->copy_completed(display->context,surface_format,alarm_pixels,quick_frame_bytes(),(uint32_t)quick_row_bytes()))return RISC_RESIDENT_BUSY;
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
 if(native_custody_retained)return -1;
 if(resident_child_active || resident_dispatching)return 0;
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
  resident_policy_requested=false;last_activity=millis_now();
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
 return result;
}
#endif
