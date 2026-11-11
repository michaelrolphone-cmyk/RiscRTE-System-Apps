#include "RiscProviderV2.h"
#ifndef TEST_SLOT
#define TEST_SLOT 0
#endif
extern const void *paper_test_api(unsigned slot);
extern void paper_test_provider_start(unsigned slot);
extern bool paper_test_provider_quiesce(unsigned slot);
extern void paper_test_provider_stop(unsigned slot);
static bool start(const risc_provider_dependency_v1 *deps, size_t count) {
    (void)deps;
    if (count) return false;
    paper_test_provider_start(TEST_SLOT); return true;
}
static bool quiesce(void) { return paper_test_provider_quiesce(TEST_SLOT); }
static void stop(void) { paper_test_provider_stop(TEST_SLOT); }
static risc_driver_v2 driver = {2, sizeof(driver), TEST_ID, TEST_CAPABILITY, 1, 0, start, stop, quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    driver.capability = paper_test_api(TEST_SLOT);
    return abi == 2 ? &driver : 0;
}
