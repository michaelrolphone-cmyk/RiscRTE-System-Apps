#pragma once
/* Test-only provider interface. Platform custody stays in the real CpuPort. */
#include <RiscRetainedWakeV1.h>
#include <stdbool.h>
#include <stdint.h>
#define DESK_GATE_CAPABILITY "test.desk-clock-gate"
typedef struct {
    uint32_t api_version, struct_size;
    bool (*hold)(bool);
    bool (*lock)(bool);
    int32_t (*enter)(uint32_t);
} desk_gate_api;
#ifdef __cplusplus
extern "C" {
#endif
int test_desk_gate_mode(void);
void test_desk_gate_safe(bool expected);
void test_desk_gate_pending(const risc_retained_wake_record_v1 *expected);
#ifdef __cplusplus
}
#endif
