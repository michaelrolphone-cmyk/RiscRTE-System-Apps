#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_BATTERY_NATIVE_UI
/* App-private, statically linked adapter contract. Not a Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_BATTERY_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_BATTERY_PRIVATE
#endif
/* Start a new controller invocation without discarding display custody. */
PORTABLE_BATTERY_PRIVATE void portable_battery_native_begin(void);
/* Declare page/detail/geometry identity before attempting frame acquisition.
 * A real change cancels the old gesture; same-layout telemetry does not. */
PORTABLE_BATTERY_PRIVATE void portable_battery_native_layout(uint64_t layout_id);
/* Submit the selected Battery layout with its invocation/layout generation. */
PORTABLE_BATTERY_PRIVATE void portable_battery_native_present(bool full);
#undef PORTABLE_BATTERY_PRIVATE
#endif
