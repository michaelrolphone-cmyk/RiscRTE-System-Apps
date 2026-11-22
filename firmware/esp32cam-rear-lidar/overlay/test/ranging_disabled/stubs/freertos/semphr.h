#pragma once
#include "FreeRTOS.h"
struct TestMutex { bool held = false; };
using SemaphoreHandle_t = TestMutex *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static TestMutex mutex; return &mutex; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t) {
    if (mutex->held) return pdFALSE;
    mutex->held = true;
    return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t mutex) { mutex->held = false; }
