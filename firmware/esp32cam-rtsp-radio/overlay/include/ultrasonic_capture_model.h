#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ultrasonic_capture {
struct Sample {
    uint64_t observedUs = 0, completedUs = 0;
    uint32_t at = 0, pulse = 0, sequence = 0;
    bool sampled = false, valid = false;
    const char *status = "not_sampled";
};

template <size_t N>
Sample selectAt(const Sample (&history)[N], uint64_t captureUs) {
    Sample result;
    for (const auto &sample : history) {
        if (sample.sampled && sample.observedUs <= captureUs &&
            sample.completedUs <= captureUs && sample.completedUs >= sample.observedUs &&
            (!result.sampled || sample.completedUs > result.completedUs)) result = sample;
    }
    return result;
}
}
