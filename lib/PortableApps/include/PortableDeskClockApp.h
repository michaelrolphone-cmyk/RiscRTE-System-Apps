#pragma once
/* Private compile-time app/deployment links. Never native Runtime exports. */
#include "PortableDeskClock.h"
#include "RiscRuntimeV1.h"
#include "AlarmServiceV1.h"
#include "RiscDisplayOutputV1.h"
typedef struct {
 void *context;
 /* 0: continue; 1: ordinary cancellation; -2: retained. No subscription. */
 int (*cancelled)(void *context);
 /* 1: staged in RAM; 0: ordinary refusal; -2: retained. Must run before
  * provider preparation/holds: Runtime deliberately rejects staging then. */
 int (*stage)(void *context,const portable_desk_record *);
 /* 1: prepared; 0: ordinary refusal ALREADY rolled back; -2: retained. */
 int (*prepare)(void *context);
 /* 1: restored, including brightness; -2: unconfirmed custody, stop all I/O. */
 int (*resume)(void *context);
 /* Terminal entry for the same already-staged proposal. Success never
  * returns. 0: refusal, caller resumes before clearing; -2: no cleanup. */
 int (*stage_enter)(void *context,const portable_desk_record *,uint32_t sleep_ms,
                    uint32_t sampled_at_ms);
} portable_desk_sleep_ops;
/* Deployment reads owned Runtime record only during app_main. Returns 1 for
 * valid classified timer wake, 0 otherwise, -2 for retained grant cleanup. */
int portable_desk_clock_boot_read(const risc_runtime_api_v1 *,portable_desk_record *);
/* Called by the selected deployment local-sleep hook, inside adapter lifecycle.
 * 0 returns to ordinary Clock; -2 preserves all grants/resources. */
int portable_desk_clock_run(const risc_runtime_api_v1 *,const portable_desk_sleep_ops *);
/* App-local adapter helpers, only linked for this explicit build. */
bool portable_desk_adapter_sleep(void);
/* Seed returns1 on success,0 if unavailable,-2 on uncertain provider failure. */
int portable_desk_adapter_seed(void);
void portable_desk_adapter_begin(void);
bool portable_desk_adapter_present(bool full);
void portable_desk_adapter_invalidate(void);
bool portable_desk_adapter_ready(void);
void portable_desk_adapter_retain(void);
void portable_desk_clock_refused(void);
void portable_desk_clock_radios_off(void);
/* Defer optional radio policy application until normal foreground is needed. */
int portable_desk_clock_mode(void);
bool portable_desk_adapter_foreground(void);
