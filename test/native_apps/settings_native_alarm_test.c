/* Real Settings native seed/readback + editor dispatch. Service callbacks are
 * modeled here; separate service fixture proves persistence and UTC behavior. */
#define PORTABLE_SETTINGS_APP
#define PORTABLE_SETTINGS_NATIVE_TIME
#define PORTABLE_SETTINGS_NATIVE_ONLY
#define PORTABLE_SETTINGS_TIME_ZONE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_ALARM_CLIENT
#define ALARM_SERVICE_TAGGED_V2
#define ALARM_SERVICE_CLOCK_EDIT_V2
#define PORTABLE_SETTINGS_CLOCK_EDIT_V2
#include "../../lib/PortableApps/src/adapter.c"
#include <assert.h>
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static int64_t epoch;
static unsigned acquired,released,seeds,begin_calls,finish_calls,cancel_calls,service_calls;
static int seed_result,read_result,acquire_result,begin_result,finish_result,cancel_result;
static bool forbidden,release_fail;
static alarm_clock_edit_permit_v2 issued;
static alarm_clock_edit_status_v2 edit_state;
static void check(void){assert(!forbidden&&!native_custody_retained);service_calls++;}
static int32_t result(int32_t r){if(r==99||r==-99||r==ALARM_RETAINED||r==ALARM_OUTPUT)forbidden=true;return r;}
static int32_t copied(void*c,alarm_status_v1*out){(void)c;check();*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY,.snapshot=2,.rtc_seconds=(uint32_t)(epoch-946684800)};return 0;}
static int32_t step(void*c){(void)c;check();return 0;}
static int32_t service_refresh(void*c){(void)c;check();return 1;}
static int32_t acknowledge(void*c,const alarm_token_v1*t){(void)c;(void)t;assert(0);return 0;}
static int32_t prepare(void*c,alarm_sleep_v1*t){(void)c;(void)t;assert(0);return 0;}
static int32_t stop(void*c){(void)c;assert(0);return 0;}
static int32_t resume(void*c,const alarm_sleep_v1*t){(void)c;(void)t;assert(0);return 0;}
static int32_t begin(void*c,uint32_t wanted,alarm_clock_edit_permit_v2*out){(void)c;check();begin_calls++;assert(!seeds);if(begin_result)return result(begin_result);issued=(alarm_clock_edit_permit_v2){sizeof(issued),1,2,wanted,(uint32_t)(epoch-946684800),3,{0,0}};*out=issued;edit_state=(alarm_clock_edit_status_v2){sizeof(edit_state),ALARM_CLOCK_EDIT_INTENT,2,wanted,issued.previous,{0,0}};return 0;}
static int32_t finish(void*c,const alarm_clock_edit_permit_v2*p){(void)c;check();finish_calls++;assert(seeds==1&&released==1&&!memcmp(p,&issued,sizeof(issued)));if(finish_result)return result(finish_result);assert(epoch==946684800+(int64_t)p->requested);edit_state.phase=ALARM_CLOCK_EDIT_VERIFIED;return 1;}
static int32_t cancel(void*c,const alarm_clock_edit_permit_v2*p){(void)c;check();cancel_calls++;assert(!seeds&&!memcmp(p,&issued,sizeof(issued)));if(cancel_result)return result(cancel_result);edit_state.phase=ALARM_CLOCK_EDIT_NONE;return 0;}
static int32_t editor_status(void*c,alarm_clock_edit_status_v2*out){(void)c;check();*out=edit_state;return 0;}
static alarm_clock_edit_descriptor_v2 service={
 {{2,sizeof(service),NULL,copied,step,service_refresh,acknowledge,prepare,stop},ALARM_SERVICE_DESCRIPTOR_TAG,1,0,ALARM_DESCRIPTOR_RESUME_SLEEP|ALARM_DESCRIPTOR_CLOCK_EDIT,resume},
 ALARM_CLOCK_EDIT_TAG,1,begin,finish,cancel,editor_status};
static int32_t snapshot(void*c,risc_realtime_snapshot_v1*out){(void)c;assert(!forbidden&&!native_custody_retained);if(read_result)return read_result;*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.epoch_seconds=epoch,.validity=RISC_REALTIME_VALID};return 0;}
static int32_t seed(void*c,int64_t e,uint32_t us){(void)c;assert(!us&&!forbidden&&!native_custody_retained);seeds++;epoch=e;return seed_result;}
static risc_realtime_control_api_v1 control={1,sizeof(control),&epoch,snapshot,seed};
static bool acquire(const char*cap,uint32_t version,uint64_t instance,risc_runtime_capability_v1*out){assert(!strcmp(cap,"runtime.realtime-control")&&version==1&&!instance);acquired++;if(acquire_result)return false;out->api=&control;out->slot=1;out->generation=1;return true;}
static bool release(risc_runtime_capability_v1*g){assert(!forbidden&&!native_custody_retained);released++;if(release_fail)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool retain(void){return true;}
static risc_runtime_api_v1 runtime;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
int main(int argc,char**argv){assert(argc==2);int mode=atoi(argv[1]);
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.retain_invocation=retain};rt=&runtime;native_custody_runtime=&runtime;alarms.api=&service.service.base;epoch=1767225600;
 snt_load_basis();snt_zone_status=PORTABLE_TIMEZONE_MISSING;assert(portable_timezone_resolve("UTC",4,&snt_rule)==0);
 settings_draft=(twatch_rtc_time_v1){2027,2,3,3,4,5,6};
 if(mode==1)settings_draft=(twatch_rtc_time_v1){2025,12,30,2,23,58,57};
 if(mode==2)begin_result=ALARM_BUSY;
 if(mode==3)acquire_result=1;
 if(mode==4)seed_result=RISC_REALTIME_IO;
 if(mode==5)read_result=RISC_REALTIME_IO;
 if(mode==6)release_fail=true;
 if(mode==7)begin_result=99;
 if(mode==8)finish_result=99;
 if(mode==9){acquire_result=1;cancel_result=99;}
 if(mode==10)service.tag=0;
 if(mode==11)finish_result=ALARM_RTC;
 bool ok=snt_save(-1);fprintf(stderr,"mode=%d ok=%d seed=%u begin=%u finish=%u cancel=%u retained=%d msg=%s\n",mode,ok,seeds,begin_calls,finish_calls,cancel_calls,native_custody_retained,editor_message?editor_message:"");
 if(mode<2){assert(ok&&seeds==1&&finish_calls==1&&!cancel_calls&&edit_state.phase==ALARM_CLOCK_EDIT_VERIFIED&&!native_custody_retained);}
 else {assert(!ok);if(mode==2||mode==10)assert(!seeds&&!finish_calls&&!cancel_calls);if(mode==3)assert(!seeds&&cancel_calls==1&&!native_custody_retained);if(mode>=4&&mode<=6)assert(seeds==1&&!finish_calls&&!cancel_calls&&native_custody_retained);if(mode>=7&&mode<=9)assert(native_custody_retained&&forbidden);if(mode==11)assert(seeds==1&&finish_calls==1&&!cancel_calls&&!native_custody_retained);}
 printf("PASS native Settings editor mode=%d verified=%u seed=%u finish=%u cancel=%u retained=%u\n",mode,ok,seeds,finish_calls,cancel_calls,native_custody_retained);
 return 0;
}
