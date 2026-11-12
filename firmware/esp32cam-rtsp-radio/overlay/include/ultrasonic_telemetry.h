#pragma once
#include <ArduinoJson.h>
namespace ultrasonic_telemetry {
void begin();
void pause(bool paused);
void writeJson(JsonObject output, bool compact = false);
}
