/* Extracted domain from file_browser_shared_input.inc; semantic UI/lifecycle supplied by files_scene.c. */
/* Files owns drafts and mutations. The shared plaintext host owns all text
 * capture and presentation, including rapid keys while a frame is pending. */
static bool fb_text_open_intent(const files_text_intent *intent) {
 unsigned mode=intent->requested_mode;const char *label=intent->label;
 if(fb_text_active||!portable_file_browser_safe())return false;
 fb_text_state=(risc_text_entry_state_v1_extended){.struct_size=sizeof(fb_text_state)};
 fb_text_closing=fb_text_invalidated=false;
#ifdef PORTABLE_TEXT_INPUT_POLICY
 fb_text_checkpoint=(portable_text_checkpoint_client){0};fb_text_policy_boundary=fb_text_policy_continuity=false;
#endif
 int rc=portable_text_client_begin_extended(&fb_text,fb_runtime,label,intent->draft,intent->capacity);
 if(rc!=RISC_TEXT_ENTRY_OK){if(!fb_text.retained)fb_message("Keyboard unavailable. Retry.");return false;}
 fb_text_return_mode=intent->identity.mode;fb_text_return_choice=intent->identity.choice;fb_text_mode=mode;fb_text_storage=intent->identity.storage;
 fb_mode=mode;fb_text_active=true;
#ifdef PORTABLE_TEXT_INPUT_POLICY
 rc=portable_text_checkpoint_enable(&fb_text,&fb_text_checkpoint);
 if(rc!=RISC_TEXT_ENTRY_OK&&!fb_text.retained){fb_text_closing=true;fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_message("Keyboard unavailable. Retry.");}
#endif
 return true;
}
static bool fb_text_close(void) {
 if(fb_text.retained||portable_text_adapter_retained())return false;
 if(!fb_text_active)return true;
 fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_text_closing=true;
 int rc=portable_text_client_close(&fb_text);
 if(rc!=RISC_TEXT_ENTRY_OK)return false;
 fb_text_active=fb_text_closing=false;memset(&fb_text_state,0,sizeof(fb_text_state));return true;
}
#ifdef PORTABLE_TEXT_INPUT_POLICY
static void fb_text_policy(void){
 if(!fb_text_checkpoint.pausing&&!fb_text_checkpoint.paused){
  risc_text_entry_checkpoint_state_v1 snapshot={.struct_size=sizeof(snapshot)};
  int rc=portable_text_checkpoint_snapshot(&fb_text,&fb_text_checkpoint,&snapshot);
  if(rc!=RISC_TEXT_ENTRY_OK||fb_text.retained)return;
  fb_text_policy_boundary=!(snapshot.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING);
  rc=portable_text_adapter_policy_prepare(snapshot.flags,&fb_text_policy_continuity);
  fb_text_policy_boundary=false;
  if(rc<0){(void)portable_text_client_retain(&fb_text);return;}
  if(!rc)return;
 }
 int rc=portable_text_checkpoint_pause_mode(&fb_text,&fb_text_checkpoint,fb_text_policy_continuity);
 if(rc!=RISC_TEXT_ENTRY_OK||fb_text.retained)return;
 fb_text_policy_boundary=true;
 uint32_t flags=0;int policy=portable_text_adapter_policy_apply(fb_text_policy_continuity,&flags);
 fb_text_policy_boundary=false;
 if(policy<0){(void)portable_text_client_retain(&fb_text);return;}
 if(fb_text.retained)return;
 if(policy==2)return; /* Root continues a fully paused policy boundary. */
 if(policy>0){fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_text_closing=true;return;}
 (void)portable_text_checkpoint_resume(&fb_text,&fb_text_checkpoint,flags);
}
#endif
static void fb_text_step(void) {
 if(!fb_text_active)return;
#ifdef PORTABLE_TEXT_INPUT_POLICY
 if(fb_text.retained)return;
 if(portable_text_adapter_policy_capture()<0){(void)portable_text_client_retain(&fb_text);return;}
#endif
 /* An expired/disconnected peer may restore the local mount while the host
  * owns input. Never apply the old draft to the replacement volume or route. */
 if(fb_text_invalidated||fb_mode!=fb_text_mode||fb_storage_index!=fb_text_storage){fb_text_invalidated=true;fb_text_closing=true;fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;}
#ifdef PORTABLE_TEXT_INPUT_POLICY
 if(!fb_text_closing&&(fb_text_checkpoint.pausing||fb_text_checkpoint.paused)){
  int attention=portable_text_adapter_attention();
  if(attention<0){(void)portable_text_client_retain(&fb_text);return;}
  if(!attention){fb_text_policy();return;}
  fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_text_closing=true;
 }
#endif
 if(!fb_text_closing){
  int rc=portable_text_client_poll_extended(&fb_text,&fb_text_state);
#ifdef PORTABLE_TEXT_INPUT_POLICY
  if(fb_text.retained)return;
  if(portable_text_adapter_policy_capture()<0){(void)portable_text_client_retain(&fb_text);return;}
#endif
  if(rc==RISC_TEXT_ENTRY_AGAIN)return;
  if(rc==RISC_TEXT_ENTRY_RETAINED||fb_text.retained)return;
  if(rc!=RISC_TEXT_ENTRY_OK){fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_message("Keyboard unavailable. Retry.");}
#ifdef PORTABLE_TEXT_INPUT_POLICY
  else if(fb_text_state.state==RISC_TEXT_ENTRY_EDITING){fb_text_policy();return;}
#else
  else if(fb_text_state.state==RISC_TEXT_ENTRY_EDITING)return;
#endif
  fb_text_closing=true;
 }
 int rc=portable_text_client_close(&fb_text);
 if(rc!=RISC_TEXT_ENTRY_OK||fb_text.retained)return;
 fb_text_active=fb_text_closing=false;fb_text_home=(fb_text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)!=0;
 fb_dirty=true;
 if(!fb_text_invalidated){
  if(fb_text_state.state==RISC_TEXT_ENTRY_ACCEPTED){
   memcpy(fb_editor,fb_text_state.text,sizeof(fb_editor));fb_filter_apply();
  }else{
   fb_mode=fb_text_return_mode;fb_choice=fb_text_return_choice;
#ifdef PORTABLE_FILE_PEER
   if(fb_mode!=FB_RENAME)fbm_new_directory=false;
#endif
   fb_message("Entry cancelled");
  }
 }
 memset(&fb_text_state,0,sizeof(fb_text_state));
}
/* Only errors can leave the app on an editor route after the host closes.
 * Keep that accepted draft intact and let the user explicitly edit or Back. */

