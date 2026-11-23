#pragma once
#include <cstdint>
#include <set>
class TwoWire {
 uint8_t address=0;
public:
 explicit TwoWire(int=0){}
 std::set<uint8_t> acknowledged{0x29};
 bool begin(int,int,uint32_t){return true;}
 void setTimeOut(uint16_t){}
 void beginTransmission(uint8_t value){address=value;}
 uint8_t endTransmission(){return acknowledged.count(address)?0:2;}
};
extern TwoWire Wire;
