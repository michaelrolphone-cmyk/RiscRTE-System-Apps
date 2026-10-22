/* Test-only lifecycle observation around unchanged production implementations.
 * Renamed implementation symbols are compile-time instrumentation, not API
 * replacement. All capabilities and stream calls enter the production Runtime. */
#include "PortableApps.h"
#include "PortableSerialMonitor.h"
#include "bridge_fixture.h"
extern int monitor_real_init(void);
extern void monitor_real_fini(void);
extern void monitor_real_main(void);
const t5_app_manifest_t portable_catalog[] = {{.display_name="Fixture", .file_name="default.elf", .compatible=true}};
const unsigned portable_catalog_count = 1;
const portable_serial_monitor_instance portable_serial_monitor_instances[] = {{41,"USB A"},{42,"USB B"}};
const unsigned portable_serial_monitor_instance_count = 2;
__attribute__((visibility("default"))) int app_module_init(void) {
    monitor_fixture_event("app:init");
    int rc = monitor_real_init();
    monitor_fixture_event(rc ? "app:init-failed" : "app:initialized");
    return rc;
}
__attribute__((visibility("default"))) void app_main(void) {
    monitor_fixture_event("app:main");
    monitor_real_main();
    monitor_fixture_event("app:returned");
}
__attribute__((visibility("default"))) void app_module_fini(void) {
    monitor_fixture_event("app:fini");
    monitor_real_fini();
    monitor_fixture_event("app:finalized");
}
__attribute__((destructor)) static void unloaded(void) { monitor_fixture_event("app:unload"); }
