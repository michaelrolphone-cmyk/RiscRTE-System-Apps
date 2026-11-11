#pragma once
#include <ArduinoJson.h>
#include "FreshIntent.h"
namespace BiomeRelay {
 class OutputStream {
  struct State;State* state=nullptr;
  FreshIntent retainedIntent;
 public:
  bool attach(JsonObjectConst grant);
  void tick();
  void close();
  bool active()const;
 };
}
