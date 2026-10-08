#include <stddef.h>
#include "PortableBroadcastAppData.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
static jmp_buf held;
static void yield_retained(uint32_t ms){assert(ms==50);longjmp(held,1);}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.yield_ms=yield_retained};
static unsigned fences;
static bool retain_current(void){++fences;return true;}
static const struct {risc_runtime_api_v1 prefix;bool(*confirm)(void);bool(*retain)(void);} modern={
 .prefix={.api_version=1,.struct_size=sizeof(modern),.yield_ms=yield_retained},.retain=retain_current};
static unsigned pauses,calls;static bool publishing=true,refuse;static int32_t outcome;
bool portable_broadcast_stop(void){pauses++;if(refuse)return false;publishing=false;return true;}
static int32_t stat_file(void*c,const char*n,uint32_t*s,uint64_t*r){assert(c==(void*)1&&!strcmp(n,"record")&&!publishing);calls++;*s=4;*r=17;return outcome;}
static int32_t read_file(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*s,uint64_t*r){assert(expected==17&&cap==4);int32_t rc=stat_file(c,n,s,r);if(!rc)memcpy(b,"test",4);return rc;}
static int32_t replace(void*c,const char*n,uint64_t expected,const void*b,uint32_t size){assert(expected==17&&size==4&&!memcmp(b,"test",4));uint32_t s;uint64_t r;return stat_file(c,n,&s,&r);}
static const risc_app_data_v1 backend={1,sizeof(backend),(void*)1,stat_file,read_file,replace};
int main(void){
 portable_broadcast_app_data s;const risc_app_data_v1 *api=portable_broadcast_data_bind(&s,&backend,&runtime);assert(api);
 uint32_t size=0;uint64_t revision=0;char bytes[4]={0};
 assert(api->stat(api->context,"record",&size,&revision)==0&&size==4&&revision==17&&pauses==1);
 publishing=true;assert(api->read(api->context,"record",17,bytes,4,&size,&revision)==0&&!memcmp(bytes,"test",4)&&pauses==2);
 publishing=true;outcome=RISC_APP_DATA_COMMIT_UNKNOWN;assert(api->replace(api->context,"record",17,"test",4)==outcome&&!s.retained);
 publishing=true;outcome=RISC_APP_DATA_RETAINED;assert(api->stat(api->context,"record",&size,&revision)==outcome&&s.retained&&!publishing);
 unsigned stopped=pauses,io=calls;assert(api->read(api->context,"record",17,bytes,4,&size,&revision)==outcome&&size==0&&revision==0);assert(api->replace(api->context,"record",17,"test",4)==outcome&&pauses==stopped&&calls==io);
 api=portable_broadcast_data_bind(&s,&backend,&runtime);refuse=publishing=true;
 if(!setjmp(held)){(void)api->stat(api->context,"record",&size,&revision);assert(!"failed pause must retain call stack");}
 assert(calls==io&&!s.retained&&publishing);refuse=false;outcome=RISC_APP_DATA_OK;
 assert(api->stat(api->context,"record",&size,&revision)==RISC_APP_DATA_OK&&calls==io+1&&!publishing);
 api=portable_broadcast_data_bind(&s,&backend,&modern.prefix);refuse=publishing=true;io=calls;stopped=pauses;
 assert(api->stat(api->context,"record",&size,&revision)==RISC_APP_DATA_RETAINED&&s.retained&&fences==1);
 assert(calls==io&&pauses==stopped+1);stopped=pauses;
 assert(api->read(api->context,"record",17,bytes,4,&size,&revision)==RISC_APP_DATA_RETAINED&&calls==io&&pauses==stopped&&fences==1);
 puts("App-data broadcast fence: pause before stat/read/replace, preserved revisions/outcomes, cleanup-only retained stack after pause refusal, no radio/storage I/O after RETAINED PASS");
}
