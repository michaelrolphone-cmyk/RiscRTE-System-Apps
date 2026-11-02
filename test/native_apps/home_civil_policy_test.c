/* Actual provider-policy consumer and actual native client, controlled tables. */
#define main original_client_tests
#include "portable_realtime_client_test.c"
#undef main
#include "RiscCivilClockV1.h"
static bool desk_promoted=true,desk_retained,civil_owned,civil_absent,civil_short,civil_release_fail;
static unsigned civil_acquires,civil_releases,interpret_calls,project_calls;
static int policy_status;static int64_t policy_offset;static bool malformed_project;
static portable_realtime_reconcile_state desk_rtc_check;
static int desk_fence(void){desk_retained=true;f.phase=PORTABLE_REALTIME_GUARD_RETAINED;return -2;}
static int desk_phase_guard(void*p){(void)p;return desk_retained?PORTABLE_REALTIME_GUARD_RETAINED:f.phase;}
static bool desk_halted(int s){return s==PORTABLE_REALTIME_CONTEXT||s==PORTABLE_REALTIME_UNCERTAIN||s==PORTABLE_REALTIME_RETAINED;}
static bool portable_app_sleep_retained(void){return desk_retained;}
static risc_civil_clock_retained_v1 civil_api;
static bool civil_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 assert(!desk_retained&&desk_promoted&&v==1&&!i&&!strcmp(n,RISC_CIVIL_CLOCK_CAPABILITY));civil_acquires++;
 if(civil_absent)return false;
 civil_owned=true;civil_api.base.base.struct_size=civil_short?RISC_CIVIL_CLOCK_BASE_SIZE:sizeof(civil_api);
 g->api=&civil_api;g->slot=3;g->generation=1;return true;
}
static bool civil_release(risc_runtime_capability_v1*g){
 assert(!desk_retained&&civil_owned&&g->slot==3);civil_releases++;
 if(civil_release_fail)return false;
 civil_owned=false;g->api=NULL;g->slot=g->generation=0;return true;
}
static const risc_runtime_api_v1 civil_runtime={.api_version=1,.struct_size=sizeof(civil_runtime),.acquire=civil_acquire,.release=civil_release};
static const risc_runtime_api_v1 *rt=&civil_runtime;
#include "../../Apps/paper_home_civil.inc"
static int32_t convert(void*p,const risc_civil_calendar_v1*raw,int64_t*out){
 (void)p;assert(civil_owned&&!desk_retained);interpret_calls++;
 if(policy_status)return policy_status;
 portable_timezone_civil value={raw->year,raw->month,raw->day,raw->hour,raw->minute,raw->second,raw->weekday};
 int64_t epoch;if(portable_timezone_civil_to_epoch(&value,&epoch))return -1;
 *out=epoch-policy_offset;return 0;
}
static int32_t project(void*p,int64_t epoch,uint32_t ns,risc_civil_calendar_v1*out){
 (void)p;assert(civil_owned&&!desk_retained);project_calls++;
 if(policy_status)return policy_status;
 portable_timezone_civil value;if(portable_timezone_epoch_to_civil(epoch,&value))return -1;
 *out=(risc_civil_calendar_v1){.struct_size=sizeof(*out),.year=value.year,.month=value.month,.day=value.day,
  .hour=value.hour,.minute=value.minute,.second=value.second,.weekday=value.weekday,.nanoseconds=ns,.zone="UTC"};
 if(malformed_project)out->weekday=7;
 return 0;
}
static int32_t inverse(void*p,const risc_civil_calendar_v1*c,risc_civil_inverse_v1*out){(void)p;(void)c;(void)out;assert(0);return -1;}
static int32_t cached(void*p,char*out,uint32_t size){
 (void)p;assert(civil_owned&&size==40);if(policy_status)return policy_status;
 memcpy(out,"@2007:7,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0",40);return 0;
}
static void policy_reset(void){
 reset();desk_promoted=true;desk_retained=civil_owned=civil_absent=civil_short=civil_release_fail=false;
 civil_acquires=civil_releases=interpret_calls=project_calls=0;policy_status=0;policy_offset=0;malformed_project=false;
 desk_rtc_check=(portable_realtime_reconcile_state){0};
 civil_api=(risc_civil_clock_retained_v1){{{1,sizeof(civil_api),NULL,NULL,1,RISC_CIVIL_CLOCK_POLICY_TAG,project,convert},1,RISC_CIVIL_CLOCK_SCHEDULE_TAG,inverse},1,RISC_CIVIL_CLOCK_RETAINED_TAG,cached};
}
static void raw_epoch(int64_t epoch){
 portable_timezone_civil value;assert(!portable_timezone_epoch_to_civil(epoch,&value));
 f.calendar=(twatch_rtc_time_v1){(uint16_t)value.year,value.month,value.day,value.weekday,value.hour,value.minute,value.second};
}
int main(void){
 original_client_tests();const int64_t epoch=1767225600;int offsets[]={0,28800,-18000};
 for(unsigned i=0;i<3;i++)for(unsigned cold=0;cold<2;cold++)for(int drift=0;drift<=300;drift+=300){
  policy_reset();policy_offset=offsets[i];raw_epoch(epoch+policy_offset);f.snapshot.epoch_seconds=epoch+drift;
  if(cold)unset();
  portable_realtime_client c={0};assert(!open_client(&c,1,1));portable_realtime_recovery_result result;
  int got=desk_civil_reconcile(&c,&result);
  assert(got==((cold||drift)?PORTABLE_REALTIME_RECOVERED:PORTABLE_REALTIME_OK));
  assert(result.snapshot.epoch_seconds==epoch&&f.seeds==(cold||drift?1u:0u)&&f.rtc_releases==1);
  assert(!civil_owned&&civil_acquires==civil_releases&&interpret_calls==1);
  assert(!portable_realtime_close(&c));
  risc_civil_calendar_v1 local={0};assert(!desk_civil_project(epoch,123456000,&local));
  assert(local.year==2026&&local.hour==0&&local.nanoseconds==123456000&&!strcmp(local.zone,"UTC"));
 }
 policy_reset();portable_realtime_client c={0};assert(!open_client(&c,1,1));portable_realtime_recovery_result result;
 assert(!desk_civil_reconcile(&c,&result));
 for(unsigned n=1;n<600;n++){f.event_count=0;f.snapshot.monotonic_before_us=100+n*100000;f.snapshot.monotonic_after_us=150+n*100000;assert(!desk_civil_reconcile(&c,&result));}
 assert(interpret_calls==1&&f.rtc_reads==1&&!f.seeds);assert(!portable_realtime_close(&c));
 for(unsigned absent=0;absent<2;absent++){
  policy_reset();civil_absent=absent!=0;civil_short=!absent;assert(!open_client(&c,1,1));
  assert(desk_civil_reconcile(&c,&result)==PORTABLE_REALTIME_UNAVAILABLE&&!f.rtc_reads&&!f.seeds);
  assert(!civil_owned&&civil_releases==(!absent?1u:0u));assert(!portable_realtime_close(&c));
 }
 int errors[]={-1,-9,7,-77};
 for(unsigned i=0;i<4;i++){
  policy_reset();policy_status=errors[i];c=(portable_realtime_client){0};assert(!open_client(&c,1,1));
  int got=desk_civil_reconcile(&c,&result);assert(!f.seeds);
  if(errors[i]==-1){assert(got==PORTABLE_REALTIME_UNUSABLE&&f.rtc_releases==1&&civil_releases==1);assert(!portable_realtime_close(&c));}
  else{assert(got==PORTABLE_REALTIME_RETAINED&&desk_retained&&!f.releases&&!civil_releases);assert_frozen(&c,PORTABLE_REALTIME_RETAINED);}
 }
 policy_reset();civil_release_fail=true;c=(portable_realtime_client){0};assert(!open_client(&c,1,1));
 assert(desk_civil_reconcile(&c,&result)==PORTABLE_REALTIME_RETAINED&&desk_retained&&civil_owned&&f.rtc_releases==1);
 policy_reset();desk_promoted=false;risc_civil_calendar_v1 untouched={.year=77};
 assert(desk_civil_project(epoch,0,&untouched)==PORTABLE_REALTIME_DENIED&&untouched.year==77&&!civil_acquires&&!f.rtc_acquires);
 policy_reset();malformed_project=true;untouched=(risc_civil_calendar_v1){.year=77};
 assert(desk_civil_project(epoch,0,&untouched)==PORTABLE_REALTIME_MALFORMED&&untouched.year==77&&!civil_owned&&civil_releases==1);
 policy_reset();policy_status=-9;
 assert(desk_civil_project(epoch,0,&untouched)==PORTABLE_REALTIME_RETAINED&&desk_retained&&civil_owned&&!civil_releases);
 policy_reset();char token[40]={0};
 assert(!desk_civil_cached_rule(token)&&!strcmp(token,"@2007:7,6;M4.1.0,M10.5.0;M3.2.0,M11.1.0"));
 portable_timezone_rule rule;assert(!portable_timezone_resolve(token,sizeof(token),&rule));
 portable_timezone_civil local;assert(!portable_timezone_utc_to_local(&rule,INT64_C(954615600),&local,NULL));
 assert(!civil_owned&&civil_acquires==civil_releases);
 puts("Shared Home foreground civil policy: installed RTC conversion, exact native cadence/seed, no TIMER binding, optional absence and terminal custody PASS");
 return 0;
}
