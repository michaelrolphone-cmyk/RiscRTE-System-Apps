#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_FREQUENCY_NATIVE_UI
/* Statically linked Frequency-only presentation ownership; no Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_FREQUENCY_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_FREQUENCY_PRIVATE
#endif
/* Read-only runtime selection. Paper formats keep their original app path. */
PORTABLE_FREQUENCY_PRIVATE bool portable_frequency_native_available(void);
/* Call begin only for native color. Existing paper UI must not activate it. */
PORTABLE_FREQUENCY_PRIVATE void portable_frequency_native_begin(void);
PORTABLE_FREQUENCY_PRIVATE void portable_frequency_native_layout(uint64_t layout_id);
PORTABLE_FREQUENCY_PRIVATE void portable_frequency_native_present(bool full);
/* Activate only for the native color presentation; paper keeps its own input.
 * Read only: sequence belongs to the adapter raw provider. Always returns the
 * current generation, consumes no events, and never delays frequency polling,
 * releases or cleanup. Layout IDs must identify the visible hit-target map. */
PORTABLE_FREQUENCY_PRIVATE bool portable_frequency_native_ready(uint64_t *generation,uint64_t *visible_sequence);
/* Read-only recovery hint after foreign pixels: true only when an invalid map
 * can be repainted without overtaking a frame, overlay or return handoff. */
PORTABLE_FREQUENCY_PRIVATE bool portable_frequency_native_repaint(void);
/* Queue ordinary adapter Back. The next poll owns drain, session suspension
 * and return; this call never launches, stops audio or releases providers. */
PORTABLE_FREQUENCY_PRIVATE void portable_frequency_native_back(void);
#undef PORTABLE_FREQUENCY_PRIVATE
#endif
