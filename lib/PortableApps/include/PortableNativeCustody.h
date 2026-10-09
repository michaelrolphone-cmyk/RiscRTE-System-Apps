#pragma once
#include <stdbool.h>
/* Hidden app-local terminal fence. Selected callers return promptly and keep
 * allocations/grants alive. The query is memory-only and shared by AppData,
 * alarm callbacks and the foreground adapter. No ordinary I/O follows it. */
void portable_adapter_retain(void);
bool portable_adapter_retained(void);
