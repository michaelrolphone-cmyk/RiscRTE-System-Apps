static void sv_tap(void) {
  sv_heading("TAP TO WAKE");
  sv_button(20,62,96,44,"OFF",!tap_draft.enabled);
  sv_button(124,62,96,44,"ON",tap_draft.enabled);
  sv_button(20,116,200,44,tap_cleanup_pending?"RETRY RESTORE":"CALIBRATE TAPS",tap_choice==2);
  sv_center(3,171,tap_message[0]?tap_message:"Crown wake stays available",SV_CAP,0);
  sv_button(24,200,86,26,"CANCEL",false);sv_button(130,200,86,26,"SAVE",false);
}
static void sv_tap_calibration(void) {
  char line[48];sv_heading("MEASURE YOUR TAPS");
  if(sv_page==SV_TAP_INTRO){
    sv_center(2,63,"Wear the watch as usual",SV_TEXT,0);
    sv_center(2,86,"Give two similar knocks",SV_CYAN,0);
    sv_center(2,107,"when prompted. Then wait.",SV_TEXT,0);
    sv_center(2,137,"Finish with wrist movement",SV_TEXT,0);
    sv_center(3,169,"Tests actual sensor detections",SV_CAP,0);
    sv_button(24,200,86,26,"CANCEL",false);sv_button(130,200,86,26,"START",false);return;
  }
  snprintf(line,sizeof(line),"%s  SETTING %u",tap_info.profile==TWATCH_TAP_BMA423?"BMA423":"BMA456H",tap_calibration.value);
  sv_center(2,61,line,SV_CAP,0);
  if(tap_calibration.phase==TAP_CAL_SETTLE){sv_center(1,91,"GET READY",SV_CYAN,0);sv_center(2,123,"Wait for the tap prompt",SV_TEXT,0);}
  else if(tap_calibration.phase==TAP_CAL_PAIR){
    snprintf(line,sizeof(line),"DOUBLE TAP %u / 3",tap_calibration.pairs+1);sv_center(1,91,line,SV_CYAN,0);
    sv_center(2,123,"Two firm, similar knocks",SV_TEXT,0);
  }else if(tap_calibration.phase==TAP_CAL_QUIET){sv_center(1,91,"PAIR DETECTED",SV_CYAN,0);sv_center(2,123,"Wait. No more taps yet.",SV_TEXT,0);}
  else if(tap_calibration.phase==TAP_CAL_MOVE){sv_center(1,91,"MOVE YOUR WRIST",SV_CYAN,0);sv_center(2,123,"Normal movement, no taps",SV_TEXT,0);}
  else if(tap_calibration.phase==TAP_CAL_READY){sv_center(1,91,"3 OF 3 DETECTED",SV_CYAN,0);sv_center(2,119,"No movement triggers",SV_TEXT,0);sv_center(3,147,"Use SAVE to apply this setting",SV_CAP,0);}
  else {sv_center(1,91,"NOT CALIBRATED",SV_CYAN,0);sv_center(2,120,tap_message,SV_TEXT,0);}
  if(tap_calibration.phase<TAP_CAL_NEXT)sv_center(3,170,"Cancel keeps saved settings",SV_CAP,0);
  if(tap_calibration.phase==TAP_CAL_READY){sv_button(24,200,86,26,"CANCEL",false);sv_button(130,200,86,26,"SAVE",false);}
  else sv_button(82,192,76,26,tap_calibration.phase<TAP_CAL_NEXT?"CANCEL":tap_cleanup_pending?"RETRY":"BACK",false);
}
