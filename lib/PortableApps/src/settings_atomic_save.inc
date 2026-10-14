/* The modal remains an ordinary input/display controller while Native owns
 * the copied operation. No resident host handoff crosses an owned token. */
static bool settings_atomic_save(const char *key,const void *value,uint32_t size,const char **message) {
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!settings_availability_save_ready(message)){settings_atomic_status=RISC_ATOMIC_BUSY;return false;}
#endif
 int rc=strcmp(key,settings_atomic_edit.key)?RISC_ATOMIC_INVALID:
  portable_preference_atomic_prepare(&settings_atomic_edit,value,size);
 if(rc!=PORTABLE_PREFERENCE_READY){*message=settings_atomic_message(rc);return false;}
 settings_atomic_running=true;*message="Saving locally - Back cancels";settings_view_redraw();
 bool cancel=false;
 for(;;) {
  if(!settings_atomic_guard(NULL)){rc=RISC_ATOMIC_RETAINED;break;}
  bool accepting=!cancel&&!settings_atomic_edit.token;
  rc=cancel?portable_preference_atomic_cancel(&settings_atomic_edit):
    settings_atomic_edit.token?portable_preference_atomic_collect(&settings_atomic_edit):
    portable_preference_atomic_submit(&settings_atomic_edit);
  if(accepting&&rc==RISC_ATOMIC_PENDING)settings_atomic_submission_accepted=true;
  if(rc!=RISC_ATOMIC_PENDING&&rc!=PORTABLE_PREFERENCE_READY)break;
  if(cancel) {
   /* poll returns immediately on a host exit request; this checked scheduler
    * delay lets the native worker finish without replaying normal UI work. */
   rt->yield_ms(1);continue;
  }
  t5_app_input_t input;
  if(!poll(&input,20)) {
   if(!settings_atomic_guard(NULL)){rc=RISC_ATOMIC_RETAINED;break;}
   cancel=true;settings_atomic_exit=true;
  } else if(input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK)) {
   cancel=true;settings_atomic_exit=true;
  }
 }
 settings_atomic_running=false;settings_atomic_result(rc);
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 /* Cancelling a save does not discard the editor's RAM draft. The accepted
  * attempt cannot be replayed; keep it visible until explicit Discard. */
 if(rc!=RISC_ATOMIC_RETAINED)settings_atomic_exit=false;
#endif
 *message=rc==RISC_ATOMIC_OK?"Saved locally; SD backup not confirmed":settings_atomic_message(rc);
 return rc==RISC_ATOMIC_OK;
}
static bool settings_atomic_choice(const char *key,uint8_t tag,unsigned value,unsigned count,const char **message) {
 if(value>=count){*message="Invalid selection";return false;}
 const uint8_t bytes[]={tag,1,(uint8_t)value,(uint8_t)(value^0xa5u)};
 return settings_atomic_save(key,bytes,sizeof(bytes),message);
}
#ifdef PORTABLE_LOW_BATTERY
static bool settings_atomic_timer(const char *key,uint32_t milliseconds,const char **message) {
 if(milliseconds<5000u||milliseconds>3600000u||milliseconds%1000u){*message="Invalid timer";return false;}
 unsigned seconds=milliseconds/1000u;
 const uint8_t bytes[]={0x54,1,(uint8_t)seconds,(uint8_t)(seconds>>8),(uint8_t)(seconds^(seconds>>8)^0xa5u)};
 return settings_atomic_save(key,bytes,sizeof(bytes),message);
}

#endif
