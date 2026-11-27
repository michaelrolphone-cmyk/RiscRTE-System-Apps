#pragma once
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscBatteryGaugeV1.h"
#ifdef PORTABLE_AUDIO_CONTINUOUS_CAPTURE
#include "PortableAudioSession.h"
#endif
#ifdef PORTABLE_RADIO_CONTINUOUS_CAPTURE
#include "PortableRadioSession.h"
#endif
/* Explicit deployment-local returning Light policy. No Runtime ABI, board IDs,
 * retained-wake records, settings writes or implicit app exit are introduced.
 * The deployment supplies a cached confirmed interval; this query must perform
 * no I/O. Zero means unavailable/disabled. Values over INT32_MAX are refused. */
uint32_t portable_foreground_idle_timeout_ms(void);
/* Initially after settled provider initialization, then only on qualified
 * recovery checkpoints for an explicit transient result. Read
 * confirmed preferences into the cache; do not write. 0: available; -2:
 * retained; -3: clean transient BUSY/IO, eligible for bounded recovery retry;
 * other negative values: unavailable (UI stays live, idle disabled). Retry
 * backoff is internal recovery, never an idle timeout or input activity. */
int portable_foreground_idle_configure(const risc_runtime_api_v1 *);
/* Pure cache update after a confirmed idle-timer Save, never a deep-timer
 * Save. This does not reset observed input activity or write preferences. */
void portable_foreground_idle_timeout_changed(uint32_t milliseconds);
/* Called only at a settled owner checkpoint with app touch closed, navigation
 * claims detached and private/model grants still live. The helper rechecks late
 * physical input and native readiness before entering returning Light sleep.
 * 1: woke; 0: clean refusal with display/peripherals fully restored.
 * -1: foreground failure; -2: retained/uncertain custody. Both negative outcomes
 * retain this invocation rather than discarding its unsaved RAM. Unknown values
 * also fail closed. No adapter provider I/O follows a retained result.
 * This hook must never enter Deep, unload, save/cancel a draft or restart work. */
int portable_app_foreground_idle_sleep(const risc_runtime_api_v1 *,
    const risc_display_output_api_v1 *,const risc_battery_gauge_api_v1 *);
#ifdef PORTABLE_FOREGROUND_IDLE_WORK_GUARD
/* Optional app-specific RAM-only activity query, not a launch/exit guard.
 * True defers suspension without stopping the operation or resetting input
 * activity. Use for legitimate work not covered by the common owner gates. */
bool portable_app_foreground_idle_inhibited(void);
#endif

#ifdef PORTABLE_FOREGROUND_ALARM_SLEEP
/* Optional deployment-local protocol. Adapter owns the exact API2 ticket.
 * prepare: 1 ready, 0 clean defer, -2 retained. duration 0 means no deadline;
 * otherwise 1..86400000 ms. resume: 1 consumed, 0 clean service refusal,
 * -2 retained. Unknown returns are terminal. No callback/cleanup follows
 * retained. Resume exactly once only after successful Light and full native
 * restoration; never consume a refused sleep. Legacy entrypoint unchanged. */
typedef struct {
 void *context;
 int (*prepare)(void *,uint32_t *duration_ms);
 int (*resume)(void *);
} portable_foreground_alarm_hooks;
int portable_app_foreground_idle_sleep_alarm(const risc_runtime_api_v1 *,
 const risc_display_output_api_v1 *,const risc_battery_gauge_api_v1 *,
 const portable_foreground_alarm_hooks *);
#endif
