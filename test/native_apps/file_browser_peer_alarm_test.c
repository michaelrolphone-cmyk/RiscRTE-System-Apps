/* Actual Files shared adapter and Nearby controller against the production
 * tagged native-UTC alarm provider. Only native radios, storage and time are
 * doubles. Busy native work must defer the alarm without manufacturing a
 * storage fault or crossing a lease over a yield. */
#define FILE_PEER_FIXTURE
#define FILE_SETUP_TEST
#define PORTABLE_ALARM_CLIENT
#define ALARM_SERVICE_TAGGED_V2
#include "file_browser_sharing_test.c"
extern void peer_resident_begin(bool),peer_backend_tick(uint64_t),peer_backend_end(void);
extern const void *peer_resident_capability(const char *);
extern unsigned peer_resident_begins(void);
extern bool peer_resident_native_busy(void),peer_resident_lease_live(void);
extern void peer_resident_connect(void),peer_resident_quiesce(void),peer_resident_hold_end(bool),peer_resident_retain(void),peer_resident_worker_busy(bool);
extern const alarm_service_v1 *peer_alarm_start(void);
extern void peer_alarm_time(uint64_t),peer_alarm_busy(bool);
extern unsigned peer_alarm_reads(void),peer_alarm_rtc_reads(void),peer_alarm_unleased_reads(void);
static const alarm_service_v1 *real_alarm;
const void *peer_fixture_capability(const char *name){return peer_resident_capability(name);}
static bool acquire_real_alarm(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(version==2&&!instance);out->api=real_alarm;++test_grants;return true;}
 return get_cap(name,version,instance,out);
}
static void checked_yield(uint32_t n){assert(!peer_resident_lease_live());test_yield(n);}
static void tick_peer(void){++test_ticks;fsh_tick();peer_backend_tick(fsh_now);peer_alarm_time(test_ticks);}
static void frame_peer(void){if(fb_dirty)fb_draw();for(unsigned i=0;i<12;i++){t5_app_input_t in={0};assert(fb_app->poll(&in,1));if(portable_file_browser_setup_frame_ready())return;}assert(false);}
static void until_peer(unsigned state){for(unsigned i=0;i<30000;++i){tick_peer();if(fpeer_status.state==state)return;assert(!fb_terminal);}assert(false);}
int main(int argc,char **argv){
 assert(argc==2);bool reproduce=!strcmp(argv[1],"reproduce"),borrowed_case=!strcmp(argv[1],"borrowed");
 peer_resident_begin(!borrowed_case);real_alarm=peer_alarm_start();runtime=test_runtime;runtime.acquire=acquire_real_alarm;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.yield_ms=checked_yield;test_runtime_override=&runtime;
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());
 fb_option(7);until_peer(PEER_DISCOVERING);tick_peer();frame_peer();fpeer_choice=0;fpeer_action();until_peer(PEER_PAIRING);frame_peer();fpeer_choice=1;fpeer_action();
 if(borrowed_case)until_peer(PEER_READY);
 else {while(!peer_resident_begins()){tick_peer();assert(!fb_terminal);}assert(peer_resident_native_busy());}
 assert(!fsh_network_initialized);
 if(!strcmp(argv[1],"connected")){peer_resident_connect();until_peer(PEER_READY);}
 /* Force a fresh production alarm storage phase to overlap the native JOIN.
  * The real adapter should defer that phase when the peer's lease is BUSY. */
 peer_resident_worker_busy(true);peer_alarm_busy(true);assert(alarm_refresh());unsigned before=peer_alarm_reads(),rtc_before=peer_alarm_rtc_reads();
 assert(alarm_audio_pump());
 if(reproduce){
  printf("REPRODUCED: peer JOIN active=%d, server initialized=%d, alarm deferred=%d, storage reads=%u, RTC reads=%u, alarm state=%u error=%d\n",peer_resident_native_busy(),fsh_network_initialized,alarm_service_deferred,peer_alarm_reads()-before,peer_alarm_rtc_reads()-rtc_before,alarms.status.state,alarms.status.error);
  assert(!alarm_service_deferred&&peer_alarm_reads()>before&&peer_alarm_rtc_reads()==rtc_before&&alarms.status.state==ALARM_STATE_BLOCKED&&alarms.status.error==ALARM_STORAGE);return 0;
 }
 assert(alarm_service_deferred&&peer_alarm_reads()==before&&!native_custody_retained&&!failed);
 peer_resident_worker_busy(false);peer_alarm_busy(false);assert(alarm_audio_pump());
 assert(!alarm_service_deferred&&!peer_resident_lease_live());
 for(unsigned i=0;i<100&&alarms.status.state!=ALARM_STATE_READY;++i)assert(alarm_audio_pump());
 assert(alarms.status.state==ALARM_STATE_READY&&alarms.status.error==ALARM_OK&&!peer_alarm_unleased_reads());
 if(!strcmp(argv[1],"lease-end")){
  peer_resident_hold_end(true);assert(alarm_refresh());assert(peer_resident_lease_live());before=peer_alarm_reads();
  for(unsigned i=0;i<10;++i){assert(alarm_audio_pump()&&alarm_service_deferred&&peer_alarm_reads()==before);tick_peer();}
  peer_resident_hold_end(false);assert(alarm_audio_pump()&&alarm_service_deferred&&!peer_resident_lease_live()&&peer_alarm_reads()==before);
  assert(alarm_audio_pump()&&!alarm_service_deferred&&peer_alarm_reads()==before+1);
 }
 if(!strcmp(argv[1],"retained-begin")||!strcmp(argv[1],"retained-end")){
  if(!strcmp(argv[1],"retained-end")){peer_resident_hold_end(true);assert(alarm_refresh()&&peer_resident_lease_live());}
  before=peer_alarm_reads();peer_resident_retain();assert(!alarm_audio_pump());
  assert(native_custody_retained&&fb_terminal&&retains==1&&peer_alarm_reads()==before);
  assert(!alarm_audio_pump()&&peer_alarm_reads()==before);printf("Peer %s fences real alarm provider PASS\n",argv[1]);return 0;
 }
 peer_resident_connect();until_peer(PEER_READY);assert(fpeer_status.count>0);
 peer_resident_quiesce();(void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;++i)tick_peer();
 assert(fsh_phase==FSH_OFF&&!fpeer&&!peer_resident_native_busy()&&!retains);assert(portable_file_browser_close());app_module_fini();peer_backend_end();
 printf("Peer %s defers real alarm storage; clean retry and directory listing PASS\n",argv[1]);return 0;
}
