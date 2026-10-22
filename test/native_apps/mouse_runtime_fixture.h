#ifndef MOUSE_RUNTIME_FIXTURE_H
#define MOUSE_RUNTIME_FIXTURE_H
/* TEST ONLY. Software dependency controls, owned by the single native test
 * thread. No product imports, asynchronous producer, or physical transport. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
 bool present, close_fails;
 uint8_t report[5];
 unsigned remaining, lower_calls, opens, closes, polls;
 uint64_t raw, last_close, now, device;
} mouse_runtime_controls;
mouse_runtime_controls *mouse_runtime_control(void);
const void *mouse_runtime_clock(void);
void mouse_runtime_lower_call(const char *name);
void mouse_runtime_event(const char *name);
const char *mouse_runtime_mode(void);
bool mouse_runtime_demand(void);
/* This harness is the ONLY call site that invokes the mouse poll callback.
 * It checks the same serialized owner as Runtime's port.owner callback. It is
 * not a proposed production executor, UI callback, or helper implementation. */
bool mouse_runtime_test_only_poll(bool (*poll)(void *, size_t), void *, size_t);
#ifdef __cplusplus
}
#endif
#endif
