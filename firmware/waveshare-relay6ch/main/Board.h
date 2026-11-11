#pragma once
#include "Hardware.h"
namespace RelayBoard {bool begin();int poll(void*,JsonObject);bool cancel(void*);bool ready(void*);int execute(void*,const char*,JsonObjectConst,JsonObject);}

namespace RelayBoard {
 bool streamBegin();void streamDesired(uint8_t,uint64_t,uint64_t);void streamNeutral();void streamEnd();
 uint64_t streamApplied();uint8_t streamOutput();
}
