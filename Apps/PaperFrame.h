#pragma once
#include <stdbool.h>
/* Private app/client coordination. The adapter owns progress and the pending
 * token; controllers retain their latest dirty state until drawing is ready.
 * Final app return drains first: Runtime checks exit safety before fini. */
#if defined(PORTABLE_NATIVE_CUSTODY_FENCE) || defined(PORTABLE_DESK_CLOCK_SPARSE_START)
bool portable_paper_frame_ready(void);
bool portable_paper_frame_drain(void);
static inline bool paper_frame_ready(void) {return portable_paper_frame_ready();}
static inline bool paper_frame_drain(void) {return portable_paper_frame_drain();}
#else
static inline bool paper_frame_ready(void) {return true;}
static inline bool paper_frame_drain(void) {return true;}
#endif
