#pragma once
#include <freertos/FreeRTOS.h>
struct TestSemaphore {bool held=false;};
using SemaphoreHandle_t=TestSemaphore *;
extern TestSemaphore testSnapshotMutex;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return &testSnapshotMutex;}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex,TickType_t){
 if(mutex->held)return pdFALSE;mutex->held=true;return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t mutex){mutex->held=false;}
