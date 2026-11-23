#pragma once
#include <cstdint>
extern uint64_t testNowUs;
inline int64_t esp_timer_get_time(){return static_cast<int64_t>(testNowUs);}
