#pragma once
#include <WebServer.h>

namespace camera_light {
// Call after radio_telemetry::begin so Content-Type is collected by WebServer.
// Uses the existing authentication callback; does not alter camera settings.
void begin(WebServer &server, bool (*authorize)());
// Sends off and keeps the remembered color and brightness. No automatic timeout.
bool off();
} // namespace camera_light
