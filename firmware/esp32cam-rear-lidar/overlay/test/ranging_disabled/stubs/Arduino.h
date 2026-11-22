#pragma once
#include <cstdint>
#include <cstdio>
inline uint32_t millis() { return 1234; }
struct TestEsp { uint64_t getEfuseMac() const { return 0x1ca14b858428ULL; } };
extern TestEsp ESP;
