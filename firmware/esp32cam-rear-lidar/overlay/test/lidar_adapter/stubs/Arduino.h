#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include <freertos/task.h>
using byte=uint8_t;
using String=std::string;
constexpr int LOW=0,HIGH=1,OUTPUT=3;
extern uint64_t testNowUs;
extern int testPins[49];
extern std::vector<std::pair<int,int>> testPinWrites;
inline uint32_t millis(){return static_cast<uint32_t>(testNowUs/1000);}
inline void pinMode(int,int){}
inline void digitalWrite(int pin,int value){testPins[pin]=value;testPinWrites.emplace_back(pin,value);}
struct TestESP {
 uint64_t getEfuseMac() const{return 0x1ca14b858428ULL;}
 uint32_t getFlashChipSize() const{return 16*1024*1024;}
 uint32_t getPsramSize() const{return 8*1024*1024;}
};
extern TestESP ESP;
inline String testString(const char *value){return value;}
inline String testString(unsigned long long value){return std::to_string(value);}
// Keep ArduinoJson's real std::string support; adapt only Arduino constructor syntax.
#define String(value) testString(value)
