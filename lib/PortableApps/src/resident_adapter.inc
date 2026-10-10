/* Explicit resident lifecycle, never entered from Runtime yield. */
#if defined(PORTABLE_RESIDENT_SHELL_HOST) || defined(PORTABLE_RESIDENT_SHELL_CLIENT)
static risc_resident_client_v1 resident;
static bool resident_ready;
static void resident_terminal(void) {
 /* Runtime already sealed both invocations; this is local memory only. */
 native_custody_retained=native_sleep_retained=failed=true;
 native_custody_surface=surface;surface=(risc_display_surface_v1){0};gauge=NULL;
}
static bool resident_bind(void) {
 if(resident_ready)return !native_custody_retained;
 if(native_custody_retained || !rt || rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE || !rt->resident_shell)return false;
 risc_resident_client_v1 next={.struct_size=sizeof(next)};
 if(!rt->resident_shell(&next) || next.api_version!=RISC_RESIDENT_SHELL_API_V1 ||
    next.struct_size<sizeof(next) || !next.invocation ||
#ifdef PORTABLE_RESIDENT_SHELL_HOST
    next.role!=RISC_RESIDENT_ROLE_HOST || !next.register_shell || !next.run_foreground || !next.request_foreground_exit
#else
    next.role!=RISC_RESIDENT_ROLE_FOREGROUND || !next.checkpoint
#endif
 )return false;
 resident=next;resident_ready=true;return true;
}
static void resident_neutral(void) {
 input_pending=false;input_sample=(portable_touch_sample){0};navigation_pending=0;
 home_pending=crown_pending=false;clear_contact_snapshots();
 input_reset();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
 desk_touch_neutral=false;
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
 nu_gesture=false;
#endif
}
static bool resident_focus(bool active) {
 if(native_custody_retained)return false;
#ifdef PORTABLE_INPUT_NAVIGATION
 if(navigation_ready) {
  const risc_input_foreground_v1 claims[]={{"input.touch.raw",1}};
  if(!navigation->foreground(navigation->context,active?claims:NULL,active?1:0)) {
   portable_adapter_retain();return false;
  }
  if(!navigation->reset(navigation->context)) {portable_adapter_retain();return false;}
  navigation_neutral=false;
 }
#else
 (void)active;
#endif
 return true;
}
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
/* Gesture arbitration only: no shared controls, fonts, assets or renderer are
 * linked into a converted foreground. Its model/stack remain live at dispatch. */
