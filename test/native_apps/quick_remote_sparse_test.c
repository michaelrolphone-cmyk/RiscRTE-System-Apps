/* Actual sparse adapter guard, with the qualified Quick fixture's endpoint doubles. */
#define main quick_adapter_baseline_main
#include "quick_remote_adapter_test.c"
#undef main
static unsigned refusal;
static bool absent(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 (void)n;(void)v;(void)i;fixture.acquires++;
 if(refusal==1)g->api=&sensor_api;
 if(refusal==2)g->slot=1;
 if(refusal==3)g->generation=1;
 if(refusal==4)g->struct_size--;
 if(refusal==5)invocation_live=false;
 return false;
}
int main(void){
 unsigned cases=0;
 for(refusal=0;refusal<=5;refusal++){
  reset_fixture();fake_runtime.acquire=absent;desk_runtime=&fake_runtime;desk_phase=DESK_TIMER;
  risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
  assert(!desk_acquire(RISC_BLUETOOTH_SENSORS_CAPABILITY,1,0,&g));
  assert(fixture.acquires==1);
  if(!refusal)assert(!native_sleep_retained&&!native_custody_retained&&!failed&&!fixture.retains);
  else if(refusal==5)assert(native_sleep_retained&&native_custody_retained&&failed&&!fixture.retains);
  else assert(native_sleep_retained&&native_custody_retained&&failed&&fixture.retains==1);
  unsigned calls=provider_calls();if(refusal)assert(!desk_acquire(RISC_BLUETOOTH_SENSORS_CAPABILITY,1,0,&g)&&provider_calls()==calls);
  cases++;
 }
 for(unsigned bad=0;bad<3;bad++){
  reset_fixture();refusal=0;fake_runtime.acquire=absent;desk_runtime=&fake_runtime;desk_phase=DESK_TIMER;
  risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
  assert(!desk_acquire(bad==2?"other.required":RISC_BLUETOOTH_SENSORS_CAPABILITY,bad==0?2:1,bad==1?1:0,&g));
  assert(native_sleep_retained&&fixture.retains==1);cases++;
 }
 reset_fixture();refusal=0;fake_runtime.acquire=absent;desk_runtime=&fake_runtime;desk_phase=DESK_TIMER;
 risc_runtime_api_v1 guarded=fake_runtime;guarded.acquire=desk_acquire;quick_remote_bind();
 assert(!pqb_start(&quick_remote,&guarded,true)&&quick_remote.status==PQB_UNAVAILABLE);
 assert(!quick_remote.retained&&!failed&&!native_sleep_retained&&!quick_remote.token&&!quick_remote.grant_owned);cases++;
 printf("Actual sparse receiver guard: %u cases passed; no physical endpoints\n",cases);return 0;
}

bool portable_desk_clock_time(uint8_t*h,uint8_t*m){(void)h;(void)m;return false;}
