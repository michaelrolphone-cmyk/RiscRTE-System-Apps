#pragma once
#include "RiscRuntimeV1.h"
#include "RiscTimeSampleV1.h"
/* Invocation-local read-only sampling client. Caller owns the network service
 * lease from start through checked cancellation/completion. UTC is not seeded
 * here: the application must use the shared alarm-aware clock transaction.
 * No background task, network ownership, credentials or persistent state. */
enum { PORTABLE_TIME_SYNC_IDLE=0, PORTABLE_TIME_SYNC_PENDING=1,
 PORTABLE_TIME_SYNC_SAMPLE=2, PORTABLE_TIME_SYNC_FAILED=3,
 PORTABLE_TIME_SYNC_RETAINED=4, PORTABLE_TIME_SYNC_CLOSING=5 };
typedef bool (*portable_time_sync_guard)(void *);
typedef struct portable_time_sync {
 const struct portable_time_sync *self;
 const risc_runtime_api_v1 *runtime;
 portable_time_sync_guard guard;
 void *guard_context;
 risc_runtime_capability_v1 grant;
 const risc_time_sample_api_v1 *api;
 uint64_t token;
 uint32_t started_ms,timeout_ms;
 int state,result,finish_state,finish_reason;
 risc_time_sample_v1 sample;
} portable_time_sync;
/* Returns state; RETAINED is terminal and prohibits all subsequent callbacks.
 * Ordinary errors close/release before FAILED; SAMPLE means closed/released.
 * Output sample must be used promptly, with a fresh bounded-age check. */
int portable_time_sync_start(portable_time_sync *,const risc_runtime_api_v1 *,
 portable_time_sync_guard,void *,const char *capability,uint64_t instance,
 const char *server,uint32_t timeout_ms);
int portable_time_sync_step(portable_time_sync *);
int portable_time_sync_cancel(portable_time_sync *);
bool portable_time_sync_epoch(portable_time_sync *,int64_t *epoch);
