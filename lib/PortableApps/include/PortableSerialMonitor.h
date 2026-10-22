#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Local, unmanifested development controller. The composition supplies only
 * explicitly authorized, nonzero provider instances. This is not discovery,
 * a device identity or product admission policy. No default instance is chosen. */
#define PORTABLE_SERIAL_MONITOR_MAX_INSTANCES 8u
typedef struct {
    uint64_t instance;
    const char *label;
} portable_serial_monitor_instance;
extern const portable_serial_monitor_instance portable_serial_monitor_instances[];
extern const unsigned portable_serial_monitor_instance_count;
/* Owner-task only. Used before launch and before all adapter finalization.
 * False is terminal retained custody, never a request to retry. */
bool portable_serial_monitor_close(void);
