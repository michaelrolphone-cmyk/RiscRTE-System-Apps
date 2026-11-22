#pragma once

#include <cstdint>

// A TCP connection can remain established even when its peer stops reading.
// Reset only the stalled socket so its latest-value mailbox can make progress.
class LiveWriteProgress {
public:
    static constexpr uint64_t idleLimitUs = 1000000;

    explicit LiveWriteProgress(uint64_t nowUs) : lastProgressUs(nowUs) {}

    void wroteBytes(uint64_t nowUs) { lastProgressUs = nowUs; }

    bool stalled(uint64_t nowUs) const {
        return nowUs - lastProgressUs >= idleLimitUs;
    }

private:
    uint64_t lastProgressUs;
};
