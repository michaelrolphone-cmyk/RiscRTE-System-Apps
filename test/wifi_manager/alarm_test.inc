static void alarm_test_due(void){
 fixture_alarm.state=ALARM_STATE_ALERT;
 fixture_alarm.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,7,1234,9};
 strcpy(fixture_alarm.label,"Shared test alarm");
}
static void alarm_test_modal(void){
 for(unsigned n=0;!(wm.alarm_flags&RISC_ALARM_FOREGROUND_HAS_FOCUS);n++){assert(n<3000&&native_alive&&!wm.retained);pump(1);}
 assert(wm.alarm_paused);
}
static void alarm_test_dismiss(void){
 /* Wait for the real modal to finish displaying before issuing Back. */
 pump(300);nav_pressed=RISC_NAV_BACK;
 for(unsigned n=0;!fixture_alarm_acks;n++){assert(n<3000&&native_alive&&!wm.retained);pump(1);}
 assert(fixture_alarm_acknowledged.revision==7&&fixture_alarm_acknowledged.deadline==1234&&fixture_alarm_acknowledged.generation==9);
 for(unsigned n=0;wm.alarm_requested||wm.alarm_paused;n++){assert(n<3000&&native_alive&&!wm.retained);pump(1);}
 assert(fixture_alarm.state==ALARM_STATE_READY);
}
static void alarm_test_run(const char*mode){
 uint64_t editor=0;unsigned radio=0;
 if(!strcmp(mode,"alarm-keyboard")){
  invoke(ADD_NETWORK,0);copy_text(wm.draft.password,sizeof(wm.draft.password),"preserved-secret");
  invoke(EDIT_PASSWORD,0);assert(wm.editor_session);editor=wm.editor_session;
 }else if(!strcmp(mode,"alarm-radio")){
  invoke(SCAN,0);assert(radio_id);radio=radio_id;
 }else if(!strcmp(mode,"alarm-storage")){
  storage_test_draft();storage_test_submitted();assert(fixture_profile.token);
  alarm_test_due();pump(100);assert(!wm.alarm_paused&&!fixture_alarm_acks);
  fixture_profile.collect_delay=0;
 }
 alarm_test_due();alarm_test_modal();
 if(editor)assert(wm.editor_session==editor&&!wm.session);
 if(radio)assert(radio_id==radio&&wm.operation==radio&&!cancels);
 alarm_test_dismiss();
 if(editor){
  risc_text_entry_state_v1 value={.struct_size=sizeof(value)};
  assert(!text_api->poll(text_api->context,editor,&value)&&value.state==RISC_TEXT_ENTRY_EDITING);
  assert(!strcmp(value.text,"preserved-secret"));wipe(&value,sizeof(value));
  /* Focus restore schedules repaint; input is deliberately gated until the
   * restored keyboard is current. Do not inject a stale pre-paint Back. */
  capture(NULL,NULL);nav_pressed=RISC_NAV_BACK;
  for(unsigned n=0;wm.editor_session;n++){assert(n<3000&&native_alive&&!wm.retained);pump(1);}
 }
 if(radio)assert(radio_id==radio&&!cancels);
 assert(alarm_driver->quiesce());exit_app();alarm_driver->stop();
 printf("Shared Wi-Fi %s exact ACK, focus and native ownership PASS\n",mode);
}
