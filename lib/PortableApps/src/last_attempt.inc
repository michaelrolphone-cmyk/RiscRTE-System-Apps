/* Pre-acquired, optional RAM-only client. Not a live serial diagnostic.
 * Conservative latch: any observed false bool, denied checkpoint or retention
 * disables later writes for the invocation, even where recovery is permitted.
 * Existing provider results/control flow are never changed. */
#ifdef PORTABLE_LAST_ATTEMPT_CHECKPOINT
#include "RiscDiagnosticCheckpointV1.h"
static risc_diagnostic_checkpoint_client_v1 last_attempt_client;
static bool last_attempt_stopped;
static void last_attempt_initialize(void) {
 last_attempt_client=(risc_diagnostic_checkpoint_client_v1){.struct_size=sizeof(last_attempt_client)};
 last_attempt_stopped=true;
 if(risc_runtime_diagnostic_checkpoint_client(native_custody_runtime,&last_attempt_client) &&
    last_attempt_client.api_version==RISC_DIAGNOSTIC_CHECKPOINT_API_V1 &&
    last_attempt_client.struct_size>=sizeof(last_attempt_client) &&
    last_attempt_client.invocation && last_attempt_client.write)last_attempt_stopped=false;
}
void portable_last_attempt_stop(void){last_attempt_stopped=true;}
bool portable_last_attempt_bool(bool result){if(!result)last_attempt_stopped=true;return result;}
void portable_last_attempt(const char *operation) {
 if(last_attempt_stopped || failed || native_custody_retained || !operation)return;
 /* Call sites supply static bounded text. No clock/health/readback operation. */
 size_t n=strlen(operation);if(!n || n>96){last_attempt_stopped=true;return;}
 char text[128];const char prefix[]="last-attempt=";
 memcpy(text,prefix,sizeof(prefix)-1);memcpy(text+sizeof(prefix)-1,operation,n);
 int32_t rc=last_attempt_client.write(last_attempt_client.invocation,text,(uint32_t)(sizeof(prefix)-1+n));
 if(rc!=RISC_DIAGNOSTIC_CHECKPOINT_OK)last_attempt_stopped=true;
}
#endif
