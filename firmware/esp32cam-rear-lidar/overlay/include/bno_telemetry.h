#pragma once
#include <ArduinoJson.h>
#include <WebServer.h>

namespace bno_telemetry {
// Optional GY-BNO08x. Only its worker accesses the SPI/SH2 singleton.
void begin(WebServer &server, bool (*authorize)());
void pause(bool value);
// Snapshot only: never touches hardware or waits for a command.
void writeJson(JsonObject output, uint64_t atUs);
// Accepts work into a bounded queue; success means accepted, not executed.
// Completion appears in telemetry.last_command with the returned command_id.
bool enqueueCommand(JsonObjectConst command, JsonObject reply);
}
