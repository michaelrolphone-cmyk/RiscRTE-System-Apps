#pragma once
#include <stdint.h>

namespace rear_boot_health {
enum class Action { wait, accept, rollback };

class Trial {
 public:
    explicit Trial(uint32_t bootMs) : started(bootMs), sampled(bootMs) {}

    Action observe(uint32_t now, bool station, uint32_t cameraAcks, uint32_t sensorPackets) {
        if (now - started >= 75000) return Action::rollback;
        if (now - sampled < 5000) return Action::wait;
        const bool progressed = station && cameraAcks != lastCamera && sensorPackets != lastSensor;
        streak = progressed ? streak + 1 : 0;
        sampled = now;
        lastCamera = cameraAcks;
        lastSensor = sensorPackets;
        return streak >= 4 ? Action::accept : Action::wait;
    }

 private:
    uint32_t started, sampled, lastCamera = 0, lastSensor = 0;
    unsigned streak = 0;
};
}
