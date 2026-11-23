#pragma once
#include "FreeRTOS.h"
#include <cstring>
#include <deque>
#include <vector>
struct TestQueue {size_t capacity,itemSize;std::deque<std::vector<unsigned char>> items;};
using QueueHandle_t=TestQueue *;
extern bool testQueueFailure;
inline QueueHandle_t xQueueCreate(UBaseType_t count,UBaseType_t itemSize){
    return testQueueFailure?nullptr:new TestQueue{count,itemSize,{}};
}
inline BaseType_t xQueueSend(QueueHandle_t queue,const void *item,TickType_t){
    if(!queue||queue->items.size()>=queue->capacity)return pdFALSE;
    const auto *first=static_cast<const unsigned char *>(item);
    queue->items.emplace_back(first,first+queue->itemSize);return pdTRUE;
}
inline BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t){
    if(!queue||queue->items.empty())return pdFALSE;
    std::memcpy(item,queue->items.front().data(),queue->itemSize);
    queue->items.pop_front();return pdTRUE;
}
