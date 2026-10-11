/* Production controller/adapter checkpoint seam. Peripherals and resident
 * dispatch are synthetic; active ownership must never reach Runtime's exit
 * barrier. Companion native async tests exercise the actual worker/provider. */
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
#include "wifi_resident_seam.inc"
static void responsive(void){
 unsigned before=poll_count,frames=presents,dispatches=checkpoints;
 for(unsigned i=0;i<10;++i){
  ticks+=25;t5_app_input_t input={0};assert(wa->poll(&input,1));
  resident_fixture_busy();wifi_make_view();portable_wifi_render(&view);
  assert(!surface.frame&&!cleanup_pending);
 }
 assert(poll_count>before&&presents>frames&&checkpoints==dispatches);
}
int main(int argc,char **argv){
 (void)argv;
 workflow_start(true);draft();resident_fixture_start();
 for(unsigned cycle=0;cycle<4;++cycle){
  wifi_activate(3);assert(wifi_operation&&scanning);responsive();
  if(cycle==0){ /* Copied results still own native scan resources. */
   progress.phase=RISC_RADIO_RESULTS;progress.scan.state=GARDEN_RADIO_SCAN_DONE;
   progress.scan.count=1;strcpy(progress.scan.entries[0].ssid,"Synthetic AP");
   progress.scan.entries[0].auth=GARDEN_RADIO_AUTH_OPEN;tick(250);
   assert(!scanning&&scan_result.count==1);responsive();
  }
  if(cycle==1){ /* Timed out scan waits for confirmed cleanup. */
   tick(15000);assert(portable_wifi_stop_pending());responsive();
  }
  if(cycle==2){ /* Worker setup failure has a quiescent terminal result. */
   progress.failure=RISC_RADIO_FAILURE_SETUP;stopped();
  }else{assert(!wifi_back());responsive();stopped();}
  if(page!=WP_ROOT)assert(!wifi_back());
  assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK);
  assert(!native_custody_retained&&!wifi_operation);
 }
 assert(checkpoints==4);
 wifi_connect();assert(wifi_operation&&joining);responsive();
 progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;progress.rssi=-40;
 tick(250);assert(link_state==WIFI_LINK_UP);responsive();
 if(argc>1){
  progress.phase=RISC_RADIO_CLEANUP_FAILED;progress.failure=RISC_RADIO_FAILURE_CLEANUP;
  
  assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY);
  assert(cleanup_pending&&wifi_operation&&checkpoints==4);
  puts("Wi-Fi retained cleanup remains owned PASS");return 0;
 }
 assert(!wifi_disconnect());responsive();stopped();
 assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK&&checkpoints==5);
 finish();
 puts("Wi-Fi resident checkpoint: delayed scan, results, timeout, setup failure, cancellation/reentry and input/render PASS");
}
