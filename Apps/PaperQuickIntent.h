#pragma once
#include <stdbool.h>
/* A private app request. The shared adapter consumes this intent on the next
 * poll, after the current frame settles; Home never drives the modal itself. */
#ifdef PORTABLE_QUICK_ACTIONS
__attribute__((visibility("hidden"))) bool portable_paper_quick_open(void);
#endif
