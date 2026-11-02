/* Reuse the existing lower-clock fixtures, not a substitute policy. */
#define main civil_clock_legacy_fixture_main
#include "clock_test.c"
#undef main
#ifdef CIVIL_NATIVE_UTC
#include "PortableRtcBasis.h"
static bool basis_utc=true,basis_bad;
static int policy_get(void*c,const char*k,void*p,uint32_t capacity,uint32_t*size){
 if(strcmp(k,PORTABLE_RTC_BASIS_KEY))return get(c,k,p,capacity,size);
 assert(live&&capacity==PORTABLE_RTC_BASIS_BYTES);
 uint8_t b[PORTABLE_RTC_BASIS_BYTES]={0x52,0x54,1,0};b[3]=basis_utc;
 portable_rtc_basis_put_word(b+8,portable_rtc_basis_check(b));if(basis_bad)b[8]^=1;
 memcpy(p,b,sizeof(b));*size=sizeof(b);return 0;
}
static const risc_bound_key_value_v1 policy_kv={1,sizeof(policy_kv),NULL,policy_get,NULL};
#endif
static int64_t epoch_at(int year,int month,int day,int hour,int minute,int second){
 portable_timezone_civil civil={year,(uint8_t)month,(uint8_t)day,(uint8_t)hour,(uint8_t)minute,(uint8_t)second,0};
 int64_t epoch;assert(!portable_timezone_civil_to_epoch(&civil,&epoch));return epoch;
}
int main(void){
 const risc_driver_v2*driver=t5_driver_get(2);
#ifdef CIVIL_NATIVE_UTC
 risc_provider_dependency_v1 deps[]={{"platform.realtime",1,&clock_api},{"storage.key-value.bound",1,&policy_kv}};
#else
 risc_provider_dependency_v1 deps[]={{"rtc.clock",2,&clock_api}};
#endif
 assert(driver->start(deps,sizeof(deps)/sizeof(*deps)));
 const risc_civil_clock_api_v1*base=driver->capability;
 const risc_civil_clock_policy_v1*policy=risc_civil_clock_get_policy(base,base->struct_size);assert(policy);
 assert(!risc_civil_clock_get_policy(base,sizeof(*base)));
 const risc_civil_clock_schedule_v1 *schedule=risc_civil_clock_get_schedule(base,base->struct_size);assert(schedule);
 risc_civil_clock_policy_v1 old_prefix=*policy;old_prefix.struct_size=sizeof(old_prefix);
 assert(risc_civil_clock_get_policy(&old_prefix,sizeof(old_prefix)));
 assert(!risc_civil_clock_get_schedule(&old_prefix,sizeof(old_prefix)));
 risc_civil_clock_schedule_v1 short_schedule=*schedule;short_schedule.schedule_tag=0;
 assert(!risc_civil_clock_get_schedule(&short_schedule,sizeof(short_schedule)));
 risc_civil_clock_policy_v1 malformed=*policy;malformed.policy_tag=0;
 assert(!risc_civil_clock_get_policy(&malformed,sizeof(malformed)));
 bad=true; /* Projection/interpretation must not read either native or RTC clock. */
 int64_t epoch=epoch_at(2026,10,10,18,34,56);
 risc_civil_calendar_v1 result={.struct_size=sizeof(result)};
 assert(!policy->project(NULL,epoch,123456789,&result));
 assert(result.year==2026&&result.month==10&&result.day==10&&result.hour==12&&result.minute==34&&result.second==56&&result.nanoseconds==123456789&&!strcmp(result.zone,"America/Denver"));
 risc_civil_calendar_v1 unchanged=result;
 assert(policy->project(NULL,epoch,1000000000,&result)==-1&&!memcmp(&result,&unchanged,sizeof(result)));
 const unsigned transitions[][7]={{3,8,8,59,59,1,59},{3,8,9,0,0,3,0},{11,1,7,59,59,1,59},{11,1,8,0,0,1,0}};
 for(unsigned i=0;i<4;i++){
  const unsigned*t=transitions[i];epoch=epoch_at(2026,t[0],t[1],t[2],t[3],t[4]);
  assert(!policy->project(NULL,epoch,999999999,&result));
  assert(result.hour==t[5]&&result.minute==t[6]&&result.second==t[4]&&result.nanoseconds==999999999);
 }
 risc_civil_calendar_v1 raw={.struct_size=sizeof(raw),.year=2026,.month=10,.day=10,.hour=18,.minute=34,.second=56};
#ifndef CIVIL_NATIVE_UTC
 raw.day=11;raw.hour=2;
#endif
 int64_t interpreted=-123;
 assert(!policy->interpret_rtc(NULL,&raw,&interpreted)&&interpreted==epoch_at(2026,10,10,18,34,56));
 raw.nanoseconds=1;assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-1);raw.nanoseconds=0;
