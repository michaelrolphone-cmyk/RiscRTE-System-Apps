#pragma once
#include "Hardware.h"
namespace RelayBoard {bool begin();int poll(void*,JsonObject);bool cancel(void*);bool ready(void*);int execute(void*,const char*,JsonObjectConst,JsonObject);}
