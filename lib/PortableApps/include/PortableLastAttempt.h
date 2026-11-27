#pragma once
/* Opt-in diagnostic only. A checkpoint names an attempted operation, never
 * asserts that it failed. No callback is added after a false bool result. */
#ifdef PORTABLE_LAST_ATTEMPT_CHECKPOINT
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#error "Last-attempt diagnostics require the native custody fence"
#endif
#include <stdbool.h>
void portable_last_attempt(const char *operation);
bool portable_last_attempt_bool(bool result);
void portable_last_attempt_stop(void);
#define PORTABLE_LAST_ATTEMPT(op) portable_last_attempt(op)
#define PORTABLE_ATTEMPT_BOOL(op,call) (portable_last_attempt(op),portable_last_attempt_bool((call)))
#else
#define PORTABLE_LAST_ATTEMPT(op) ((void)0)
#define PORTABLE_ATTEMPT_BOOL(op,call) (call)
#endif
