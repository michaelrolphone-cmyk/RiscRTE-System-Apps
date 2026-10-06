#pragma once
#include <stdbool.h>
/* File handles never cross a poll, sleep, alarm or quick-control boundary.
 * A failed read-handle close retains its grant and blocks further storage I/O.
 * The controller retries Close/Back; it never abandons uncertain ownership. */
bool portable_file_browser_safe(void);
bool portable_file_browser_close(void);
