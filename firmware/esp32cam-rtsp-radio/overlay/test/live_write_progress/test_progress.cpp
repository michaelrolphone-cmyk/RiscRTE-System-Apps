#include <cassert>

#include "../../src/live_write_progress.h"

int main() {
    LiveWriteProgress video(100);
    LiveWriteProgress lidar(100);
    assert(!video.stalled(100 + LiveWriteProgress::idleLimitUs - 1));

    lidar.wroteBytes(900100);
    assert(video.stalled(100 + LiveWriteProgress::idleLimitUs));
    assert(!lidar.stalled(100 + LiveWriteProgress::idleLimitUs));

    LiveWriteProgress progressingVideo(100);
    for (uint64_t now = 800100; now <= 4000100; now += 800000) {
        assert(!progressingVideo.stalled(now));
        progressingVideo.wroteBytes(now);
    }
    assert(progressingVideo.stalled(4000100 + LiveWriteProgress::idleLimitUs));
}
