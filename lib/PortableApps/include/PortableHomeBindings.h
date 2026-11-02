#pragma once
#include <stdint.h>
/* Provider-owned saved-data bindings, loaded only in foreground application
 * work. No display/device property selects an application view or namespace. */
uint64_t portable_home_crash_namespace(void);
uint64_t portable_home_catalog_volume(void);
unsigned portable_home_points_schema(void);
