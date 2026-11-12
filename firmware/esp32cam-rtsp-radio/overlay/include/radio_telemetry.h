#pragma once

#include <WebServer.h>
#include <ArduinoJson.h>

namespace radio_telemetry {
// Registers authenticated telemetry routes and starts the bounded radio worker.
// authorize must send the authentication response itself and return false on denial.
void begin(WebServer &server, bool (*authorize)());
void loop();
// Requests pause; an in-flight short operation may finish before the worker stops.
// This never waits for Bluetooth host initialization.
void pause(bool paused);
bool writeFrameJson(JsonObject output, uint64_t captureUs);
} // namespace radio_telemetry

