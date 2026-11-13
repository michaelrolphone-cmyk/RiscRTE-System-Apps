#pragma once
#include "RiscRuntimeClockV1.h"

#ifdef PORTABLE_STAGE_LOGS
#include <stdio.h>
/* A direct diagnostic statement at the caller's code boundary. No recorder,
 * phase IDs, command, queue, heap allocation or per-move event stream. */
static inline void portable_stage_log(const risc_runtime_api_v1 *runtime,
    const char *stage, const char *detail) {
    uint32_t now=0;
    if(!runtime || !runtime->diagnostic || !risc_runtime_monotonic_ms_v1(runtime,&now))return;
    char line[224];
    int n=snprintf(line,sizeof(line),"APP t_ms=%llu stage=%s %s",
        (unsigned long long)now,stage,detail?detail:"");
    if(n>0 && (size_t)n<sizeof(line))(void)runtime->diagnostic(line);
    else (void)runtime->diagnostic("APP stage=log-truncated");
}
#else
#define portable_stage_log(runtime,stage,detail) ((void)0)
#endif
