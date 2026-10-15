/* Files owns drafts and mutations. The shared plaintext host owns all text
 * capture and presentation, including rapid keys while a frame is pending. */
static bool fb_text_begin(unsigned mode,const char *label) {
 if(fb_text_active||!portable_file_browser_safe())return false;
#ifdef PORTABLE_TOUCH_SCROLL
 portable_file_browser_scroll_interrupt();
#endif
 fb_text_state=(risc_text_entry_state_v1_extended){.struct_size=sizeof(fb_text_state)};
 fb_text_closing=fb_text_invalidated=false;
#ifdef PORTABLE_TEXT_INPUT_POLICY
 fb_text_checkpoint=(portable_text_checkpoint_client){0};fb_text_policy_boundary=fb_text_policy_continuity=false;
#endif
 int rc=portable_text_client_begin_extended(&fb_text,fb_runtime,label,fb_editor,
                                           mode==FB_FILTER?sizeof(fb_query):sizeof(fb_editor));
 if(rc!=RISC_TEXT_ENTRY_OK){if(!fb_text.retained)fb_message("Keyboard unavailable. Retry.");return false;}
 fb_text_return_mode=fb_mode;fb_text_return_choice=fb_choice;fb_text_mode=mode;fb_text_storage=fb_storage_index;
 fb_mode=mode;fb_text_active=true;fb_armed=fb_contact=fb_drag=fbp_contact_down=false;
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
 if(fb_mode!=fb_text_mode||fb_storage_index!=fb_text_storage){fb_text_invalidated=true;fb_text_closing=true;fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;}
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
 fb_armed=fb_contact=fb_drag=fbp_contact_down=false;fbp_clean=true;fb_dirty=true;
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
static void fb_text_draw(void) {
 if(fb_paper){
  if(!paper_frame_ready())return;
  fb_paper->begin();fbp_text(32,28,fbp_w()-64,fb_mode==FB_FILTER?"FIND FILE":"EDIT NAME",true,true,false);
  fbp_wrap(32,120,fbp_w()-64,4,fb_editor,true);fbp_wrap(32,276,fbp_w()-64,4,fb_status,true);
  int half=(fbp_w()-80)/2;fbp_button(32,fbp_h()-112,half,88,"CANCEL",NULL,false);fbp_button(fbp_w()/2+8,fbp_h()-112,half,88,"EDIT",NULL,true);
  fb_app->present(true);
 }else{
  portable_nova_begin();portable_nova_header(fb_mode==FB_FILTER?"FIND FILE":"EDIT NAME");
  portable_nova_wrap(2,18,52,204,18,3,fb_editor,NOVA_CYAN);portable_nova_wrap(1,18,116,204,18,3,fb_status,NOVA_WHITE);
  portable_nova_button(16,188,208,44,"Edit",true);fb_app->present(false);
 }
 fb_dirty=false;
}
