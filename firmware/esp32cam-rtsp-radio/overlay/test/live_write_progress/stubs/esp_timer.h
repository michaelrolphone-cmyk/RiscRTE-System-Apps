#pragma once
#include <cstdint>
extern uint64_t transportTestClockUs;
inline int64_t esp_timer_get_time() { return transportTestClockUs; }
inline void vTaskDelay(unsigned ticks) { transportTestClockUs += ticks * 1000; }
