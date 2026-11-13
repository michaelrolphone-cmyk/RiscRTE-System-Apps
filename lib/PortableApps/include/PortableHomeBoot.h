#pragma once
#include "RiscRuntimeV1.h"
#include "RiscRetainedWakeV1.h"
#include "RiscHomePresentationV1.h"
/* Single application bootstrap owns the consuming native read. View consumers
 * receive exactly the original record once, with no external TIMER lookup. */
int portable_home_boot_begin(const risc_runtime_api_v1*);
unsigned portable_home_boot_view(void);
uint32_t portable_home_boot_cause(void);
int32_t portable_home_boot_read(const risc_retained_wake_api_v1*,uint32_t,uint32_t,risc_retained_wake_record_v1*,uint32_t*);
int32_t portable_home_boot_read_bytes(const risc_retained_wake_api_v1_extended*,uint32_t,uint32_t,void*,uint32_t,uint32_t*,uint32_t*);
