#define REFERENCE_HOME_MAIN original_shared_quick_home_main
#define SPARSE_EXPECT_LAUNCHES 0u
#include "shared_quick_home_test.c"
#undef REFERENCE_HOME_MAIN
extern void reference_finish_home(void);
extern uint32_t home_idle_last_activity(void);
extern uint32_t home_idle_delay(void);
static unsigned automatic_key_at,automatic_last_activity,automatic_delay,idle_polls;
static uint32_t wanted_seconds;
static bool timeout_navigation(void*c,risc_input_navigation_frame_v1*out){
 if(!getenv("HOME_IDLE_SECONDS"))return nav_poll(c,out);
 (void)c;safe();*out=(risc_input_navigation_frame_v1){0};++idle_polls;
 if(!lock_attempted){
  /* Advance time with no synthetic key or contact. The normal app poll owns
   * deadline evaluation and the actual native key is read only by sleep. */
  ms+=1000;lock_phase=2;lock_hold_until=0;assert(idle_polls<200);
 }else if(++lock_post_polls==14)reference_finish_home();
 return true;
}
static int32_t timeout_preferences(void*c,const char*k,void*out,uint32_t cap,uint32_t*size){
 if(getenv("HOME_IDLE_SECONDS")&&!strcmp(k,PORTABLE_SLEEP_IDLE_KEY)){
  safe();assert(cap>=5);uint8_t data[]={0x54,1,(uint8_t)wanted_seconds,(uint8_t)(wanted_seconds>>8),(uint8_t)(wanted_seconds^(wanted_seconds>>8)^0xa5u)};
  memcpy(out,data,5);*size=5;return RISC_KEY_VALUE_OK;
 }
 return preferences(c,k,out,cap,size);
}
static bool timeout_key(void*c,bool*down){
 if(getenv("HOME_IDLE_SECONDS")&&!automatic_key_at){
  automatic_key_at=ms;automatic_last_activity=home_idle_last_activity();automatic_delay=home_idle_delay();
  assert(automatic_delay==wanted_seconds*1000u);
  assert(ms-automatic_last_activity>=automatic_delay&&ms-automatic_last_activity<automatic_delay+1500);
 }
 return read_key(c,down);
}
static bool timeout_obtain(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!obtain(n,v,id,g))return false;
 if(!strcmp(n,"input.navigation")){static risc_input_navigation_api_v1 nav;nav=navigation;nav.poll=timeout_navigation;g->api=&nav;}
 if(!strcmp(n,"storage.key-value")&&id==1){static risc_key_value_v1 prefs;prefs=*(const risc_key_value_v1*)g->api;prefs.get=timeout_preferences;g->api=&prefs;}
 if(!strcmp(n,X4_POWER_CAPABILITY)){static x4_power_deep_v1 p;p=power;p.power.read_key=timeout_key;g->api=&p;}
 return true;
}
int main(int argc,char**argv){
 if(getenv("HOME_IDLE_SECONDS"))wanted_seconds=(unsigned)atoi(getenv("HOME_IDLE_SECONDS"));
 runtime.acquire=timeout_obtain;runtime.resident_shell=ref_resident;
 int result=shared_reference_base_main(argc,argv);assert(!result);
 if(!strcmp(argv[1],"terminal")) {
  assert(terminal&&entries==1&&!promoted&&!launches);
  printf("Actual Home idle retained-minute refresh: entry=%u foreground=0 PASS\n",entries);return 0;
 }
 assert(lock_attempted&&entries==1&&!launches&&lock_clean_frames);
 if(getenv("HOME_IDLE_SECONDS"))assert(automatic_key_at&&idle_polls&&!reference_opened);
 if(getenv("LOCK_RETAIN"))assert(terminal&&barriers==1);
 else if(getenv("LOCK_REFUSE"))assert(!portable_desk_clock_lock_requested()&&lock_restored_frames);
 printf("Actual Home %s: timer=%u ms first-key-delay=%u desk-clean=%u entry=%u PASS\n",getenv("HOME_IDLE_SECONDS")?"timeout":"manual lock",automatic_delay,automatic_key_at-automatic_last_activity,lock_clean_frames,entries);return 0;
}
