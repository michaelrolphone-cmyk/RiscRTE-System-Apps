#pragma once
#include <cstddef>
#include <cstdint>
constexpr int SERIAL_8N1=0;
class HardwareSerial {
public:
 explicit HardwareSerial(int){}
 void setRxBufferSize(size_t){}
 void begin(uint32_t,int,int,int){}
 int available() const{return 0;}
 int read(){return -1;}
 int availableForWrite() const{return 0;}
 size_t write(const uint8_t *,size_t count){return count;}
};
