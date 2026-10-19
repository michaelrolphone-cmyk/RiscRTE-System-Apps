#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_FILES_NATIVE_UI
/* Statically linked Files-only presentation ownership; no Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_FILES_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_FILES_PRIVATE
#endif
/* Call begin only for native color. Existing paper UI must not activate it. */
PORTABLE_FILES_PRIVATE void portable_files_native_begin(void);
PORTABLE_FILES_PRIVATE void portable_files_native_layout(uint64_t layout_id);
PORTABLE_FILES_PRIVATE void portable_files_native_present(bool full);
/* Activate only for the native color presentation; paper keeps its own input.
 * Read only: sequence belongs to the adapter raw provider. Always returns the
 * current generation, consumes no events, and never delays files polling,
 * releases or cleanup. Layout IDs must identify the visible hit-target map. */
PORTABLE_FILES_PRIVATE bool portable_files_native_ready(uint64_t *generation,uint64_t *visible_sequence);
#undef PORTABLE_FILES_PRIVATE
#endif
