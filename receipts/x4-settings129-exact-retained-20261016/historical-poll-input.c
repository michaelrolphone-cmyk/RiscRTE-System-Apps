static bool poll_input(t5_app_input_t *out, uint32_t wait) {
  memset(out, 0, sizeof(*out));
  clear_contact_snapshots();
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
  /* TIMER has no foreground modal or subscriptions. The selected clock owns
   * alarm reconciliation and the promotion decision before normal polling. */
  if(!portable_desk_adapter_ready())return false;
  if(desk_phase==DESK_TIMER){rt->yield_ms(wait?wait:1);return !failed;}
#endif
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  app_contact=(t5_app_contact_t){0};
#endif
  if (failed)
    return false;
#ifdef PORTABLE_ALARM_CLIENT
  bool consumed=false;
  if(!alarm_foreground(&consumed))return false;
  if(consumed){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    crown_pending=false;return true;}
#endif
#ifdef PORTABLE_BLE_BROADCAST
  /* Background model/service progression shares the owner task, not the
   * display completion clock. Mutable raster custody is still excluded. */
  if(!raster_lease_mutable() && !broadcast_tick())return false;
#endif
  /* A rejected paper top-edge gesture replays its original down followed
   * by the saved current sample before sampling another contact. */
#ifdef PORTABLE_QUICK_ACTIONS
  if(quick_replay_delivery){quick_replay_delivery=false;goto replay_input;}
#endif
  uint32_t now=millis_now(),spent=now-last_poll_at;
  if(failed)return false;
  input_service();input_dispatch();
  if(failed)return false;
  const uint32_t idle_budget=spent<wait?wait-spent:0;
  uint32_t delay=idle_budget;
  while(delay && !input_progressed && !navigation_pending && !failed) {
    uint32_t slice=delay>4u?4u:delay;
#ifdef PORTABLE_USB_TRANSFER_APP
    if(slice>2u)slice=2u;
    portable_usb_transfer_service();
#endif
    rt->yield_ms(paper_token?1u:slice);
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!contexts_capture_checkpoint())return false;
#endif
    if(!paper_present_progress())return false;
    input_pending=false;input_service();input_dispatch();
    uint32_t elapsed=(uint32_t)(millis_now()-now);
    delay=elapsed>=idle_budget?0:idle_budget-elapsed;
  }
  if(input_progressed || navigation_pending || !wait)rt->yield_ms(1);
  last_poll_at=millis_now();
  if(!paper_present_progress() || failed)return false;
#ifdef PORTABLE_ALARM_CLIENT
  if(!alarm_foreground(&consumed))return false;
  if(consumed){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    crown_pending=false;return true;}
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
  /* A recognized pull-down is model state. Keep it while an existing image
   * still owns the display, and continue child input/services in the meantime.
   * Once custody is clear, the ordinary checkpoint attempts it exactly once;
   * a user-declined edit/launch guard must not turn into repeated prompts. */
  bool resident_display_ready=!surface.frame && !paper_token && display_settled && !alarm_modal;
  if(resident_display_ready) {
    uint32_t reason=resident_controls_requested?RISC_RESIDENT_CHECKPOINT_CONTROLS:RISC_RESIDENT_CHECKPOINT_POLL;
    int status=resident_checkpoint(reason);
    if(status==RISC_RESIDENT_RETAINED)return false;
    if(status==RISC_RESIDENT_EXIT){out->exit_requested=true;return true;}
    if(status<0){failed=true;return false;}
#ifdef PORTABLE_RESIDENT_POLICY
    if(reason==RISC_RESIDENT_CHECKPOINT_POLL && status==RISC_RESIDENT_OK && resident_policy_pending &&
       resident_policy_ready()) {
      status=resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY);
      if(status==RISC_RESIDENT_RETAINED)return false;
      if(status==RISC_RESIDENT_EXIT){out->exit_requested=true;return true;}
      if(status<0){failed=true;return false;}
      return true;
    }
#endif
    if(reason==RISC_RESIDENT_CHECKPOINT_CONTROLS) {
      resident_controls_requested=false;return true;
    }
  }
#endif
#ifdef PORTABLE_LOW_BATTERY
  if(!low_battery_poll()){failed=true;return false;}
#endif
#ifdef PORTABLE_CRASH_REPORT_SD
  if(!failure_archive_checkpoint())return false;
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!raster_lease_mutable()&&!contexts_tick())return false;
#if defined(PORTABLE_CONTEXTS_CLOCK_RF_ONLY) && !defined(PORTABLE_RESIDENT_SHELL_HOST)
  if(contexts_clock_handoff){out->exit_requested=true;return true;}
#endif
#endif
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
  portable_audio_capture_resume();
#endif
#if defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_SLEEP_MANUAL_ONLY)
  if(!failed && (uint32_t)(millis_now()-last_activity)>=portable_idle_ms() &&
     !idle_capture_active() &&
#ifdef PORTABLE_X4_IDLE_POLICY
     automatic_idle_ready() &&
#endif
     !navigation_pending && !(input_pending && (input_sample.down || input_sample.released))) {
#ifdef PORTABLE_DESK_LOCK_HOME
    /* Home's timeout enters the same desk owner as its explicit lock. Other
     * foreground apps keep the reversible Light helper and suspended stack. */
    if(!portable_desk_clock_request_lock(true))return !failed;
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
    automatic_idle=true;
#endif
    if(!idle_sleep())return false;
#ifdef PORTABLE_DESK_LOCK_HOME
    if(!portable_desk_clock_request_lock(false))return false;
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
    automatic_idle=false;
#endif
    return !failed; /* Waking crown/contact never becomes an app action. */
  }
