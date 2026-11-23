#pragma once
#include <cstdint>
#include <cstddef>
#define PROGMEM
using byte = uint8_t;
inline uint8_t highByte(uint16_t value) { return value >> 8; }
inline uint8_t lowByte(uint16_t value) { return value & 255; }
uint32_t millis();
void delay(uint32_t milliseconds);
