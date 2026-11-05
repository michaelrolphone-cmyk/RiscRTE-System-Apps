/* Actual Files controller, peer/network/WebDAV implementation and resident
 * checkpoint. Radio/transport and the final Runtime dispatch are synthetic.
 * The dispatch models CpuPort::appExitSafe's radio/lease/TCP idle requirement. */
#define FILE_PEER_FIXTURE
#define FILE_SETUP_TEST
#include "file_browser_sharing_test.c"
extern void peer_resident_begin(bool),peer_backend_tick(uint64_t),peer_backend_end(void);
extern const void*peer_resident_capability(const char*);
extern unsigned peer_resident_begins(void),peer_resident_cancels(void),peer_resident_acquires(void);
extern bool peer_resident_native_busy(void),peer_resident_tcp_live(void),peer_resident_lease_live(void);
extern void peer_resident_connect(void),peer_resident_quiesce(void),peer_resident_hold_end(bool),peer_resident_retain(void);
const void*peer_fixture_capability(const char*n){return peer_resident_capability(n);}
/* Compile the real checkpoint against the already-included Files adapter. */
#define PORTABLE_RESIDENT_SHELL_CLIENT
#define PORTABLE_RESIDENT_POLICY
#include "RiscResidentShellV1.h"
static bool resident_activity_pending,display_settled=true,alarm_modal,native_sleep_retained;
#include "../../lib/PortableApps/src/resident_adapter.inc"
static unsigned dispatches,unsafe_dispatches;
static int32_t dispatch(uint64_t token,const risc_resident_request_v1*q,risc_resident_reply_v1*out){
 (void)token;(void)out;assert(q->reason==RISC_RESIDENT_CHECKPOINT_POLL);++dispatches;
 if(peer_resident_native_busy()){++unsafe_dispatches;return RISC_RESIDENT_RETAINED;}
 return RISC_RESIDENT_OK;
}
static void tick(void){++test_ticks;fsh_tick();peer_backend_tick(fsh_now);}
static void frame(void){if(fb_dirty)fb_draw();for(unsigned i=0;i<12;i++){t5_app_input_t input={0};assert(fb_app->poll(&input,1));if(portable_file_browser_setup_frame_ready())return;}assert(false);}
static void checkpoint_busy(void){
 assert(!surface.frame&&!paper_token&&display_settled&&!alarm_modal);
 unsigned before=dispatches;int rc=resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL);
 if(rc!=RISC_RESIDENT_BUSY)fprintf(stderr,"peer checkpoint escaped: rc=%d native_busy=%d dispatches=%u unsafe=%u server_network=%d\n",rc,peer_resident_native_busy(),dispatches,unsafe_dispatches,fsh_network_initialized);
 assert(rc==RISC_RESIDENT_BUSY&&dispatches==before&&!native_custody_retained);
}
static void checkpoint_idle(void){unsigned before=dispatches;assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK&&dispatches==before+1&&!unsafe_dispatches);}
static void until(unsigned state){for(unsigned i=0;i<30000;++i){tick();if(fpeer_status.state==state)return;assert(!fb_terminal);}assert(false);}
static void start(bool direct){
 peer_resident_begin(direct);runtime=test_runtime;runtime.acquire=get_cap;runtime.release=release_cap;runtime.retain_invocation=retain_owner;test_runtime_override=&runtime;
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());resident_ready=true;resident.checkpoint=dispatch;resident.invocation=1;
 fb_option(7);until(PEER_DISCOVERING);tick();frame();checkpoint_idle();
 fpeer_choice=0;fpeer_action();until(PEER_PAIRING);frame();fpeer_choice=1;fpeer_action();
}
static void finish(void){
 peer_resident_quiesce();(void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;++i)tick();
 assert(fsh_phase==FSH_OFF&&!fpeer&&!peer_resident_native_busy()&&!retains);checkpoint_idle();
 assert(portable_file_browser_close());app_module_fini();peer_backend_end();
}
int main(int argc,char**argv){
 assert(argc>=2);const char*mode=argv[1];bool direct=strncmp(mode,"borrowed",8)!=0;bool holdLease=!strcmp(mode,"lease")||!strcmp(mode,"borrowed-lease");start(direct);
 if(direct){
  bool sawStartup=false;while(!peer_resident_begins()){unsigned acquired=peer_resident_acquires();tick();assert(!fb_terminal);if(acquired&&!peer_resident_begins()){checkpoint_busy();sawStartup=true;}}assert(sawStartup);assert(peer_resident_native_busy()&&!fsh_network_initialized);frame();checkpoint_busy();
  for(unsigned i=0;i<10;++i){tick();checkpoint_busy();}
  if(!strcmp(mode,"retained")){peer_resident_retain();tick();assert(fb_terminal&&native_custody_retained&&retains==1);unsigned before=dispatches;assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_RETAINED&&dispatches==before);puts("Peer retained custody blocks dispatch PASS");return 0;}
  if(!strcmp(mode,"cancel")){(void)fsh_back();while(!peer_resident_cancels()){tick();checkpoint_busy();}for(unsigned i=0;i<10;++i){tick();checkpoint_busy();}finish();puts("Peer cancellation suppresses checkpoints until quiescent PASS");return 0;}
  peer_resident_connect();
 }
 bool sawTcp=false,sawLease=false;
 if(holdLease)peer_resident_hold_end(true);
 for(unsigned i=0;i<30000&&fpeer_status.state!=PEER_READY;++i){
  tick();assert(!fb_terminal);
  if(peer_resident_tcp_live()){sawTcp=true;checkpoint_busy();}
  if(peer_resident_lease_live()){sawLease=true;checkpoint_busy();if(holdLease){for(unsigned j=0;j<10;++j){tick();checkpoint_busy();}peer_resident_hold_end(false);}}
  if(direct)checkpoint_busy();
 }
 assert(fpeer_status.state==PEER_READY&&sawTcp);
 if(holdLease)assert(sawLease);
 if(direct)checkpoint_busy();else {assert(!peer_resident_begins()&&!peer_resident_native_busy());checkpoint_idle();}
 finish();printf("Peer %s native ownership and resident checkpoints PASS\n",mode);return 0;
}
