static void setup_draw_and_wait(void){
 for(unsigned i=0;i<40;i++){
  if(fb_dirty)fb_draw();
  if(portable_file_browser_setup_frame_ready()&&!fb_dirty){fsh_setup_display_sync();return;}
  t5_app_input_t input={0};assert(fb_app->poll(&input,1));
 }
 assert(false);
}
static bool run_setup_scene(const char *mode){
 assert(portable_file_browser_setup_service_supported());
 fsh_setup_start();assert(fsh_setup_screen&&fsh_setup_live());
 if(!strcmp(mode,"ble-cancel-start")){assert(fsh_setup_close());assert(!setup_opens);goto close_http;}
 for(unsigned i=0;i<10&&fsh_setup_status.provider_state!=RISC_SETUP_ADVERTISING;i++)step();
 assert(setup_owned&&setup_opens==1&&setup_polls&&fsh_setup_status.state==PORTABLE_FILE_SETUP_STATE_ACTIVE);
 for(size_t i=0;i<sizeof(fsh_setup.descriptor);i++)assert(!((unsigned char*)&fsh_setup.descriptor)[i]);
 if(!strcmp(mode,"ble-owner-loss")){
  setup_context=true;step();assert(portable_adapter_retained()&&fb_terminal&&retains==1);
  unsigned before=calls,old_polls=setup_polls;fsh_tick();assert(!fsh_close());assert(calls==before&&setup_polls==old_polls);
  puts("Files BLE actual frontend terminal fence PASS");return true;
 }
 setup_state=RISC_SETUP_PAIR_CONFIRM;step();assert(fsh_setup_status.pairing_generation==1);
 fsh_setup_choice(true);assert(!setup_confirms); /* Never accept an unseen comparison. */
 setup_draw_and_wait();capture_paper("ble-comparison");
 fsh_setup_display_sync();assert(fsh_setup_display_generation==1);
 if(!strcmp(mode,"ble-stale")){
  setup_stale=true;fsh_setup_choice(true);assert(setup_confirms==1&&!fsh_setup_display_generation);
  fsh_setup_choice(true);assert(setup_confirms==1);step();assert(fsh_setup_status.pairing_generation==2);
  setup_draw_and_wait();
 }
 if(!strcmp(mode,"ble-reject")){
  fsh_setup_choice(false);for(unsigned i=0;i<8&&fsh_setup_live();i++)step();assert(!setup_owned&&setup_releases==1&&listeners==1);
 }else{
  fsh_setup_choice(true);step();assert(fsh_setup_status.provider_state==RISC_SETUP_READY&&fsh_setup_status.flags==15);
  setup_draw_and_wait();capture_paper("ble-ready");
 }
 if(!strcmp(mode,"ble-back-retry")){
  setup_refuse=true;
  if(fb_paper)(void)fsh_setup_touch(100,fbp_h()-80);else (void)fsh_setup_touch(100,210);
  assert(portable_file_browser_cleanup_only()&&fsh_phase==FSH_ACTIVE);
  unsigned before=calls,old_polls=setup_polls,old_presents=test_presents;
  for(unsigned i=0;i<16;i++){t5_app_input_t input={0};assert(fb_app->poll(&input,10));assert(calls==before&&setup_polls==old_polls&&test_presents==old_presents&&fsh_phase==FSH_ACTIVE);}
  setup_refuse=false;
  for(unsigned i=0;i<8&&portable_file_browser_cleanup_only();i++){t5_app_input_t input={0};assert(fb_app->poll(&input,10));}
  assert(!portable_file_browser_cleanup_only()&&!fsh_setup_screen&&listeners==1&&fsh_phase==FSH_ACTIVE&&!cancel_calls);
 }
 if(!strcmp(mode,"ble-close-retry")){
  setup_refuse=true;assert(!fsh_close()&&portable_file_browser_cleanup_only());
  unsigned before=calls,old_polls=setup_polls,old_presents=test_presents;
  for(unsigned i=0;i<32;i++){fsh_tick();assert(portable_file_browser_cleanup_only());fb_draw();assert(calls==before&&setup_polls==old_polls&&test_presents==old_presents&&!cancel_calls);}
  assert(setup_sleeps==33);setup_refuse=false;step();assert(!setup_owned&&portable_file_browser_cleanup_only());step();assert(!portable_file_browser_cleanup_only()&&setup_releases==1);
 }
 if(!strcmp(mode,"ble-expiry")){test_ticks+=FB_SHARE_LIFETIME_MS;step();assert(fsh_phase==FSH_STOPPING);}
close_http:
 (void)fsh_close();
 for(unsigned i=0;i<20&&fsh_phase!=FSH_OFF;i++){quiescent=true;step();}
 assert(fsh_phase==FSH_OFF&&!setup_owned&&!listeners&&!owned&&!fsh_status.password[0]);
 printf("Files BLE %s: actual frontend/client lifecycle PASS\n",mode);return false;
}
