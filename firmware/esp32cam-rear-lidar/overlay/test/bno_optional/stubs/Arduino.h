#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
using String = std::string;
constexpr int LOW=0, HIGH=1, INPUT_PULLUP=2, OUTPUT=3;
extern uint64_t testNowUs;
extern int testPins[49];
extern std::vector<std::pair<int,int>> testPinModes, testPinWrites;
inline uint32_t millis(){return static_cast<uint32_t>(testNowUs/1000);}
inline uint32_t micros(){return static_cast<uint32_t>(testNowUs);}
inline void pinMode(int pin,int mode){testPinModes.emplace_back(pin,mode);}
inline int digitalRead(int pin){return testPins[pin];}
inline void digitalWrite(int pin,int value){testPins[pin]=value;testPinWrites.emplace_back(pin,value);}
