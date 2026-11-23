#pragma once
#include <array>
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
    void end(){++testSpiEnds;}
    int bus_;
};
