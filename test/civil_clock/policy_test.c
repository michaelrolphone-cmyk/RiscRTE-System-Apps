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
 live=false;assert(policy->project(NULL,epoch,0,&result)==-9);
 assert(policy->interpret_rtc(NULL,&raw,&interpreted)==-9&&!driver->quiesce());
 puts("Civil policy: prefix compatibility, full fractional projection, DST edges, saved RTC basis, no clock I/O/writes and retained fence PASS");
}
