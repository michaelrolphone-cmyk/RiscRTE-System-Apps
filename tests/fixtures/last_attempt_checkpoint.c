#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscDiagnosticCheckpointV1.h"
#include "PortableLastAttempt.h"
static bool failed,native_custody_retained;
static const risc_runtime_api_v1 *native_custody_runtime;
#include "last_attempt.inc"
static unsigned writes,gets,calls;static int deny;static char saved[128];
static int32_t write_checkpoint(uint64_t id,const char *text,uint32_t n){
 assert(id==7&&!native_custody_retained);++writes;
 if(deny)return RISC_DIAGNOSTIC_CHECKPOINT_DENIED;
 assert(n<sizeof(saved));memcpy(saved,text,n);saved[n]=0;return RISC_DIAGNOSTIC_CHECKPOINT_OK;
}
static bool get_client(risc_diagnostic_checkpoint_client_v1*out){
 ++gets;*out=(risc_diagnostic_checkpoint_client_v1){1,sizeof(*out),7,write_checkpoint};return true;
}
void portable_adapter_retain(void){portable_last_attempt_stop();native_custody_retained=true;}
bool portable_adapter_retained(void){return native_custody_retained;}
#include "PortableTouch.h"
static bool poll_bad(void*c,size_t n){(void)c;assert(n==1);++calls;return false;}
static bool snapshot_bad(void*c,risc_touch_snapshot_v1*out){(void)c;(void)out;++calls;return false;}
static int32_t next_bad(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)id;(void)out;++calls;return -2;}
static bool bool_call(bool result){++calls;return result;}
static risc_runtime_api_v1 runtime;
static void reset(void){writes=gets=calls=0;deny=0;failed=native_custody_retained=false;saved[0]=0;runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.diagnostic_checkpoint_client=get_client};native_custody_runtime=&runtime;last_attempt_initialize();assert(gets==1);}
int main(void){
 reset();assert(PORTABLE_ATTEMPT_BOOL("display.submit",bool_call(true)));assert(writes==1&&calls==1&&!strcmp(saved,"last-attempt=display.submit"));
 assert(!PORTABLE_ATTEMPT_BOOL("display.status",bool_call(false)));unsigned before=writes;PORTABLE_LAST_ATTEMPT("must-not-write");assert(writes==before&&calls==2&&!strcmp(saved,"last-attempt=display.status"));
 reset();deny=1;PORTABLE_LAST_ATTEMPT("denied");PORTABLE_LAST_ATTEMPT("no-retry");assert(writes==1);
 reset();portable_adapter_retain();PORTABLE_LAST_ATTEMPT("after-retain");assert(!writes);
 reset();failed=true;PORTABLE_LAST_ATTEMPT("after-failed");assert(!writes);
 reset();runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;last_attempt_initialize();PORTABLE_LAST_ATTEMPT("old-api");assert(!writes&&gets==1);
 reset();risc_touch_api_v1 api={.poll=poll_bad,.snapshot=snapshot_bad,.next=next_bad};portable_touch t={.api=&api,.subscription=1,.initialized=true};
 (void)portable_touch_collect(&t);assert(writes==1&&calls==1&&!native_custody_retained);portable_touch_sample out;portable_touch_next(&t,&out);assert(native_custody_retained&&writes==1&&calls==2&&!strcmp(saved,"last-attempt=touch.poll"));
 reset();t=(portable_touch){.api=&api,.subscription=1};assert(!portable_touch_sync(&t,true));assert(native_custody_retained&&writes==1&&!strcmp(saved,"last-attempt=touch.snapshot"));PORTABLE_LAST_ATTEMPT("late");assert(writes==1);
 reset();t=(portable_touch){.api=&api,.subscription=1,.initialized=true};portable_touch_next(&t,&out);assert(native_custody_retained&&writes==1&&!strcmp(saved,"last-attempt=touch.next"));
 puts("PASS last-attempt literal records; bool failures/terminal touch/retention/denial stop all later checkpoints; original results unchanged");
}
