#pragma once
#include <WebServer.h>
namespace camera_exposure { void begin(WebServer &server, bool (*authorize)()); }
