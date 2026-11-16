#pragma once
#include <ArduinoJson.h>
#include <Wire.h>

namespace bno_telemetry {
void begin(TwoWire &bus);
void writeJson(JsonObject out, uint64_t at);
// Command handling from socket / HTTP. Returns true if handled.
bool handleCommand(JsonObjectConst cmd, JsonObject reply);
} // namespace bno_telemetry
