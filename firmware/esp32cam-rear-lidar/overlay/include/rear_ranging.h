#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
namespace rear_ranging {
void begin(WebServer &,bool (*)());
void pause(bool);
void writeJson(JsonObject,uint64_t captureUs);
// Pair only after a successful begin; the sole IotWebConf main-loop call owns
// this interval. Status snapshots use a separate lock.
bool beginNetworkLoop();
void endNetworkLoop();
}
