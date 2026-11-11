#include "RiscRuntimeV1.h"
extern void paper_test_child(void);
__attribute__((visibility("default"))) void app_main(void) {
    const risc_runtime_api_v1 *rt = risc_runtime_get_api(1);
    risc_runtime_capability_v1 grant = {0}; grant.struct_size = sizeof(grant);
    /* Parent grants must not leak into this ungranted child. */
    if (!rt || rt->acquire("display.output", 1, 0, &grant)) return;
    paper_test_child(); rt->diagnostic("PAPER_CHILD returned grants=none");
}
