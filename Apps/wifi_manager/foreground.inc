/* App coordination only. Scheduler/UI behavior lives in the shared provider.
 * Modal focus is local provider custody, never a Runtime exit checkpoint. */
static bool keyboard_step(wifi_manager *);
static bool alarm_call(wifi_manager*a,bool focus){
 if(!a->alarm)return true;
 if(!storage_begin(a))return live(a); /* A busy PHY lease is ordinary deferral. */
 risc_alarm_foreground_status_v1 state={.struct_size=sizeof(state)};
 int rc=a->alarm->advance(a->alarm->context,focus?RISC_ALARM_FOREGROUND_FOCUS_AVAILABLE:0,&state);
 if(!live(a))return false;
 if(rc==RISC_ALARM_FOREGROUND_RETAINED)return retain(a);
 if(!storage_end(a))return false;
 if(rc==RISC_ALARM_FOREGROUND_BUSY)return true;
 if((rc!=RISC_ALARM_FOREGROUND_OK&&rc!=RISC_ALARM_FOREGROUND_AGAIN)||
 state.struct_size!=sizeof(state)||state.flags&~15u)return retain(a);
 a->alarm_flags=state.flags;
 if(state.flags&(RISC_ALARM_FOREGROUND_NEEDS_FOCUS|RISC_ALARM_FOREGROUND_CUE|RISC_ALARM_FOREGROUND_LOADING))a->alarm_requested=true;
 if(state.flags&RISC_ALARM_FOREGROUND_HAS_FOCUS)a->alarm_redraw=true;
 return true;
}
static bool alarm_probe(wifi_manager*a,uint32_t flags){
 if(!a->alarm||a->alarm_requested||a->alarm_paused||storage_pending(a)||
 (flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING)))return true;
 return alarm_call(a,false);
}
static bool alarm_consumes(wifi_manager*a){
 return a->alarm_requested||(a->alarm_flags&(RISC_ALARM_FOREGROUND_CUE|RISC_ALARM_FOREGROUND_LOADING));
}
static bool alarm_focus_step(wifi_manager*a){
 if(!a->alarm_requested&&!a->alarm_paused)return true;
 /* Complete accepted storage receipts before borrowing another foreground.
  * Preserve dirty and staged drafts; interruption is not Discard or Exit. */
 if(storage_pending(a)){db_step(a);if(live(a))(void)policy_step(a);return live(a);}
 if(!background_pause(a))return false;
 if(!a->alarm_paused){
  int rc;
  if(a->editor_session){
   if(!a->text_checkpoint)return retain(a);
   rc=a->text_checkpoint->pause_for_focus_change(a->text->context,a->editor_session);
   if(!live(a))return false;
   if(rc==RISC_TEXT_ENTRY_BUSY)return keyboard_step(a);
   if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
   if(rc!=RISC_TEXT_ENTRY_OK)return retain(a);
   a->alarm_owner=2;
  }else if(a->session){
   if(!a->checkpoint)return retain(a);
   rc=a->checkpoint->pause(a->scene->context,a->session);
   if(!live(a))return false;
   if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY)return true;
   if(rc!=RISC_SCENE_OK)return retain(a);
   a->alarm_owner=1;
  }else return retain(a);
  a->alarm_paused=true;
 }
 /* Keep RF connected using the existing checked service lease. No lease is
  * held across this function's return, and no resident checkpoint runs here. */
 if(!alarm_call(a,true))return false;
 if(a->alarm_flags)return true;
 int rc=a->alarm_owner==2?
 a->text_checkpoint->resume(a->text->context,a->editor_session,(a->alarm_redraw?RISC_TEXT_ENTRY_RESUME_REDRAW:0)|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED):
 a->checkpoint->resume(a->scene->context,a->session,(a->alarm_redraw?RISC_SCENE_RESUME_REDRAW:0)|RISC_SCENE_RESUME_FOCUS_CHANGED);
 if(!live(a))return false;
 if(rc)return retain(a);
 a->alarm_paused=a->alarm_requested=a->alarm_redraw=false;a->alarm_owner=0;
 return true;
}
