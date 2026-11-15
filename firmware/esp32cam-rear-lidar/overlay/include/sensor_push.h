#pragma once
#include <WebServer.h>
namespace sensor_push { void begin(WebServer &,bool (*)()); void pause(bool); }
