#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef PORTABLE_ALARMS_NATIVE_UI
/* App-private, statically linked adapter contract. Not a Runtime/T5 ABI. */
#if defined(__GNUC__)
#define PORTABLE_ALARMS_PRIVATE __attribute__((visibility("hidden")))
#else
#define PORTABLE_ALARMS_PRIVATE
#endif
/* Start a new controller invocation without discarding display custody. */
PORTABLE_ALARMS_PRIVATE void portable_alarms_native_begin(void);
/* Declare page/detail/geometry identity before attempting frame acquisition.
 * A real change cancels the old gesture; same-layout telemetry does not. */
PORTABLE_ALARMS_PRIVATE void portable_alarms_native_layout(uint64_t layout_id);
/* Submit the selected Alarms layout with its invocation/layout generation. */
PORTABLE_ALARMS_PRIVATE void portable_alarms_native_present(bool full);
#undef PORTABLE_ALARMS_PRIVATE
#endif
