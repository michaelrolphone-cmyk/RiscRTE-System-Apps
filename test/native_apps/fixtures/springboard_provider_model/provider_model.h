#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "RiscDisplayOutputV1.h"
#ifdef __cplusplus
extern "C" {
#endif
void model_reset(unsigned width,unsigned height,unsigned pair_cost_us);
bool model_submit(void);
bool model_pending(void);
void model_yield(unsigned request_ms);
unsigned model_now_ms(void);
uint64_t model_now_us(void);
void model_advance_us(unsigned duration_us);
unsigned model_row_count(void);
unsigned model_poll_count(void);
const risc_display_output_api_v1* model_display_api(void);
void model_teardown(void);
#ifdef __cplusplus
}
#endif
