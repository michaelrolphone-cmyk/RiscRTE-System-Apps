#pragma once
#include <stdint.h>
/* Private app/client receipt, not a Runtime/provider ABI. Zero is unavailable.
 * IDs identify exact completed app images, not merely an idle frame queue. */
#if defined(PORTABLE_FRAME_RECEIPTS)
__attribute__((visibility("hidden"))) uint64_t portable_frame_requested_id(void);
__attribute__((visibility("hidden"))) uint64_t portable_frame_completed_id(void);
#endif
