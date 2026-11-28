#pragma once
#include <WebServer.h>
namespace sensor_push {
void offerCamera(uint32_t intervalMs);
void begin(WebServer &,bool (*)());
void pause(bool);
void bootProgress(uint32_t &cameraAcknowledged, uint32_t &sensorPackets);
}
