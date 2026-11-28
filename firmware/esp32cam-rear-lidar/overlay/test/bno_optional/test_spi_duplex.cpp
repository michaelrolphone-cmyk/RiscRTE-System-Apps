#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

struct sh2_Hal_t {};
enum { LOW, HIGH, MSBFIRST, SPI_MODE3 };
constexpr int _int_pin=3, _cs=40, _spiPortSpeed=1000000;
static uint8_t rearSpiReply[384];
static unsigned rearSpiReplySize;
static uint32_t rearSpiReplyTime;
static int32_t rearSpiDiagnostic[8];
static int attention=LOW, selections=0;
static bool selected=false;
static size_t offset=0;
static std::vector<uint8_t> incoming, outbound;
struct SPISettings {SPISettings(int,int,int){}};

uint32_t micros(){return 12345;}
void delay(int){}
int digitalRead(int pin){assert(pin==_int_pin);return attention;}
void digitalWrite(int pin,int level){
    assert(pin==_cs);
    selected=level==LOW;
    if(selected){++selections;attention=HIGH;offset=0;}
}
struct Spi {
    void beginTransaction(SPISettings){}
    void endTransaction(){assert(!selected);}
    void transfer(uint8_t *bytes,unsigned size){
        assert(selected);
        for(unsigned i=0;i<size;++i,++offset){
            outbound.push_back(bytes[i]);
            bytes[i]=offset<incoming.size()?incoming[offset]:0;
        }
    }
} spi;
auto *_spiPort=&spi;

#include "production_spi.inc"

int main(){
    uint8_t request[]={5,0,2,7,0xf9};
    incoming={9,0x80,1,6,0xf8,1,2,3,4};
    assert(spihal_write(nullptr,request,sizeof(request))==5);
    assert((outbound==std::vector<uint8_t>{5,0,2,7,0xf9,0,0,0,0}));
    assert(attention==HIGH && selections==1 && rearSpiReplySize==9);
    assert(spihal_write(nullptr,request,sizeof(request))==0);
    uint8_t response[32]={};
    uint32_t timestamp=0;
    assert(spihal_read(nullptr,response,sizeof(response),&timestamp)==9);
    assert(timestamp==12345 && selections==1 && rearSpiReplySize==0);
    assert(!memcmp(response,incoming.data(),incoming.size()));

    attention=LOW;
    incoming={0xff,0xff,0xff,0xff};
    outbound.clear();
    assert(spihal_write(nullptr,request,sizeof(request))==5);
    assert(outbound.size()==sizeof(request) && rearSpiReplySize==0);
    assert(rearSpiDiagnostic[6]==2 && rearSpiDiagnostic[7]==1);
    assert(spihal_read(nullptr,response,sizeof(response),&timestamp)==0);

    attention=LOW;
    incoming={7,0,2,1,0xf8,8,9};
    outbound.clear();
    assert(spihal_read(nullptr,response,sizeof(response),&timestamp)==7);
    assert(selections==3 && outbound==std::vector<uint8_t>(7,0));
    assert(!memcmp(response,incoming.data(),incoming.size()));
}
