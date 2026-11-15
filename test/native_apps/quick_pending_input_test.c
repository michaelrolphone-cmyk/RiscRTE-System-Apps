/* Real adapter dispatch/reducer, deterministic raw-touch provider media. */
#define TEST_SCENE_QUICK
#define PORTABLE_QUICK_NATIVE_GEOMETRY
#define PORTABLE_QUICK_PENDING_FRAME
#define main inherited_native_scene_main
#include "native_scene_adapter_test.c"
#undef main
static unsigned barrier_mode,barrier_base,barrier_waits;
static void barrier_yield(uint32_t n) {
 yield(n);
 if(submitted>barrier_base) {
  ++barrier_waits;
  if(barrier_mode==10)navigation_pending|=T5_APP_BUTTON_BACK;
  else if(barrier_mode==11)down=true;
  else if(barrier_waits>=3)completed=submitted;
 }
}
int main(int argc,char **argv) {
 unsigned mode=argc>1?(unsigned)atoi(argv[1]):0;assert(mode<13);
 assert(app_module_init()==0);auto_complete=true;
 portable_native_scene_begin();paint(1);step();step();
 touch_x=touch_y=100;
 if(mode>=8) {
  quick_background=malloc(quick_frame_bytes());assert(quick_background);
  memcpy(quick_background,alarm_pixels,quick_frame_bytes());
  quick_modal=true;quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);
  quick.ui.gesture=PQA_IDLE;quick.ui.pending=0;quick_atomic_running=true;
  barrier_mode=mode;barrier_base=submitted;barrier_waits=0;
  auto_complete=mode==8;quick_fail_submit=mode==12;
  risc_runtime_api_v1 wrapper=api;wrapper.yield_ms=barrier_yield;rt=&wrapper;
  int result=quick_pending_frame();
  if(mode==12) {
   assert(result==PENDING_FAULT&&retained);unsigned before=calls;
   assert(quick_pending_frame()==PENDING_FAULT);app_module_fini();assert(calls==before);
   puts("PASS pending-frame actual display submit failure retains without later callbacks");return 0;
  }
  if(retained||submitted!=barrier_base+1||!quick.ui.save_pending||quick.ui.pending)fprintf(stderr,"barrier mode=%u result=%d retained=%d submitted=%u base=%u save=%d action=%u waits=%u\n",mode,result,retained,submitted,barrier_base,quick.ui.save_pending,quick.ui.pending,barrier_waits);
  assert(!retained&&submitted==barrier_base+1&&quick.ui.save_pending&&!quick.ui.pending);
  if(mode==10||mode==11)assert(result==PENDING_CANCELLED);
  else {
   assert(result==PENDING_READY&&!paper_token);
   if(mode==9)assert(barrier_waits>=3&&completed==submitted);
  }
  /* Cancellation can leave an owned pending display operation. Complete that
     real token before ordinary safe test cleanup, never after retained custody. */
  completed=submitted;assert(paper_present_progress());quick_pending_done();
  quick_atomic_running=false;quick_modal=false;app_module_fini();
  assert(!retained&&!grants&&!frames);
  printf("PASS actual pending-frame wrapper case %u; one submitted pending image\n",mode);return 0;
 }
 if(mode==2||mode==3||mode==7){down=true;step();}
 quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);
 quick.ui.gesture=PQA_IDLE;quick.ui.pending=0;
 quick_atomic_running=true;quick.ui.save_pending=true;quick.ui.save_cancelled=false;
 if(mode==1)down=true;
 if(mode==3)down=false;
 if(mode==4)bad_poll=true;
 if(mode==5)bad_next=true;
 if(mode==6)navigation_pending|=T5_APP_BUTTON_BACK;
 input_service();input_dispatch();
 if(mode==5){
  assert(retained);unsigned before=calls;
  input_service();input_dispatch();app_module_fini();assert(calls==before);
  puts("PASS pending-frame actual raw provider fault retains; no later provider callbacks");return 0;
 }
 assert(!retained && quick.ui.pending==0 && !quick_replay_delivery);
 if(mode==0)assert(!quick.ui.save_cancelled);
 else if(mode!=6)assert(quick.ui.save_cancelled);
 if(mode==6)assert(quick_pending_cancelled(NULL)==1);
 if(mode==7){
  for(unsigned i=0;i<8;i++){input_service();input_dispatch();assert(quick.ui.save_cancelled && !quick.ui.pending && !quick_replay_delivery);}
 }
 if(mode==4){assert(touch.poll_failed||touch.resync);assert(quick_pending_cancelled(NULL)==1);}
 /* This test never submits a preference. It exercises real contact routing
    while the owning operation has reserved input, not a replay of the edit. */
 quick_atomic_running=false;quick.ui.save_pending=false;bad_poll=false;
 app_module_fini();assert(!retained&&!grants&&!frames);
 printf("PASS pending-frame actual adapter input case %u; no queued action\n",mode);return 0;
}
