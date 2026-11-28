#include <assert.h>
#include <stdint.h>
#include "rear_boot_health.h"

using rear_boot_health::Action;
using rear_boot_health::Trial;

int main() {
    Trial healthy(0);
    for (uint32_t i = 1; i < 4; ++i)
        assert(healthy.observe(i * 5000, true, i, i) == Action::wait);
    assert(healthy.observe(20000, true, 4, 4) == Action::accept);

    Trial stalledCamera(0), stalledSensors(0), disconnected(0);
    for (uint32_t i = 1; i < 15; ++i) {
        assert(stalledCamera.observe(i * 5000, true, 0, i) == Action::wait);
        assert(stalledSensors.observe(i * 5000, true, i, 0) == Action::wait);
        assert(disconnected.observe(i * 5000, false, i, i) == Action::wait);
    }
    assert(stalledCamera.observe(75000, true, 0, 15) == Action::rollback);
    assert(stalledSensors.observe(75000, true, 15, 0) == Action::rollback);
    assert(disconnected.observe(75000, false, 15, 15) == Action::rollback);

    Trial intermittent(UINT32_MAX - 8000);
    const uint32_t start = UINT32_MAX - 8000;
    assert(intermittent.observe(start + 5000, true, 1, 1) == Action::wait);
    assert(intermittent.observe(start + 10000, true, 2, 2) == Action::wait);
    assert(intermittent.observe(start + 15000, false, 2, 2) == Action::wait);
    assert(intermittent.observe(start + 20000, true, 3, 3) == Action::wait);
    assert(intermittent.observe(start + 25000, true, 4, 4) == Action::wait);
    assert(intermittent.observe(start + 30000, true, 5, 5) == Action::wait);
    assert(intermittent.observe(start + 34999, true, 6, 6) == Action::wait);
    assert(intermittent.observe(start + 35000, true, 6, 6) == Action::accept);
}
