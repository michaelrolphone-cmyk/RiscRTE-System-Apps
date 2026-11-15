#pragma once
#include <WebServer.h>
namespace sensor_push { void offerCamera(uint32_t intervalMs); void begin(WebServer &server, bool (*authorize)()); }
