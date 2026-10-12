/* Files owns drafts and mutations. The shared plaintext host owns all text
 * capture and presentation, including rapid keys while a frame is pending. */
static bool fb_text_begin(unsigned mode,const char *label) {
 if(fb_text_active||!portable_file_browser_safe())return false;
#ifdef PORTABLE_TOUCH_SCROLL
 portable_file_browser_scroll_interrupt();
#endif
 fb_text_state=(risc_text_entry_state_v1_extended){.struct_size=sizeof(fb_text_state)};
 fb_text_closing=fb_text_invalidated=false;
 int rc=portable_text_client_begin_extended(&fb_text,fb_runtime,label,fb_editor,
                                           mode==FB_FILTER?sizeof(fb_query):sizeof(fb_editor));
 if(rc!=RISC_TEXT_ENTRY_OK){if(!fb_text.retained)fb_message("Keyboard unavailable. Retry.");return false;}
 fb_text_return_mode=fb_mode;fb_text_return_choice=fb_choice;fb_text_mode=mode;fb_text_storage=fb_storage_index;
 fb_mode=mode;fb_text_active=true;fb_armed=fb_contact=fb_drag=fbp_contact_down=false;return true;
}
static bool fb_text_close(void) {
 if(fb_text.retained||portable_text_adapter_retained())return false;
 if(!fb_text_active)return true;
 fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_text_closing=true;
 int rc=portable_text_client_close(&fb_text);
 if(rc!=RISC_TEXT_ENTRY_OK)return false;
 fb_text_active=fb_text_closing=false;memset(&fb_text_state,0,sizeof(fb_text_state));return true;
}
static void fb_text_step(void) {
 if(!fb_text_active)return;
 /* An expired/disconnected peer may restore the local mount while the host
  * owns input. Never apply the old draft to the replacement volume or route. */
 if(fb_mode!=fb_text_mode||fb_storage_index!=fb_text_storage){fb_text_invalidated=true;fb_text_closing=true;fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;}
 if(!fb_text_closing){
  int rc=portable_text_client_poll_extended(&fb_text,&fb_text_state);
  if(rc==RISC_TEXT_ENTRY_AGAIN)return;
  if(rc==RISC_TEXT_ENTRY_RETAINED||fb_text.retained)return;
  if(rc!=RISC_TEXT_ENTRY_OK){fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_message("Keyboard unavailable. Retry.");}
  else if(fb_text_state.state==RISC_TEXT_ENTRY_EDITING)return;
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
