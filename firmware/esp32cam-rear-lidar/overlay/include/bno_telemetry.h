#pragma once
#include <ArduinoJson.h>

namespace bno_telemetry {
void begin();
void writeJson(JsonObject out, uint64_t at);
// Command handling from socket. Returns true if handled.
bool handleCommand(JsonObjectConst cmd, JsonObject reply);
} // namespace bno_telemetry