static bool resident_top_pending,resident_controls_requested,resident_neutral_gate;
static int resident_top_x,resident_top_y;
static void resident_input(portable_touch_sample *sample) {
 if(resident_neutral_gate) {
  if(sample->valid && !sample->down && !touch.home_down)resident_neutral_gate=false;
  *sample=(portable_touch_sample){0};return;
 }
 if(sample->cancelled || !sample->valid){resident_top_pending=false;return;}
 if(sample->began) {
  resident_top_x=sample->x;resident_top_y=sample->y;
  resident_top_pending=sample->tap_eligible && sample->y<height()*72/800;
 }
 if(resident_top_pending && sample->down) {
  int dx=(int)sample->x-resident_top_x,dy=(int)sample->y-resident_top_y;
  if(abs(dx)>width()/12 || dy<0)resident_top_pending=false;
  else if(dy>=height()*24/800 && dy>abs(dx)*2) {
   resident_controls_requested=true;resident_top_pending=false;
   *sample=(portable_touch_sample){0};
  }
 }
 if(sample->released)resident_top_pending=false;
}
#ifdef PORTABLE_RESIDENT_POLICY
static bool resident_policy_pending;
static bool resident_capture_active(void) {
#ifdef PORTABLE_CONTEXTS_CLIENT
 /* The service owns capture. Its admitted policy is a conservative local
  * bound; do not cross Runtime's exit barrier while either source may run. */
 if(contexts_client.loaded&&contexts_client.policy.enabled&&
    (contexts_client.policy.audio_allowed||contexts_client.policy.radio_allowed))return true;
#endif
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
 if(portable_audio_capture_active())return true;
#endif
#ifdef PORTABLE_RADIO_CONTINUOUS_CAPTURE
 if(portable_radio_capture_active())return true;
#endif
 return false;
}
static uint32_t resident_policy_flags(void) {
 uint32_t flags=resident_activity_pending?RISC_RESIDENT_POLL_ACTIVITY:0;
 bool blocked=resident_capture_active();
#ifdef PORTABLE_USB_TRANSFER_APP
 blocked=true; /* This dedicated media owner stays awake until clean exit. */
#endif
#if defined(PORTABLE_WIFI_SESSION_APP) || defined(PORTABLE_UPDATE_APP)
 blocked|=!portable_wifi_idle_ready();
#endif
 if(blocked)flags|=RISC_RESIDENT_POLL_INHIBIT_POLICY|RISC_RESIDENT_POLL_INHIBIT_IDLE;
 if(touch.down || touch.home_down || home_pending || crown_pending || navigation_pending ||
    resident_top_pending || resident_controls_requested || handoff_requested ||
    (input_pending && (input_sample.down || input_sample.released || input_sample.cancelled)))
  flags|=RISC_RESIDENT_POLL_INHIBIT_IDLE;
 return flags;
}
static bool resident_policy_ready(void) {
 return !failed && !native_custody_retained &&
   !(resident_policy_flags()&(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY));
}
#endif
static int resident_checkpoint(uint32_t reason) {
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
#ifdef PORTABLE_RESIDENT_POLICY
 if(reason==RISC_RESIDENT_CHECKPOINT_POLL && resident_capture_active()) {
  /* Native capture ownership fails Runtime's exit barrier. Keep this local
   * and rearm inactivity only after the owner reaches a clean checkpoint. */
  resident_activity_pending=true;return RISC_RESIDENT_BUSY;
 }
#ifdef PORTABLE_USB_TRANSFER_APP
 if(reason==RISC_RESIDENT_CHECKPOINT_POLL){resident_activity_pending=true;return RISC_RESIDENT_BUSY;}
#endif
#endif
 if(!resident_bind())return RISC_RESIDENT_INCOMPATIBLE;
 if(surface.frame || paper_token || !display_settled || alarm_modal)return RISC_RESIDENT_BUSY;
#ifdef PORTABLE_FILE_BROWSER_APP
 if(!portable_file_browser_safe())return RISC_RESIDENT_BUSY;
#endif
#if defined(PORTABLE_WIFI_SESSION_APP) || defined(PORTABLE_UPDATE_APP)
 if(!portable_wifi_services_safe())return native_custody_retained?RISC_RESIDENT_RETAINED:RISC_RESIDENT_BUSY;
#endif
#ifdef PORTABLE_RADIO_SESSION
 if(!portable_radio_services_safe())return RISC_RESIDENT_BUSY;
#endif
#ifdef PORTABLE_AUDIO_SESSION
 if(!portable_audio_services_safe())return RISC_RESIDENT_BUSY;
#endif
#ifdef PORTABLE_RESIDENT_POLICY
 bool policy=reason==RISC_RESIDENT_CHECKPOINT_POLICY;
 if(policy && (!resident_policy_pending || !resident_policy_ready()))return RISC_RESIDENT_BUSY;
#endif
 bool modal=reason!=RISC_RESIDENT_CHECKPOINT_POLL;
 if(modal) {
  /* The frozen host request has no cancelable exit or child edit-state field.
   * Resolve a guarded edit/media owner before exposing navigation controls.
   * Refusal stays in the existing child UI and starts no host presentation. */
#ifdef PORTABLE_RESIDENT_POLICY
  if(!policy)
#endif
  if(!app_allows_launch("default.elf"))return RISC_RESIDENT_BUSY;
#if defined(PORTABLE_RESIDENT_POLICY) && defined(PORTABLE_FILE_BROWSER_APP) && !defined(PORTABLE_FILE_SHARING)
  if(policy && !portable_file_browser_close())return native_custody_retained?RISC_RESIDENT_RETAINED:RISC_RESIDENT_BUSY;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!contexts_suspend())return RISC_RESIDENT_RETAINED;
#endif
#ifdef PORTABLE_BLE_BROADCAST
  if(!portable_broadcast_stop())return RISC_RESIDENT_RETAINED;
#endif
#ifdef PORTABLE_RADIO_SESSION
  if(!portable_radio_suspend()){portable_adapter_retain();return RISC_RESIDENT_RETAINED;}
#endif
#ifdef PORTABLE_AUDIO_SESSION
  if(!portable_audio_suspend()){portable_adapter_retain();return RISC_RESIDENT_RETAINED;}
#endif
#if defined(PORTABLE_WIFI_SESSION_APP) || defined(PORTABLE_UPDATE_APP)
  if(!portable_wifi_suspend()){
#ifdef PORTABLE_WIFI_SESSION_APP
    if(portable_wifi_stop_pending())return RISC_RESIDENT_BUSY;
#endif
    portable_adapter_retain();return RISC_RESIDENT_RETAINED;
  }
#endif
  if(!resident_focus(false))return RISC_RESIDENT_RETAINED;
#ifdef PORTABLE_RESIDENT_POLICY
  if(policy && !portable_touch_close(&touch,rt)){portable_adapter_retain();return RISC_RESIDENT_RETAINED;}
#endif
 }
 risc_resident_request_v1 request={sizeof(request),reason,0,0};
#ifdef PORTABLE_RESIDENT_POLICY
 if(reason==RISC_RESIDENT_CHECKPOINT_POLL)request.flags=resident_policy_flags();
#endif
 risc_resident_reply_v1 reply={sizeof(reply),0};
 int status=resident.checkpoint(resident.invocation,&request,&reply);
 if(status==RISC_RESIDENT_RETAINED) {resident_terminal();return status;}
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
#ifdef PORTABLE_RESIDENT_POLICY
 if(reason==RISC_RESIDENT_CHECKPOINT_POLL && status==RISC_RESIDENT_OK) {
  resident_activity_pending=false;
  if(request.flags&RISC_RESIDENT_POLL_INHIBIT_POLICY)resident_policy_pending=false;
  else if(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST)resident_policy_pending=true;
 }
 if(policy && status==RISC_RESIDENT_OK)resident_policy_pending=false;
 if(status==RISC_RESIDENT_EXIT)resident_policy_pending=false;
#endif
 if(modal) {
#ifdef PORTABLE_RESIDENT_POLICY
  if(policy && status!=RISC_RESIDENT_EXIT && !portable_touch_open(&touch,rt)) {
   portable_adapter_retain();return RISC_RESIDENT_RETAINED;
  }
#endif
  resident_neutral();resident_neutral_gate=true;
  /* A clean EXIT unwinds through ordinary app/fini cleanup, without taking
   * focus back from the host. No image or model is reconstructed. */
  if(status!=RISC_RESIDENT_EXIT && !resident_focus(true))return RISC_RESIDENT_RETAINED;
  paper_previous_valid=previous_valid=false;
 }
 if(status==RISC_RESIDENT_EXIT)handoff_requested=true;
 return status;
}
#endif
