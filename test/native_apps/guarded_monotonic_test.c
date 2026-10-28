#include "RiscRuntimeClockV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const risc_runtime_api_v1 *rt,*native_custody_runtime,*desk_runtime;
static risc_runtime_api_v1 settings_runtime,desk_guarded_runtime;
static bool native_custody_retained,native_sleep_retained,allowed=true,ok=true;
static unsigned calls,health_calls,retains;static uint32_t clock_value;
static bool sample(uint32_t*out){calls++;*out=clock_value;return ok;}
static bool health(risc_runtime_health_v1*out){health_calls++;out->uptime_ms=42;return ok;}
#ifdef PORTABLE_FILE_SETUP
static bool file_setup_io_allowed(void){return allowed;}
#endif
static void portable_adapter_retain(void){assert(!native_custody_retained);native_custody_retained=true;retains++;}
static void portable_desk_adapter_retain(void){assert(!native_sleep_retained);native_sleep_retained=true;retains++;}
#define PORTABLE_ATTEMPT_BOOL(label,expression) (expression)
#define desk_acquire NULL
#define desk_release NULL
#define custody_acquire NULL
#define custody_release NULL
#define desk_guard_yield NULL
#define desk_guard_diagnostic NULL
#define desk_guard_launch NULL
#define custody_yield NULL
#define custody_diagnostic NULL
#define custody_launch NULL
#define PORTABLE_DESK_CLOCK_SPARSE_START 1
enum {DESK_STARTING=2,DESK_FAILED=5};
static bool failed;static unsigned desk_phase;
#include "production_slices.inc"
static void reset(void){calls=health_calls=retains=0;native_custody_retained=native_sleep_retained=false;allowed=ok=true;clock_value=0;}
int main(void){
 for(unsigned lane=0;lane<2;lane++)for(unsigned mode=0;mode<3;mode++){
  reset();risc_runtime_api_v1 source={.api_version=RISC_RUNTIME_API_V1,.struct_size=sizeof(source),.health=health,.monotonic_ms=sample};
  if(mode==1)source.struct_size=offsetof(risc_runtime_api_v1,monotonic_ms);
  if(mode==2)source.monotonic_ms=NULL;
  void*prefix=malloc(source.struct_size);assert(prefix);memcpy(prefix,&source,source.struct_size);rt=native_custody_runtime=desk_runtime=prefix;
  if(lane)copy_desk();else copy_custody();risc_runtime_api_v1*copy=lane?&desk_guarded_runtime:&settings_runtime;
  assert(copy->struct_size==source.struct_size);assert((copy->monotonic_ms!=NULL)==(mode==0));
  uint32_t out=99;assert(risc_runtime_monotonic_ms_v1(copy,&out));assert(out==(mode?42:0)&&calls==(mode?0:1)&&health_calls==(mode?1:0));
  if(!mode){clock_value=UINT32_MAX;assert(risc_runtime_monotonic_ms_v1(copy,&out)&&out==UINT32_MAX);ok=false;out=77;assert(!risc_runtime_monotonic_ms_v1(copy,&out)&&out==77&&retains==1&&!health_calls);}
  else{if(lane)native_sleep_retained=true;else native_custody_retained=true;}
  unsigned before=calls+health_calls;out=88;assert(!risc_runtime_monotonic_ms_v1(copy,&out)&&out==88&&before==calls+health_calls);free(prefix);
 }
 reset();risc_runtime_api_v1 source={.api_version=1,.struct_size=sizeof(source),.health=health,.monotonic_ms=sample};native_custody_runtime=desk_runtime=&source;
 assert(!custody_monotonic(NULL)&&!desk_guard_monotonic(NULL)&&!calls);
#ifdef PORTABLE_FILE_SETUP
 allowed=false;uint32_t out=55;assert(!custody_monotonic(&out)&&out==55&&!calls&&!retains);risc_runtime_health_v1 h={0};assert(!custody_health(&h)&&!health_calls);
#endif
 for(unsigned mode=0;mode<3;mode++){
  reset();source.struct_size=mode==1?offsetof(risc_runtime_api_v1,monotonic_ms):sizeof(source);source.monotonic_ms=mode==2?NULL:sample;rt=&source;failed=false;desk_phase=DESK_STARTING;clock_value=123;
  assert(millis_now()==(mode?42:123)&&!failed&&calls==(mode?0:1)&&health_calls==(mode?1:0));
  ok=false;unsigned old_health=health_calls;assert(!millis_now()&&failed);if(!mode)assert(health_calls==old_health);
  unsigned before=calls+health_calls;assert(!millis_now()&&before==calls+health_calls);
  failed=false;desk_phase=DESK_STARTING-1;assert(!millis_now()&&before==calls+health_calls);desk_phase=DESK_FAILED;assert(!millis_now()&&before==calls+health_calls);
 }
 puts("PASS exact guarded wrappers/copy slices: old-sized allocation/null suffix fallback, zero/wrap, authoritative failure retains without fallback/output change, no callbacks after retention, optional setup guard");}
