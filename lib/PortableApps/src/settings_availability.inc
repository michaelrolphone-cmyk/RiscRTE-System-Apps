/* A temporary draft is never rebased onto a newly found source. Keep its data
 * grant until explicit Save or Discard; host handoff is BUSY during the edit. */
static unsigned settings_availability_return_page;
static bool settings_availability_editing(void) {return settings_availability_draft;}
static bool settings_availability_filter(t5_app_input_t *input) {
 if(!settings_availability_draft||settings_atomic_running||settings_atomic_edit.token)return false;
 bool leave=input->exit_requested||(input->buttons&T5_APP_BUTTON_BACK)||home_pending||crown_pending||
  (sv_page==SV_FIELDS&&editor_field==7&&(input->buttons&T5_APP_BUTTON_CONFIRM));
 if(settings_availability_discard) {
  bool discard=!!(input->buttons&T5_APP_BUTTON_CONFIRM);
  bool keep=!!(input->buttons&T5_APP_BUTTON_BACK)||home_pending||crown_pending;
  home_pending=crown_pending=false;navigation_pending=0;
  memset(input,0,sizeof(*input));
  if(discard||keep) {
   settings_availability_discard=false;
   if(discard)settings_availability_draft=false;
   sv_switch(settings_availability_return_page);
   if(discard){input->buttons=T5_APP_BUTTON_BACK;input->exit_requested=true;}
  }
  return true;
 }
 /* Back inside the date value selector or time-zone cities is navigation,
  * not an editor exit. Physical Home still asks before any handoff. */
 if(!input->exit_requested&&!home_pending&&!crown_pending&&
    (sv_page==SV_VALUE||sv_page==SV_TIMEZONE_CITIES))return false;
 if(!leave)return false;
 settings_availability_return_page=sv_page;settings_availability_discard=true;
 home_pending=crown_pending=false;navigation_pending=0;memset(input,0,sizeof(*input));
 sv_switch(SV_DISCARD);return true;
}
static uint8_t settings_availability_activate(void) {
 if(settings_availability_draft||settings_atomic_owned())return T5_APP_SETTING_NO_CHANGE;
 if(!settings_availability_refresh())return T5_APP_SETTING_ERROR;
 sv_switch(SV_STORAGE);bool armed=false;t5_app_input_t input;
 while(poll(&input,20)) {
  if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK))break;
  if(!input.buttons&&!input.tapped){armed=true;continue;}
  if(!armed)continue;
  armed=false;
  if(input.buttons&T5_APP_BUTTON_CONFIRM) {
   /* The same explicit zero-draft boundary closes all Settings-owned data.
    * A provider or resident host may still hold another namespace-1 grant. */
   if(!settings_atomic_close()){portable_adapter_retain();return T5_APP_SETTING_ERROR;}
   rt->yield_ms(1);
   if(native_custody_retained||!settings_atomic_open())return T5_APP_SETTING_ERROR;
   settings_time_format_load();
   settings_message=settings_availability.state.availability==RISC_KEY_VALUE_RECONCILIATION_PENDING?
     "Still pending: finish other edits, then clean Runtime restart":
     portable_settings_availability_message(&settings_availability);
   settings_view_redraw();
  }
 }
 settings_message=portable_settings_availability_message(&settings_availability);
 sv_switch(SV_ROOT);return T5_APP_SETTING_NO_CHANGE;
}
