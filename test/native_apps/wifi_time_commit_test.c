#define PORTABLE_WIFI_TIME_SYNC
#define main settings_fixture_main
#include "settings_native_alarm_test.c"
#undef main
static uint32_t sample_now=100;static bool clock_lost;static unsigned clock_reads;
static bool sample_clock(uint32_t *out){assert(!native_custody_retained);clock_reads++;if(clock_lost)return false;*out=sample_now;return true;}
static int active_mode;
static int32_t delayed_begin(void *c,uint32_t wanted,alarm_clock_edit_permit_v2 *out){int32_t rc=begin(c,wanted,out);sample_now=1101;return rc;}
static bool delayed_acquire(const char *cap,uint32_t v,uint64_t i,risc_runtime_capability_v1 *out){bool ok=acquire(cap,v,i,out);if(active_mode==14)sample_now=1101;else clock_lost=true;return ok;}
static bool sample_guard(void *p){(void)p;return !native_custody_retained;}
int main(int argc,char **argv){assert(argc==2);int mode=atoi(argv[1]);active_mode=mode;
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.retain_invocation=retain,.monotonic_ms=sample_clock};rt=&runtime;native_custody_runtime=&runtime;alarms.api=&service.service.base;epoch=1767225600;display_settled=true;
 portable_time_sync sample={.runtime=&runtime,.guard=sample_guard,.state=PORTABLE_TIME_SYNC_SAMPLE,.sample={.utc_seconds=1800000000,.observed_monotonic_ms=100}};sample.self=&sample;
 if(mode==1)display_settled=false;
 if(mode==2)alarm_modal=true;
 if(mode==3)sample_now=1101;
 if(mode==4)clock_lost=true;
 if(mode==5)begin_result=ALARM_BUSY;
 if(mode==6)acquire_result=1;
 if(mode==7)seed_result=RISC_REALTIME_IO;
 if(mode==8)read_result=RISC_REALTIME_IO;
 if(mode==9)release_fail=true;
 if(mode==10)begin_result=99;
 if(mode==11)finish_result=99;
 if(mode==12)finish_result=ALARM_RTC;
 if(mode==13)service.begin=delayed_begin;
 if(mode==14||mode==15)runtime.acquire=delayed_acquire;
 if(mode==16)sample.sample.observed_monotonic_ms=101;
 if(mode==17){alarms.api=NULL;wifi_clock_commit.message="Native time verified";}
 const char *message=NULL;int result=portable_wifi_time_commit(&sample,&message);
 if(!mode){assert(result==1&&epoch==1800000000&&seeds==1&&finish_calls==1&&!cancel_calls);assert(strstr(message,"unauthenticated"));}
 if(mode==1||mode==2)assert(result==0&&!begin_calls&&!seeds&&!clock_reads);
 if(mode==3)assert(result==-1&&!begin_calls&&!seeds&&epoch==1767225600);
 if(mode==4)assert(result==-2&&native_custody_retained&&!begin_calls&&!seeds&&clock_reads==1);
 if(mode==5)assert(result==-1&&!seeds&&!cancel_calls);
 if(mode==6)assert(result==-1&&!seeds&&cancel_calls==1);
 if(mode>=7&&mode<=11)assert(result==-2&&native_custody_retained);
 if(mode==12)assert(result==-1&&seeds==1&&finish_calls==1&&!native_custody_retained);
 if(mode==13)assert(result==-1&&!seeds&&cancel_calls==1&&!acquired&&epoch==1767225600);
 if(mode==14)assert(result==-1&&!seeds&&cancel_calls==1&&released==1&&epoch==1767225600);
 if(mode==15)assert(result==-2&&!seeds&&!cancel_calls&&!released&&native_custody_retained);
 if(mode==16)assert(result==-1&&!begin_calls&&!seeds);
 if(mode==17)assert(result==-1&&!seeds&&!begin_calls&&!strcmp(message,"Clock update unavailable"));
 printf("PASS Wi-Fi shared clock commit mode=%d result=%d seed=%u cancel=%u\n",mode,result,seeds,cancel_calls);return 0;
}
