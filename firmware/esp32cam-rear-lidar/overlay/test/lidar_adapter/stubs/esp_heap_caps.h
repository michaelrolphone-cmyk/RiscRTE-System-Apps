#pragma once
#include <cstddef>
#include <cstdint>
constexpr uint32_t MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
void *heap_caps_calloc(size_t count,size_t size,uint32_t capabilities);
