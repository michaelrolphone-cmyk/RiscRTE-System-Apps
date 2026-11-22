#pragma once
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using TaskHandle_t = void *;
using TaskFunction_t = void (*)(void *);
constexpr BaseType_t pdPASS = 1;
#define pdMS_TO_TICKS(ms) (ms)

constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFALSE = 0;
struct portMUX_TYPE {};
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(lock) do { (void)(lock); } while (0)
#define portEXIT_CRITICAL(lock) do { (void)(lock); } while (0)
