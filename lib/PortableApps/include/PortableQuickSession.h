#pragma once
#include "PortableQuickActions.h"
#include "PortableQuickPreferences.h"
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
/* One invocation-local session. No retained grants or app pointers. */
typedef struct {
 pqa_state ui;
 unsigned brightness,volume,restore_volume;
 bool loaded,hour_24,dnd_enabled;
} pqa_session;
void pqa_session_init(pqa_session *s);
bool pqa_session_load(pqa_session *s,const risc_runtime_api_v1 *rt);
/* Caller must be at a settled display/service boundary. False is a lifecycle
 * failure; ordinary failed saves are shown in ui.error_flags. alerts_changed signals
 * confirmed volume or DND changes so the caller refreshes alarm.service. */
bool pqa_session_apply(pqa_session *s,const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *display,uint32_t actions,bool *alerts_changed);
bool pqa_session_restore(const pqa_session *s,const risc_display_output_api_v1 *display);
