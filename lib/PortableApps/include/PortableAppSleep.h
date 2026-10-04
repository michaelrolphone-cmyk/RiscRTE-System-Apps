#pragma once
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscBatteryGaugeV1.h"
/* Optional deployment-local app client hook, like PortableNavigation. It is
 * compiled only by an explicit target deployment. No device names, storage
 * privileges or firmware UI enter the generic adapter. Caller closes touch
 * subscriptions and holds no display lease; app RAM and grants remain live.
 * 1: woke, 0: ordinary refusal with display restored, -1: stop invocation.
 * A retained native error is returned without provider restore/normal I/O;
 * the runtime's pre-fini barrier must retain the app and dependency graph. */
int portable_app_sleep(const risc_runtime_api_v1 *runtime,
                       const risc_display_output_api_v1 *display,
                       const risc_battery_gauge_api_v1 *gauge);
