#pragma once
#include <cstdint>
extern uint64_t transportTestClockUs, timingTestDelayExtraUs;
inline int64_t esp_timer_get_time() { return transportTestClockUs; }
// Include deterministic wake-up latency, not merely the requested sleep.
inline void vTaskDelay(unsigned ticks) {
    transportTestClockUs += ticks * 1000 + timingTestDelayExtraUs;
}
