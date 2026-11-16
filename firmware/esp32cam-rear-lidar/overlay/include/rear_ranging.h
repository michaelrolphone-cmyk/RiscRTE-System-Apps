#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
namespace rear_ranging {
void begin(WebServer &,bool (*)());
void pause(bool);
void writeJson(JsonObject,uint64_t captureUs);
}
