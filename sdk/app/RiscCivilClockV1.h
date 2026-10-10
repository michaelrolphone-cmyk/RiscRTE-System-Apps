#pragma once
#include <stdint.h>
#define RISC_CIVIL_CLOCK_CAPABILITY "time.civil"
/* Read-only local-calendar policy. No UI, alarm programming, clock seeding or
 * global timezone mutation. Days/minutes are local civil values, not durations. */
typedef struct {uint32_t struct_size;int32_t day;uint32_t minute;char zone[40];} risc_civil_time_v1;
typedef struct {uint32_t api_version,struct_size;void *context;int32_t (*read)(void *,risc_civil_time_v1 *);} risc_civil_clock_api_v1;
/* 0=valid, -1=unavailable/invalid time, -9=retained; do no further I/O on -9. */
