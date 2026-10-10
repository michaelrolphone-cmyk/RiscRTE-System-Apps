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
#ifdef PORTABLE_PAPER_CROSSFADE
void portable_paper_transition_begin(void);
void portable_paper_transition_cancel(void);
bool portable_paper_transition_active(void);
static inline void paper_transition_begin(void){portable_paper_transition_begin();}
static inline void paper_transition_cancel(void){portable_paper_transition_cancel();}
static inline bool paper_transition_active(void){return portable_paper_transition_active();}
#else
static inline void paper_transition_begin(void){}
static inline void paper_transition_cancel(void){}
static inline bool paper_transition_active(void){return false;}
#endif
