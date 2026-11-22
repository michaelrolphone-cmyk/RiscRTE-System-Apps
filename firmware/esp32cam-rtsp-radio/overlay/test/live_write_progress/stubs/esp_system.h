#pragma once
#include <cstddef>
#include <cstdint>
inline void esp_fill_random(void *buffer, size_t size) {
    uint8_t *out = static_cast<uint8_t *>(buffer);
    for (size_t i = 0; i < size; ++i) out[i] = static_cast<uint8_t>(i + 1);
}
