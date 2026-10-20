#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_TIMECARD_NATIVE_UI
/* Statically linked Timecard-only presentation ownership; no Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_TIMECARD_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_TIMECARD_PRIVATE
#endif
typedef struct {
 bool valid,began,down,released,cancelled;
 int x,y;
} portable_timecard_contact;
/* Current app poll only; a modal/navigation-consumed poll returns cancellation.
 * Read once after each poll: no polling, provider reads or event lookahead. */
PORTABLE_TIMECARD_PRIVATE bool portable_timecard_native_contact(portable_timecard_contact *out);
/* Read-only runtime selection. Paper formats keep their original app path. */
PORTABLE_TIMECARD_PRIVATE bool portable_timecard_native_available(void);
/* Call begin only for native color. Existing paper UI must not activate it. */
PORTABLE_TIMECARD_PRIVATE void portable_timecard_native_begin(void);
PORTABLE_TIMECARD_PRIVATE void portable_timecard_native_layout(uint64_t layout_id);
PORTABLE_TIMECARD_PRIVATE void portable_timecard_native_present(bool full);
/* Activate only for the native color presentation; paper keeps its own input.
 * Read only: sequence belongs to the adapter raw provider. Always returns the
 * current generation, consumes no events, and never delays timecard polling,
 * releases or cleanup. Layout IDs must identify the visible hit-target map. */
PORTABLE_TIMECARD_PRIVATE bool portable_timecard_native_ready(uint64_t *generation,uint64_t *visible_sequence);
/* Read-only recovery hint after foreign pixels: true only when an invalid map
 * can be repainted without overtaking a frame, overlay or return handoff. */
PORTABLE_TIMECARD_PRIVATE bool portable_timecard_native_repaint(void);
/* Queue ordinary adapter Back. The next poll owns drain, session suspension
 * and return; this call never launches, changes tracking state or releases providers. */
PORTABLE_TIMECARD_PRIVATE void portable_timecard_native_back(void);
#undef PORTABLE_TIMECARD_PRIVATE
#endif
