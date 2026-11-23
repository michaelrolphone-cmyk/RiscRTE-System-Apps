#pragma once
#include "FreeRTOS.h"
#include <string>
#include <vector>
struct TestTask {std::string name;unsigned stack;unsigned priority;};
extern std::vector<TestTask> testTasks;
extern bool testTaskFailure;
inline BaseType_t xTaskCreate(void (*)(void *),const char *name,unsigned stack,
                             void *,UBaseType_t priority,void *){
    if(testTaskFailure)return pdFALSE;
    testTasks.push_back({name,stack,priority});return pdPASS;
}
inline void vTaskDelay(TickType_t){}
