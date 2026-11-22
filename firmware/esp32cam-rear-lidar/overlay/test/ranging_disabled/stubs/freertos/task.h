#pragma once
#include "FreeRTOS.h"
#include <string>
#include <vector>
struct TestTask { TaskFunction_t callback; std::string name; UBaseType_t priority; };
extern std::vector<TestTask> testTasks;
inline BaseType_t xTaskCreate(TaskFunction_t callback, const char *name, uint32_t,
                              void *, UBaseType_t priority, TaskHandle_t *) {
    testTasks.push_back({callback, name, priority});
    return pdPASS;
}
inline void vTaskDelay(TickType_t) {}