#ifdef CIVIL_NATIVE_UTC
 basis_utc=false;raw.day=10;raw.hour=12;
 assert(!policy->interpret_rtc(NULL,&raw,&interpreted)&&interpreted==epoch_at(2026,10,10,18,34,56));
 basis_bad=true;assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-1);basis_bad=false;
 raw.month=11;raw.day=1;raw.hour=1;raw.minute=30;raw.second=0;
 assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-1); /* no silent DST fold choice */
 raw.month=3;raw.day=8;raw.hour=2;assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-1);
#endif
#ifndef CIVIL_NATIVE_UTC
 raw=(risc_civil_calendar_v1){.struct_size=sizeof(raw),.year=2000,.month=1,.day=1};
 assert(!policy->interpret_rtc(NULL,&raw,&interpreted)&&interpreted==INT64_C(946656000));
 raw.year=2100;assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-1);
 raw.year=1999;assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-1);
#endif
 /* Provider inverse owns historical policy and never selects a fold. */
 risc_civil_inverse_v1 inverse={.struct_size=sizeof(inverse)};
 risc_civil_calendar_v1 input={.struct_size=sizeof(input),.year=2026,.month=3,.day=8,.hour=2,.minute=30};
 assert(!schedule->inverse(NULL,&input,&inverse)&&inverse.count==0);
 input.month=11;input.day=1;input.hour=1;
 assert(!schedule->inverse(NULL,&input,&inverse)&&inverse.count==2&&inverse.epoch[0]<inverse.epoch[1]);
 for(unsigned i=0;i<inverse.count;i++) {
  result.struct_size=sizeof(result);assert(!policy->project(NULL,inverse.epoch[i],0,&result));
  assert(result.year==input.year&&result.month==input.month&&result.day==input.day&&result.hour==input.hour&&result.minute==input.minute);
 }
 input=(risc_civil_calendar_v1){.struct_size=sizeof(input),.year=2000,.month=3,.day=20,.hour=12};
 assert(!schedule->inverse(NULL,&input,&inverse)&&inverse.count==1);
#ifdef CIVIL_NATIVE_UTC
 assert(inverse.epoch[0]==epoch_at(2000,3,20,18,0,0));
#else
 assert(inverse.epoch[0]==epoch_at(2000,3,20,19,0,0));
#endif
 assert(!policy->project(NULL,inverse.epoch[0],765432100,&result));
 assert(result.hour==12&&result.minute==0&&result.nanoseconds==765432100);
#ifndef CIVIL_NATIVE_UTC
 input.month=4;input.day=2;input.hour=2;input.minute=30;
 assert(!schedule->inverse(NULL,&input,&inverse)&&inverse.count==0);
 input.month=10;input.day=29;input.hour=1;
 assert(!schedule->inverse(NULL,&input,&inverse)&&inverse.count==2);
#endif
 risc_civil_inverse_v1 unchanged_inverse=inverse;
 input.month=13;assert(schedule->inverse(NULL,&input,&inverse)==-1&&!memcmp(&inverse,&unchanged_inverse,sizeof(inverse)));
 live=false;assert(policy->project(NULL,epoch,0,&result)==-9);
 assert(schedule->inverse(NULL,&input,&inverse)==-9&&!memcmp(&inverse,&unchanged_inverse,sizeof(inverse)));
 assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-9&&!driver->quiesce());
 puts("Civil policy: prefix compatibility, full fractional projection, DST edges, saved RTC basis, no clock I/O/writes and retained fence PASS");
}
