/* Start the actual app with Bluetooth policy enabled. Exercise both native
 * ownership classes while the shared foreground provider is initially live. */
static void broadcast_wait_live(void){
 for(unsigned i=0;!broadcast_live;i++){assert(i<1000);pump(1);}
 assert(broadcast_steps&&broadcast_pauses&&!wm.retained);
}
static void broadcast_test_run(void){
 broadcast_wait_live();
 invoke(SCAN,0);assert(radio_id&&wm.operation&&!broadcast_live);
 unsigned pauses=broadcast_pauses,steps=broadcast_steps;
 pump(100);assert(pauses==broadcast_pauses&&steps==broadcast_steps);
 stop_delay=8;invoke(CANCEL,0);pump(100);assert(!radio_id);broadcast_wait_live();
 storage_test_draft();
 fixture_profile.collect_delay=1000;
 wifi_manager_test_action(SAVE_DRAFT,0);
 for(unsigned i=0;!fixture_profile.token;i++){assert(i<100);pump(1);}
 assert(storage_pending(&wm)&&!broadcast_live);
 pauses=broadcast_pauses;steps=broadcast_steps;
 pump(100);assert(pauses==broadcast_pauses&&steps==broadcast_steps);
 wifi_manager_test_action(BACK,0);assert(fixture_profile.cancellations);
 fixture_profile.collect_delay=0;pump(100);
 assert(!storage_pending(&wm));broadcast_wait_live();
 exit_app();assert(!broadcast_live);
 puts("Shared Wi-Fi telemetry pause before RF/storage, ownership deferral and quiescent resume PASS");
}
