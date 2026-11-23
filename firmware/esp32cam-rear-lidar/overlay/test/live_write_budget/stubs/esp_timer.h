#pragma once
#include <cstdint>
#include <functional>

extern uint64_t transportTestClockUs;
extern std::function<void()> transportTestOnDelay;

inline int64_t esp_timer_get_time() { return transportTestClockUs; }

// Yield to a controlled peer/independent-socket action at each simulated tick.
// The production writer and all send/recv/backpressure operations remain real.
inline void vTaskDelay(unsigned ticks) {
    transportTestClockUs += ticks * 1000;
    if (transportTestOnDelay) transportTestOnDelay();
}
