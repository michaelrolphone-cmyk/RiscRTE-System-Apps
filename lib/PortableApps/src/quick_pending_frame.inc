#ifdef PORTABLE_QUICK_SLICED_NATIVE
static uint64_t quick_pending_receipt;
#endif
/* Opt-in copied-preference barrier shared by foreground Quick clients. The
 * exact preview is visibly unconfirmed before any submit. Historical profiles
 * and Home's independent controller are unchanged when this selector is off. */
static char quick_pending_time[6]="--:--";
static bool quick_pending_battery_valid;
static uint8_t quick_pending_battery;
static bool quick_pending_safe(void *unused){
 (void)unused;return quick_atomic_guard(NULL)&&!native_sleep_retained;
}
static int quick_pending_cancelled(void *unused){
 (void)unused;
 /* This modal owns and consumes raw input while the app is paused. Like the
  * outer Quick loop, discard its already-delivered sample before the next
  * service/dispatch cycle; otherwise input_pending masks every later edge. */
 input_pending=false;input_sample=(portable_touch_sample){0};
 input_service();if(!quick_pending_safe(NULL))return -1;
 input_dispatch();if(!quick_pending_safe(NULL))return -1;
 input_pending=false;input_sample=(portable_touch_sample){0};
 if(quick.ui.save_cancelled||home_pending||handoff_requested||
    (navigation_pending&T5_APP_BUTTON_BACK)||touch.down||touch.poll_failed||touch.resync||
    !pqa_visible(&quick.ui)||quick.ui.torch)return 1;
 bool consumed=false;
 if(!alarm_foreground(&consumed)||!quick_pending_safe(NULL))return -1;
 return consumed||alarm_modal||quick.ui.save_cancelled||
  home_pending||handoff_requested||(navigation_pending&T5_APP_BUTTON_BACK)?1:0;
}
static int quick_pending_ready(void *unused){
 (void)unused;
 if(!paper_present_progress()||!quick_pending_safe(NULL))return -1;
 if(!portable_paper_frame_idle())return 0;
#ifdef PORTABLE_QUICK_SLICED_NATIVE
 if(quick_pending_receipt&&presentation_completed_id!=quick_pending_receipt){
  /* Idle after replacement/discard is not this preview's completion. */
  quick.ui.save_cancelled=true;return 0;
 }
#endif
 return 1;
}
static bool quick_pending_paint(void *unused){
 (void)unused;
 if(!portable_paper_frame_ready()||!quick_pending_safe(NULL))return false;
 if(!quick_copy_background()||!quick_pending_safe(NULL))return false;
 if(!quick_render_watch(quick_pending_time,quick_pending_battery_valid,quick_pending_battery)||!quick_pending_safe(NULL))return false;
 #ifdef PORTABLE_QUICK_SLICED_NATIVE
 uint64_t before=presentation_requested_id;
#endif
 present(!quick.ui.paper);
#ifdef PORTABLE_QUICK_SLICED_NATIVE
 if(!quick_pending_safe(NULL))return false;
 if(presentation_requested_id==before)return false;
 quick_pending_receipt=presentation_requested_id;
#endif
 return quick_pending_safe(NULL);
}
static int quick_pending_frame(void){
 if(!quick_pending_safe(NULL))return PENDING_FAULT;
 quick.ui.save_cancelled=false;quick.ui.save_pending=true;
#ifdef PORTABLE_QUICK_SLICED_NATIVE
 quick_pending_receipt=0;
#endif
 portable_quick_pending_frame frame={0};
 const portable_quick_pending_ops ops={quick_pending_safe,quick_pending_cancelled,quick_pending_ready,quick_pending_paint};
 for(unsigned n=0;n<10000;n++){
  int result=portable_quick_pending_step(&frame,&ops,NULL);
  if(result!=PENDING_WAIT)return result;
  if(!quick_pending_safe(NULL))return PENDING_FAULT;
  rt->yield_ms(1);
 }
 /* An uncompleted image cannot authorize a mutation. Preserve custody and the
  * last visible pending image; never issue speculative cleanup or a write. */
 portable_adapter_retain_silent();return PENDING_FAULT;
}
static void quick_pending_done(void){
 /* Only safe completion/cancellation reaches this, never retained results. */
 quick.ui.save_pending=false;quick.ui.save_cancelled=false;
 pqa_cancel_input(&quick.ui);
}
