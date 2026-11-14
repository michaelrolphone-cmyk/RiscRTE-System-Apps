#pragma once
#include <WebServer.h>
namespace sensor_push { void begin(WebServer &server, bool (*authorize)()); }
