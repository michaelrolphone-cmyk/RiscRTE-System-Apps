/* Shared direct-device orchestration. No saved profile is read or written. */
static void fsh_direct_fail(const char*message){fsh_error(message);fsh_phase=FSH_STOPPING;}
// Keep the already-declared control grant pinned until broker cleanup. The
// broker records ON as its restore state; we restore the original power state
// only after its checked close. No saved Bluetooth preference is changed.
static bool fsh_direct_power_prepare(void){
 if(fsh_ble_power_ready)return true;
 if(!fsh_ble_power_grant.api){
  if(!fsh_acquire("bluetooth.hci",16,&fsh_ble_power_grant))return false;
  return false;
 }
 const portable_bluetooth_control_v1*b=fsh_ble_power_grant.api;
 if(b->api_version!=1||b->struct_size<sizeof(*b)||!b->set_enabled||!b->status){
  fsh_direct_fail("Bluetooth power control unavailable");return false;
 }
 uint8_t state=PORTABLE_BLUETOOTH_RETAINED;
 if(!b->status(b->context,&state)||!fsh_owner(NULL)||state>PORTABLE_BLUETOOTH_ON){fsh_retain();return false;}
 if(state==PORTABLE_BLUETOOTH_OFF){
  fsh_ble_restore_off=true;
  bool enabled=b->set_enabled(b->context,true);
  if(!fsh_owner(NULL)){fsh_retain();return false;}
  if(!enabled){fsh_direct_fail("Bluetooth could not start");return false;}
  state=PORTABLE_BLUETOOTH_RETAINED;
  if(!b->status(b->context,&state)||!fsh_owner(NULL)||state!=PORTABLE_BLUETOOTH_ON){fsh_retain();return false;}
 }
 fsh_ble_power_ready=true;return true;
}
static void fsh_direct_tick(uint64_t now){
 if(fsh_direct_stage<6&&now>=10000u){fsh_direct_fail("Private setup timed out");return;}
 if(!(fsh_radio_flags&PORTABLE_RADIO_BLUETOOTH)){fsh_direct_fail("Enable Bluetooth in Quick Actions first");return;}
 if(!portable_file_browser_setup_service_supported()){fsh_direct_fail("Direct setup needs a cooperative display");return;}
 if(fsh_direct_stage==0){
  if(!fsh_direct_power_prepare())return;
  if(!fsh_acquire("net.wifi",15,&fsh_wifi_grant))return;
  portable_network_session_init(&fsh_network,fsh_wifi_grant.api,NULL,fsh_owner,NULL);fsh_network_initialized=true;
  fsh_direct_stage=1;return;
 }
 if(fsh_direct_stage==1){if(fsh_acquire(RISC_ENTROPY_SOURCE_CAPABILITY,0,&fsh_entropy_grant))fsh_direct_stage=2;return;}
 if(fsh_direct_stage==2){
  const risc_entropy_source_v1*e=fsh_entropy_grant.api;unsigned char bytes[32]={0};
  if(e->api_version!=1||e->struct_size<sizeof(*e)||!e->fill){fsh_direct_fail("Private network entropy unavailable");return;}
  int rc=e->fill(e->context,bytes,sizeof(bytes));
  if(!fsh_owner(NULL)){fsh_setup_wipe(bytes,sizeof(bytes));fsh_retain();return;}
  if(rc){fsh_setup_wipe(bytes,sizeof(bytes));if(rc==RISC_ENTROPY_SOURCE_BUSY)return;if(rc==RISC_ENTROPY_SOURCE_UNAVAILABLE||rc==RISC_ENTROPY_SOURCE_INVALID)fsh_direct_fail("Private network entropy unavailable");else fsh_retain();return;}
  static const char hex[]="0123456789abcdef";memcpy(fsh_offer.ssid,"Files-",6);
  for(unsigned i=0;i<8;i++){fsh_offer.ssid[6+2*i]=hex[bytes[i]>>4];fsh_offer.ssid[7+2*i]=hex[bytes[i]&15];}
  for(unsigned i=0;i<16;i++){fsh_offer.password[2*i]=hex[bytes[8+i]>>4];fsh_offer.password[2*i+1]=hex[bytes[8+i]&15];}
  fsh_setup_wipe(bytes,sizeof(bytes));fsh_direct_stage=3;return;
 }
 if(fsh_direct_stage==3){if(fsh_release(&fsh_entropy_grant))fsh_direct_stage=4;return;}
 if(fsh_direct_stage==4){
  const portable_file_sharing_hooks hooks={NULL,fsh_owner,malloc,free};
  fsh_client=portable_file_sharing_create(fb_runtime,&hooks);
  if(!fsh_client||!portable_file_sharing_defer_network(fsh_client,true)){fsh_direct_fail("Cannot prepare private sharing");return;}
  portable_file_sharing_config config={PORTABLE_FILE_SHARING_EXPORT_INSTANCE,PORTABLE_FILE_SHARING_TCP_INSTANCE,PORTABLE_FILE_SHARING_ENTROPY_INSTANCE,{0},8080,FB_SHARE_LIFETIME_MS,true,false};
#ifdef PORTABLE_FILE_SHARING_SELECTED_VOLUME
  config.storage_volume=fsh_volume_share();config.files_instance=config.storage_volume?PORTABLE_FILE_BROWSER_INSTANCE:PORTABLE_FILE_SHARING_EXPORT_INSTANCE;
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
  if(config.storage_volume)config.files_instance=fb_root_instance(fb_storage_index);
#endif
#endif
  if(!portable_file_sharing_start(fsh_client,&config,now)){fsh_direct_fail("Cannot prepare private sharing");return;}
  fsh_phase=FSH_ACTIVE;fsh_direct_stage=5;return;
 }
 if(fsh_direct_stage==5){
  if(!portable_file_browser_services_begin())return;
  portable_file_sharing_tick(fsh_client,now);portable_file_sharing_get_status(fsh_client,&fsh_status);
  if(!portable_file_browser_services_end())return;
  if(fsh_status.state==PORTABLE_FILE_SHARING_RETAINED){fsh_retain();return;}
  if(fsh_status.state==PORTABLE_FILE_SHARING_OFF||fsh_status.state==PORTABLE_FILE_SHARING_ERROR){fsh_direct_fail("Cannot prepare private sharing");return;}
  if(!fsh_status.password[0])return;
  if(!portable_file_setup_init(&fsh_setup,fb_runtime,fsh_owner,NULL)){fsh_retain();return;}fsh_setup_initialized=true;
  fsh_offer.struct_size=sizeof(fsh_offer);fsh_offer.endpoint=(risc_bluetooth_session_descriptor_v1){.struct_size=sizeof(fsh_offer.endpoint),.transport=RISC_SESSION_HTTP,.remaining_lifetime_ms=(uint32_t)(fsh_status.expires_ms-now)};
  memcpy(fsh_offer.address,(uint8_t[]){192,168,4,1},4);
  snprintf(fsh_url,sizeof(fsh_url),"http://192.168.4.1:%u/",fsh_status.port);
  snprintf(fsh_offer.endpoint.url,sizeof(fsh_offer.endpoint.url),"%s",fsh_url);
  snprintf(fsh_offer.endpoint.username,sizeof(fsh_offer.endpoint.username),"%s",fsh_status.username);
  snprintf(fsh_offer.endpoint.password,sizeof(fsh_offer.endpoint.password),"%s",fsh_status.password);
  snprintf(fsh_offer.endpoint.session_id,sizeof(fsh_offer.endpoint.session_id),"files-%llu",(unsigned long long)fsh_status.expires_ms);
  const portable_file_setup_config config={.instance_id=PORTABLE_FILE_SHARING_SETUP_INSTANCE,.setup_lifetime_ms=FB_SHARE_LIFETIME_MS,.name="RiscRTE Files"};
  int rc=portable_file_setup_start_network(&fsh_setup,&config,&fsh_offer,now);
  fsh_setup_wipe(&fsh_offer.endpoint,sizeof(fsh_offer.endpoint));
  if(!fsh_setup_result(rc))return;
  if(rc!=PORTABLE_FILE_SETUP_PENDING){fsh_direct_fail("Private Bluetooth setup unavailable");return;}
  fsh_setup_screen=true;fsh_setup_leaving=fsh_setup_stopping=false;fsh_setup_polled=now;
  fsh_setup_display_generation=fsh_setup_pending_generation=0;fsh_direct_stage=6;fb_dirty=true;return;
 }
 if(fsh_direct_stage==6){
  if(!fsh_setup_status.network_authorized){
   if(fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_OFF||fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_UNAVAILABLE||fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_ENDING)fsh_direct_fail("Private setup declined or ended");
   return;
  }
  /* Both users have confirmed. BLE owns only its authenticated setup link. */
  int rc=portable_network_session_start_private(&fsh_network,true,fsh_offer.ssid,fsh_offer.password,(uint32_t)now);
  fsh_setup_wipe(&fsh_offer,sizeof(fsh_offer));
  if(rc==PORTABLE_NETWORK_RETAINED){fsh_retain();return;}
  if(rc!=PORTABLE_NETWORK_PENDING){fsh_direct_fail("Private Wi-Fi unavailable");return;}
  fsh_private_started=true;fsh_direct_stage=7;return;
 }
 int connected=portable_network_session_step(&fsh_network,(uint32_t)now);portable_network_session_snapshot(&fsh_network,&fsh_link);
 if(connected==PORTABLE_NETWORK_RETAINED){fsh_retain();return;}
 if(connected<0){fsh_direct_fail("Private Wi-Fi ended");return;}
 if(connected!=PORTABLE_NETWORK_READY)return;
 if(fsh_direct_stage==7){
  if(memcmp(fsh_link.ipv4.address,(uint8_t[]){192,168,4,1},4)){fsh_direct_fail("Private network address mismatch");return;}
  if(!portable_file_sharing_defer_network(fsh_client,false)){fsh_direct_fail("Private WebDAV unavailable");return;}
  fsh_setup_screen=false;fsh_direct_stage=8;fb_dirty=true;return;
 }
 if(!portable_file_browser_services_begin())return;
 portable_file_sharing_tick(fsh_client,now);portable_file_sharing_get_status(fsh_client,&fsh_status);
 if(!portable_file_browser_services_end())return;
 if(fsh_status.state==PORTABLE_FILE_SHARING_RETAINED)fsh_retain();
 else if(fsh_status.state==PORTABLE_FILE_SHARING_OFF||fsh_status.state==PORTABLE_FILE_SHARING_ERROR)fsh_direct_fail("Private sharing ended");
 uint32_t second=(uint32_t)now/1000u;if(second!=fsh_last_second){fsh_last_second=second;fb_dirty=true;}
}
