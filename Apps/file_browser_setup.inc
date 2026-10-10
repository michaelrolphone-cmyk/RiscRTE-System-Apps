/* Optional BLE only brokers an already-ready finite HTTP session. */
#ifndef PORTABLE_FILE_SHARING_SETUP_INSTANCE
#error BLE setup requires an explicit provider instance
#endif
static portable_file_setup fsh_setup;
static portable_file_setup_status fsh_setup_status;
static bool fsh_setup_initialized,fsh_setup_screen,fsh_setup_leaving,fsh_setup_stopping,fsh_setup_servicing;
static uint64_t fsh_setup_polled;
static uint32_t fsh_setup_display_generation,fsh_setup_display_number;
static uint32_t fsh_setup_pending_generation,fsh_setup_pending_number;
static void fsh_setup_wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static bool fsh_setup_live(void){return fsh_setup_initialized&&fsh_setup_status.state!=PORTABLE_FILE_SETUP_STATE_OFF&&fsh_setup_status.state!=PORTABLE_FILE_SETUP_STATE_UNAVAILABLE;}
bool portable_file_browser_cleanup_only(void){return fsh_setup_initialized&&portable_file_setup_cleanup_only(&fsh_setup);}
static void fsh_setup_fence(void){
 if(fsh_setup_initialized)(void)portable_file_setup_close(&fsh_setup); /* owner is now false; pure wipe/fence */
 memset(&fsh_setup_status,0,sizeof(fsh_setup_status));
 fsh_setup_display_generation=fsh_setup_display_number=fsh_setup_pending_generation=fsh_setup_pending_number=0;
}
static bool fsh_setup_result(int rc){
 if(rc==PORTABLE_FILE_SETUP_RETAINED){fsh_retain();return false;}
 portable_file_setup_status next;portable_file_setup_get_status(&fsh_setup,&next);
 if(next.state!=fsh_setup_status.state||next.provider_state!=fsh_setup_status.provider_state||next.flags!=fsh_setup_status.flags||next.pairing_generation!=fsh_setup_status.pairing_generation||next.pairing_number!=fsh_setup_status.pairing_number||next.error!=fsh_setup_status.error)fb_dirty=true;
 if(next.pairing_generation!=fsh_setup_status.pairing_generation||next.pairing_number!=fsh_setup_status.pairing_number){fsh_setup_display_generation=0;fsh_setup_pending_generation=0;}
 fsh_setup_status=next;return true;
}
bool portable_file_browser_setup_service(void){
 if(!fsh_owner(NULL))return false;
 if(!fsh_setup_initialized||fsh_setup_status.state!=PORTABLE_FILE_SETUP_STATE_ACTIVE)return true;
 if(fsh_setup_servicing){fsh_retain();return false;}
 fsh_setup_servicing=true;
 uint64_t now=fsh_clock();
 bool ok=true;
 if(now-fsh_setup_polled>=8){fsh_setup_polled=now;ok=fsh_setup_result(portable_file_setup_step(&fsh_setup,now));}
 fsh_setup_servicing=false;return ok;
}
static bool fsh_setup_close(void){
 if(!fsh_setup_initialized)return true;
 fsh_setup_display_generation=fsh_setup_pending_generation=0;
 if(!fsh_setup_live())return true;
 int rc=portable_file_setup_close(&fsh_setup);
 if(!fsh_setup_result(rc))return false;
 if(fsh_setup_live())return false;
 fsh_setup_stopping=false;
 if(fsh_setup_leaving){fsh_setup_screen=false;fsh_setup_leaving=false;}
 fb_dirty=true;return true;
}
bool portable_file_browser_setup_cleanup(void){return fsh_setup_close();}
static void fsh_setup_display_sync(void){
 if(fsh_setup_pending_generation&&portable_file_browser_setup_frame_ready()){
  if(fsh_setup_pending_generation==fsh_setup_status.pairing_generation&&fsh_setup_pending_number==fsh_setup_status.pairing_number){
   fsh_setup_display_generation=fsh_setup_pending_generation;fsh_setup_display_number=fsh_setup_pending_number;
  }
  fsh_setup_pending_generation=0;
 }
}
static void fsh_setup_tick(void){
 if(!fsh_setup_initialized)return;
 if(fsh_setup_stopping||fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_ENDING||fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_CLEANUP_PENDING){(void)fsh_setup_close();return;}
 if(fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_STARTING){(void)fsh_setup_result(portable_file_setup_step(&fsh_setup,fsh_clock()));return;}
 (void)portable_file_browser_setup_service();
 if(fsh_owner(NULL)&&!portable_file_browser_cleanup_only())fsh_setup_display_sync();
}
static void fsh_setup_start(void){
 if(fsh_phase!=FSH_ACTIVE||fsh_setup_live())return;
 if(!(fsh_radio_flags&PORTABLE_RADIO_BLUETOOTH)){fsh_error("Enable Bluetooth in Quick Actions first");return;}
 if(!portable_file_browser_setup_service_supported()){fsh_error("Bluetooth setup needs a cooperative display");return;}
 uint64_t now=fsh_clock();
 if((fsh_status.state!=PORTABLE_FILE_SHARING_LISTENING&&fsh_status.state!=PORTABLE_FILE_SHARING_SERVING)||fsh_status.expires_ms<=now||!fsh_status.password[0])return;
 if(!fsh_setup_initialized){
  if(!portable_file_setup_init(&fsh_setup,fb_runtime,fsh_owner,NULL)){fsh_retain();return;}
  fsh_setup_initialized=true;
 }
 risc_bluetooth_session_descriptor_v1 descriptor={.struct_size=sizeof(descriptor),.transport=RISC_SESSION_HTTP,.remaining_lifetime_ms=(uint32_t)(fsh_status.expires_ms-now)};
 snprintf(descriptor.url,sizeof(descriptor.url),"%s",fsh_url);
 snprintf(descriptor.username,sizeof(descriptor.username),"%s",fsh_status.username);
 snprintf(descriptor.password,sizeof(descriptor.password),"%s",fsh_status.password);
 snprintf(descriptor.session_id,sizeof(descriptor.session_id),"files-%llu",(unsigned long long)fsh_status.expires_ms);
 const portable_file_setup_config config={.instance_id=PORTABLE_FILE_SHARING_SETUP_INSTANCE,.setup_lifetime_ms=FB_SHARE_LIFETIME_MS,.name="RiscRTE Files"};
 int rc=portable_file_setup_start(&fsh_setup,&config,&descriptor,now);
 fsh_setup_wipe(&descriptor,sizeof(descriptor));
 if(!fsh_setup_result(rc))return;
 if(rc<0){fsh_error("Bluetooth setup could not start");return;}
 fsh_setup_screen=true;fsh_setup_leaving=fsh_setup_stopping=false;fsh_setup_polled=now;
 fsh_setup_display_generation=fsh_setup_pending_generation=0;fsh_message[0]=0;fb_dirty=true;
}
static void fsh_setup_choice(bool accept){
 fsh_setup_display_sync();
 if(!fsh_setup_display_generation||!portable_file_browser_setup_frame_ready()||fsh_setup_display_generation!=fsh_setup_status.pairing_generation||fsh_setup_display_number!=fsh_setup_status.pairing_number)return;
 uint32_t generation=fsh_setup_display_generation,number=fsh_setup_display_number;
 fsh_setup_display_generation=fsh_setup_pending_generation=0;
 (void)fsh_setup_result(portable_file_setup_confirm(&fsh_setup,generation,number,accept));
 fb_dirty=true;
}
static const char *fsh_setup_label(void){
 if(fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_UNAVAILABLE)return "Bluetooth is unavailable. HTTP sharing stays active.";
 if(fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_OFF)return "Bluetooth setup ended. HTTP sharing stays active.";
 if(fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_STARTING)return "Starting Bluetooth setup...";
 if(fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_ENDING||fsh_setup_stopping)return "Closing Bluetooth setup...";
 switch(fsh_setup_status.provider_state){
 case RISC_SETUP_PAIR_CONFIRM:return "Compare this number on both devices. Match only if they agree.";
 case RISC_SETUP_READY:return "Secure setup connected. Open the shared URL in your WebDAV client.";
 case RISC_SETUP_CONNECTED:return "Connected. Waiting for secure pairing...";
 default:return "Choose RiscRTE Files in the setup client on your other device.";
 }
}
static void fsh_setup_draw(void){
 if(portable_file_browser_cleanup_only())return;
 if(fb_paper&&!paper_frame_ready())return;
 const portable_file_setup_status frame=fsh_setup_status;
 bool pairing=frame.state==PORTABLE_FILE_SETUP_STATE_ACTIVE&&frame.provider_state==RISC_SETUP_PAIR_CONFIRM&&frame.pairing_generation;
 fb_dirty=false; /* Checkpoint callbacks may request a newer frame while this one renders. */
 char number[8];snprintf(number,sizeof(number),"%06u",frame.pairing_number);
 fsh_setup_display_generation=fsh_setup_pending_generation=0;
 if(fb_paper){
  fb_paper->begin();fbp_text(32,28,fbp_w()-64,"BLUETOOTH SETUP",true,true,false);fb_app->fill_rect(32,100,fbp_w()-64,3,true);
  fbp_wrap(32,140,fbp_w()-64,5,fsh_setup_label(),true);
  if(pairing){fbp_text((fbp_w()-fb_paper->measure(number,true))/2,316,fbp_w()-64,number,true,true,true);int half=(fbp_w()-80)/2;fbp_button(32,384,half,88,"REJECT",NULL,false);fbp_button(fbp_w()/2+8,384,half,88,"MATCH",NULL,true);}
  fbp_wrap(32,fbp_h()-260,fbp_w()-64,3,"BLE shares this temporary session only. HTTP file contents remain unencrypted.",true);
  fbp_wrap(32,fbp_h()-176,fbp_w()-64,2,fsh_message,true);
  fbp_button(32,fbp_h()-112,fbp_w()-64,88,"BACK",NULL,false);
 }else{
  portable_nova_begin();portable_nova_header("BLUETOOTH SETUP");
  portable_nova_wrap(3,16,49,208,13,4,fsh_setup_label(),NOVA_TEXT);
  if(pairing){portable_nova_center(0,16,104,208,number,NOVA_WHITE);portable_nova_button(16,130,96,44,"Reject",false);portable_nova_button(120,130,104,44,"Match",true);}
  portable_nova_wrap(3,16,174,208,10,2,"Temporary BLE setup. HTTP files are unencrypted.",NOVA_CAP);
  portable_nova_button(16,196,208,36,"Back",false);
 }
 fb_app->present(false);
 if(pairing){fsh_setup_pending_generation=frame.pairing_generation;fsh_setup_pending_number=frame.pairing_number;}
 fsh_setup_display_sync();
}
static bool fsh_setup_touch(int x,int y){
 if(portable_file_browser_cleanup_only())return false;
 if(fb_paper){
  if(x>=32&&x<fbp_w()-32&&y>=384&&y<472){if(x<fbp_w()/2-8)fsh_setup_choice(false);else if(x>=fbp_w()/2+8)fsh_setup_choice(true);return false;}
  if(x<32||x>=fbp_w()-32||y<fbp_h()-112||y>=fbp_h()-24)return false;
 }else{
  if(y>=130&&y<174){if(x>=16&&x<112)fsh_setup_choice(false);else if(x>=120&&x<224)fsh_setup_choice(true);return false;}
  if(!((x<52&&y<44)||(x>=16&&x<224&&y>=196&&y<232)))return false;
 }
 fsh_setup_leaving=fsh_setup_stopping=true;
 if(fsh_setup_close()){fsh_setup_screen=false;fsh_setup_leaving=false;}
 fb_dirty=true;return false;
}
