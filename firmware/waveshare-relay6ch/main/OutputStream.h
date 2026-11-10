#pragma once
#include <ArduinoJson.h>
namespace BiomeRelay {
 class OutputStream {
  struct State;State* state=nullptr;
 public:
  bool attach(JsonObjectConst grant);
  void tick();
  void close();
  bool active()const;
 };
}
