#pragma once
#include <cstdint>
extern uint64_t transportTestClockUs;
extern thread_local bool diagnosticInCallback;
inline int64_t esp_timer_get_time() {
    const uint64_t now = transportTestClockUs;
    if (diagnosticInCallback) transportTestClockUs += 3;
    return static_cast<int64_t>(now);
}
inline void vTaskDelay(unsigned ticks) { transportTestClockUs += ticks * 1000; }