#endif
#ifdef PORTABLE_QUICK_ACTIONS
  bool quick_consumed=false;
  if(!quick_foreground(&quick_consumed))return false;
  if(quick_consumed){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    #if !defined(PORTABLE_RESIDENT_SHELL_HOST) && (!defined(PORTABLE_UPDATE_APP) || !defined(PORTABLE_HOME_APP))
    crown_pending=false;
#endif
    out->exit_requested=quick_launch_pending;
    /* A reserved gesture is not app contact. Keep the presentation neutral
     * without an invented release, so deliberate replay can begin normally. */
    if(quick.ui.gesture==PQA_TOP_PENDING && !pqa_visible(&quick.ui))nova_contact.valid=true;
    input_pending=false;
    return true;
  }
replay_input:
#endif
#ifdef PORTABLE_HOME_APP
  /* Physical root navigation must not first trigger a nested app Back/redraw. */
  if(home_pending||crown_pending){
#if defined(PORTABLE_SETTINGS_APP) && defined(PORTABLE_TOUCH_SCROLL)
    settings_view_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
    portable_wifi_scroll_interrupt();
#endif
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_FILE_BROWSER_APP)
    portable_file_browser_scroll_interrupt();
#endif
    navigation_pending=0;input_pending=false;return !failed;}
#endif
  if (nova_mode) { np_poll(out); input_navigation_take(out); return !failed; }
#ifdef PORTABLE_SETTINGS_APP
  if (settings_view_poll(out)) return !failed;
#endif
  input_navigation_take(out);
#if defined(PORTABLE_TOUCH_SCROLL) && defined(PORTABLE_WIFI_SETTINGS_APP)
  if(wifi_scroll_poll(out))return !failed;
#endif
  if(out->buttons&T5_APP_BUTTON_BACK)return !failed;
  portable_touch_sample sample;input_take(&sample);
  uint16_t x=sample.x,y=sample.y;
#if defined(PORTABLE_APP_OWNS_TOUCH_CHROME) && !defined(PORTABLE_SETTINGS_APP)
  if(sample.valid && !sample.cancelled && sample.tap_eligible && sample.down && x<width() && y<height())
    app_contact=(t5_app_contact_t){true,(int16_t)x,(int16_t)y};
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
  if(sample.cancelled || !sample.valid)nu_gesture=false;
  if(sample.began){nu_gesture=true;nu_start_x=x;nu_start_y=y;
#ifdef PORTABLE_QUICK_ACTIONS
   if(quick_replay_pending){nu_start_x=quick.ui.start_x;nu_start_y=quick.ui.start_y;}
#endif
  }
#ifdef PORTABLE_QUICK_ACTIONS
  quick_replay_pending=false;
#endif
  if(sample.released && nu_gesture) {
    nu_gesture=false;
    int dx=(int)x-nu_start_x,dy=(int)y-nu_start_y;
    if(sample.moved && !sample.cancelled) {
      if(dy>=28 && dy>abs(dx)*2)out->buttons|=T5_APP_BUTTON_UP;
      else if(dy<=-28 && -dy>abs(dx)*2)out->buttons|=T5_APP_BUTTON_DOWN;
      else if(dx>=36 && dx>abs(dy)*2)out->buttons|=T5_APP_BUTTON_BACK;
      return !failed;
    }
  }
#endif
  if (sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled) {
    if (x >= width() || y >= height())
      return !failed;
    if (
#ifdef PORTABLE_APP_OWNS_TOUCH_CHROME
        false /* App tabs and drag gestures own the whole touch surface. */
#elif defined(PORTABLE_NOVA_UI)
        y>=4 && y<48 && x>=8 && x<52
#else
        y<40 && x<56
#endif
       ) {
#ifdef PORTABLE_SETTINGS_APP
      out->exit_requested = back_exits_app;
#else
      out->exit_requested = true;
#endif
      out->buttons = T5_APP_BUTTON_BACK;
    }
#ifdef PORTABLE_NOVA_UI
    else if(list_mode && y>=184 && y<228) {
      if(x>=12 && x<64)out->buttons=T5_APP_BUTTON_UP;
      else if(x>=72 && x<168)out->buttons=T5_APP_BUTTON_CONFIRM;
      else if(x>=176 && x<228)out->buttons=T5_APP_BUTTON_DOWN;
    }
#endif
#ifndef PORTABLE_NOVA_UI
    else if (list_mode && y >= height() - 32)
#ifdef PORTABLE_SETTINGS_APP
      out->buttons = settings_editing
                         ? (x < width() / 2 ? T5_APP_BUTTON_LEFT : T5_APP_BUTTON_RIGHT)
                         : (x < width() / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN);
#else
      out->buttons = x < width() / 2 ? T5_APP_BUTTON_UP : T5_APP_BUTTON_DOWN;
#endif
    else if (list_mode && y < 40 && x >= width() - 56)
      out->buttons = T5_APP_BUTTON_CONFIRM;
#endif
    else {
      out->tapped = true;
      out->touch_x = x;
      out->touch_y = y;
    }
  }
  return !failed;
}
