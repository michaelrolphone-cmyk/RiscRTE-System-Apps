#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_LORA_NATIVE_UI
/* Statically linked LoRa-only presentation ownership; no Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_LORA_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_LORA_PRIVATE
#endif
/* Begin only succeeds for the capability-selected RGB565 surface. A false
 * result leaves the existing legacy/paper input path untouched. */
PORTABLE_LORA_PRIVATE bool portable_lora_native_begin(void);
PORTABLE_LORA_PRIVATE void portable_lora_native_layout(uint64_t layout_id);
PORTABLE_LORA_PRIVATE void portable_lora_native_present(bool full);
/* Activate only for the native color presentation; paper keeps its own input.
 * Read only: sequence belongs to the adapter raw provider. Always returns the
 * current generation, consumes no events, and never delays lora polling,
 * releases or cleanup. Layout IDs must identify the visible hit-target map. */
PORTABLE_LORA_PRIVATE bool portable_lora_native_ready(uint64_t *generation,uint64_t *visible_sequence);
/* Original physical DOWN for this delivered eligible release, including a
 * Quick top-edge replay. Layout changes discard the witness. No I/O. */
PORTABLE_LORA_PRIVATE bool portable_lora_native_tap_origin(uint16_t *x,uint16_t *y);
#undef PORTABLE_LORA_PRIVATE
#endif
