#pragma once
#include <cstdint>
using BaseType_t=int;
using UBaseType_t=unsigned;
using TickType_t=uint32_t;
constexpr BaseType_t pdTRUE=1,pdFALSE=0,pdPASS=1;
#define pdMS_TO_TICKS(ms) (ms)
struct portMUX_TYPE {int depth=0;};
#define portMUX_INITIALIZER_UNLOCKED {}
inline void portENTER_CRITICAL(portMUX_TYPE *mux){++mux->depth;}
inline void portEXIT_CRITICAL(portMUX_TYPE *mux){--mux->depth;}
