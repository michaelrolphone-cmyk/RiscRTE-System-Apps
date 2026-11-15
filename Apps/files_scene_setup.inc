/* Extracted domain from file_browser_setup.inc; semantic UI/lifecycle supplied by files_scene.c. */
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
 if(next.state!=fsh_setup_status.state||next.provider_state!=fsh_setup_status.provider_state||next.error!=fsh_setup_status.error){
  char line[192];
  snprintf(line,sizeof(line),"FILES t_ms=%llu role=share event=broker state=%u broker=%u result=%d error=%d",(unsigned long long)fsh_now,(unsigned)next.state,(unsigned)next.provider_state,rc,(int)next.error);
  if(fsh_owner(NULL)&&fb_runtime->diagnostic)(void)fb_runtime->diagnostic(line);
 }
 if(next.state!=fsh_setup_status.state||next.provider_state!=fsh_setup_status.provider_state||next.flags!=fsh_setup_status.flags||next.pairing_generation!=fsh_setup_status.pairing_generation||next.pairing_number!=fsh_setup_status.pairing_number||next.error!=fsh_setup_status.error)fb_dirty=true;
 if(next.pairing_generation!=fsh_setup_status.pairing_generation||next.pairing_number!=fsh_setup_status.pairing_number){fsh_setup_display_generation=0;fsh_setup_pending_generation=0;}
 fsh_setup_status=next;return true;
}
bool portable_file_browser_setup_service(void){
 if(!fsh_owner(NULL)){
  /* A successful file.open dispatch closes Files, then app_main still drains
   * its last frame. Completed setup owns no work; terminal is not retention.
   * Never relax the owner fence for live, pending or retained setup. */
  return fb_terminal&&!portable_adapter_retained()&&fsh_phase==FSH_OFF&&
   (!fsh_setup_initialized||fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_OFF||
    fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_UNAVAILABLE);
 }
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
 fsh_trace(accept?"confirm":"reject",0);
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


