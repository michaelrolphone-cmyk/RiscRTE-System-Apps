/* New candidate values are demonstrated by real status observations. A Cancel,
 * timeout, noisy/insufficient session, or failed restoration never writes NVS. */
static bool settings_tap_begin_candidate(void){
  if(tap_cleanup_pending && !tap_motion->resume_wake(tap_motion->context)){tap_message="Restore failed - retry";return false;}
  tap_cleanup_pending=true; /* even a failed begin can partially mutate */
  if(!tap_motion->tap_observe_begin(tap_motion->context,(uint16_t)tap_calibration.value)){
    tap_message="Sensor setup failed";return false;
  }
  portable_tap_calibration_candidate(&tap_calibration,millis_now());return true;
}
static bool settings_tap_measure(void){
  if(!settings_tap_close())return false;
  tap_grant=(risc_runtime_capability_v1){.struct_size=sizeof(tap_grant)};
  if(!rt->acquire(TWATCH_MOTION_CAPABILITY,1,0,&tap_grant)){tap_message="Sensor unavailable";return false;}
  tap_motion=tap_grant.api;tap_info=(twatch_tap_info_v1){.struct_size=sizeof(tap_info)};
  if(!portable_motion_tap_valid(tap_motion) || !tap_motion->tap_info(tap_motion->context,&tap_info) ||
     !portable_tap_calibration_start(&tap_calibration,&tap_info,millis_now())){
    tap_message="Sensor update required";(void)settings_tap_close();return false;
  }
  settings_motion_active=true;settings_motion_interrupted=false;tap_message="";
  if(!settings_tap_begin_candidate()){
    tap_calibration.phase=TAP_CAL_ERROR;(void)settings_tap_close();return false;
  }
  sv_switch(SV_TAP_CAL);t5_app_input_t input;bool cancelled=false;
  while(tap_calibration.phase<TAP_CAL_NEXT || tap_calibration.phase==TAP_CAL_NEXT){
    unsigned phase=tap_calibration.phase,pairs=tap_calibration.pairs,value=tap_calibration.value;
    if(!poll(&input,10) || input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK)){tap_requested_exit=input.exit_requested;cancelled=true;break;}
    if(settings_motion_interrupted){tap_message="Calibration interrupted";tap_calibration.phase=TAP_CAL_ERROR;break;}
    twatch_tap_observation_v1 observed={.struct_size=sizeof(observed)};
    if(!tap_motion->tap_observe(tap_motion->context,&observed)){tap_message="Sensor read failed";tap_calibration.phase=TAP_CAL_ERROR;break;}
    portable_tap_calibration_feed(&tap_calibration,millis_now(),&observed);
    if(tap_calibration.phase==TAP_CAL_NEXT && !settings_tap_begin_candidate()){tap_calibration.phase=TAP_CAL_ERROR;break;}
    if(phase!=tap_calibration.phase || pairs!=tap_calibration.pairs || value!=tap_calibration.value)settings_view_redraw();
  }
  bool ready=!cancelled && tap_calibration.phase==TAP_CAL_READY;
  if(cancelled)tap_message="Calibration cancelled";
  else if(tap_calibration.phase==TAP_CAL_NOISY)tap_message="Extra triggers. Try again.";
  else if(tap_calibration.phase==TAP_CAL_INSUFFICIENT)tap_message="Not enough clear samples";
  else if(tap_calibration.phase==TAP_CAL_ERROR && !tap_message[0])tap_message="Sampling interrupted";
  if(!settings_tap_close())ready=false;
  if(!ready && tap_calibration.phase==TAP_CAL_READY)tap_calibration.phase=TAP_CAL_ERROR;
  return ready;
}
static uint8_t settings_tap_activate(void){
  int loaded=portable_tap_load(settings_store,&tap_draft);
  tap_message=tap_cleanup_pending?"Restore failed - retry":tap_unconfirmed?"Save unconfirmed - retry":
      loaded==PORTABLE_TAP_UNAVAILABLE?"Storage unavailable":loaded==PORTABLE_TAP_INVALID?"Invalid record - choose":"";
  tap_choice=tap_draft.enabled?1:0;tap_requested_exit=false;
  sv_switch(SV_TAP);bool armed=false;t5_app_input_t input;
  uint8_t result=T5_APP_SETTING_NO_CHANGE;
  while(poll(&input,20)){
    if(input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK)){
      if(!settings_tap_close()){settings_view_redraw();continue;}
      if(input.exit_requested){result=T5_APP_SETTING_ACTION_REQUESTED;break;}
      if(sv_page!=SV_TAP){sv_switch(SV_TAP);continue;}
      break;
    }
    if(!input.buttons && !input.tapped){armed=true;continue;}
    if(!armed)continue;
    armed=false;
    bool confirm=(input.buttons&T5_APP_BUTTON_CONFIRM)!=0;
    if(sv_page==SV_TAP_INTRO){
      if(confirm){
        bool ready=settings_tap_measure();
        if(failed)break;
        if(tap_requested_exit){result=T5_APP_SETTING_ACTION_REQUESTED;break;}
        if(ready)sv_switch(SV_TAP_CAL);else sv_switch(SV_TAP);
      }
      continue;
    }
    if(sv_page==SV_TAP_CAL){
      if(confirm && tap_calibration.phase==TAP_CAL_READY && !tap_cleanup_pending){
        if(tap_info.profile==TWATCH_TAP_BMA423)tap_draft.bma423=(uint8_t)tap_calibration.value;
        else tap_draft.bma456h=(uint8_t)tap_calibration.value;
        tap_draft.measured|=(uint8_t)(1u<<(tap_info.profile-1));
        /* Calibrating while Off does not implicitly enable tap wake. */
        if(portable_tap_save(settings_store,&tap_draft)){tap_unconfirmed=false;result=T5_APP_SETTING_UPDATED;tap_message="Measured setting saved";break;}
        tap_unconfirmed=true;tap_message="Save unconfirmed - retry";sv_switch(SV_TAP);
      }
      continue;
    }
    if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))tap_choice=(tap_choice+2)%3;
    else if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))tap_choice=(tap_choice+1)%3;
    else if(input.tapped && sv_inside(input.touch_x,input.touch_y,20,62,96,44)){tap_choice=0;tap_draft.enabled=0;}
    else if(input.tapped && sv_inside(input.touch_x,input.touch_y,124,62,96,44)){tap_choice=1;tap_draft.enabled=1;}
    else if(input.tapped && sv_inside(input.touch_x,input.touch_y,20,116,200,44)){
      tap_choice=2;
      if(tap_cleanup_pending){(void)settings_tap_close();settings_view_redraw();continue;}
      sv_switch(SV_TAP_INTRO);continue;
    }
    if(confirm){
      if(tap_cleanup_pending){(void)settings_tap_close();settings_view_redraw();continue;}
      if(portable_tap_save(settings_store,&tap_draft)){tap_unconfirmed=false;result=T5_APP_SETTING_UPDATED;tap_message="Tap wake saved";break;}
      tap_unconfirmed=true;tap_message="Save unconfirmed - retry";
    }
    if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT)){
      if(tap_choice<2)tap_draft.enabled=(uint8_t)tap_choice;
      else {sv_switch(SV_TAP_INTRO);continue;}
    }
    settings_view_redraw();
  }
  /* Runtime checks its exit barrier before fini. Never return an unrestored
   * enrollment on a failed display/health/poll path; retain this live stack. */
  while(!settings_tap_close()){
    rt->diagnostic("TAP restore-unconfirmed; retrying with live authority");rt->yield_ms(50);
  }
  settings_message=result==T5_APP_SETTING_UPDATED?tap_message:tap_unconfirmed?"Tap setting unconfirmed":"";
  sv_switch(SV_ROOT);return result;
}
