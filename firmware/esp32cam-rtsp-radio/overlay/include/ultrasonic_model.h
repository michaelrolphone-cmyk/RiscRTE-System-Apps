#pragma once
#include <stdint.h>
namespace ultrasonic_model {
constexpr uint32_t TRIGGER_INTERVAL_MS = 65;
constexpr uint32_t TIMEOUT_US = 30000;
constexpr uint32_t STALE_MS = 1000;
// Fixed sound speed, no claim of environmental compensation.
inline float metres(uint32_t pulseUs) { return pulseUs * 0.000170f; }
inline bool inRange(uint32_t pulseUs) {
    return pulseUs >= 118 && pulseUs <= 23529;
}
inline bool fresh(bool sampled, bool valid, bool paused, uint32_t now, uint32_t at) {
    return sampled && valid && !paused && uint32_t(now-at) <= STALE_MS;
}
}
