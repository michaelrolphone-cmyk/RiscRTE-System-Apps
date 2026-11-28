#pragma once
#include <array>
#include <cassert>
#include "Arduino.h"
constexpr int MSBFIRST=1, SPI_MODE3=3;
struct SPISettings { SPISettings(int hz,int order,int mode){assert(hz==1000000 && order==MSBFIRST && mode==SPI_MODE3);} };
#include <vector>
constexpr int HSPI=1;
extern int testSpiBegins, testSpiEnds;
extern std::vector<std::array<int,4>> testSpiRoutes;
class SPIClass {
public:
    explicit SPIClass(int bus):bus_(bus){}
    void begin(int sck,int miso,int mosi,int cs){
        ++testSpiBegins;testSpiRoutes.push_back({{sck,miso,mosi,cs}});
    }
    void beginTransaction(SPISettings){}
    uint8_t transfer(uint8_t b){assert(testPins[46]==LOW && testPins[40]==HIGH);return b;}
    void endTransaction(){}
    void end(){++testSpiEnds;}
    int bus_;
};
