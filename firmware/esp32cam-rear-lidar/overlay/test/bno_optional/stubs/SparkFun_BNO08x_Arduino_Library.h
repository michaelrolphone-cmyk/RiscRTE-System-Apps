#pragma once
#include <cassert>
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include "SPI.h"
#include "sh2.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
inline bool rearBnoSpiPending(){return false;}
inline void rearBnoSpiDiagnostics(int32_t *out){for(int i=0;i<10;++i)out[i]=0;}
struct TestEvent {sh2_SensorValue_t value{};int decodeStatus=SH2_OK;};
struct TestImu {
    bool beginResult=true,commandResult=true,reset=false;
    int begins=0,closes=0,services=0,decodes=0;
    std::deque<bool> reportResults;
    std::deque<TestEvent> events;
    std::vector<std::pair<int,unsigned>> reports;
    std::vector<std::pair<std::string,int>> commands;
    std::function<void()> duringBegin,duringCommand;
};
extern TestImu testImu;
extern sh2_SensorCallback_t *testCallback;
extern void *testCallbackCookie;
class BNO08x {
public:
    sh2_ProductIds_t prodIds{};
    bool beginSPI(uint8_t cs,uint8_t interrupt,uint8_t reset,uint32_t speed,SPIClass &spi){
        assert(cs==40&&interrupt==3&&reset==46&&speed==1000000&&spi.bus_==HSPI);
        ++testImu.begins;
        if(testImu.duringBegin)testImu.duringBegin();
        prodIds.numEntries=1;prodIds.entry[0].swPartNumber=10003660;
        prodIds.entry[0].swVersionMajor=1;prodIds.entry[0].swVersionMinor=2;
        prodIds.entry[0].swVersionPatch=3;
        return testImu.beginResult;
    }
    bool wasReset(){bool value=testImu.reset;testImu.reset=false;return value;}
    bool serviceBus(){
        ++testImu.services;
        while(!testImu.events.empty()){
            assert(testCallback);sh2_SensorEvent_t event{};
            testCallback(testCallbackCookie,&event);testImu.events.pop_front();
        }
        return true;
    }
    bool report(int id,unsigned ms){
        assert(testCallback);testImu.reports.emplace_back(id,ms);
        if(testImu.reportResults.empty())return true;
        bool result=testImu.reportResults.front();testImu.reportResults.pop_front();return result;
    }
    bool enableRotationVector(unsigned ms){return report(SH2_ROTATION_VECTOR,ms);}
    bool enableAccelerometer(unsigned ms){return report(SH2_ACCELEROMETER,ms);}
    bool enableGyro(unsigned ms){return report(SH2_GYROSCOPE_CALIBRATED,ms);}
    bool enableMagnetometer(unsigned ms){return report(SH2_MAGNETIC_FIELD_CALIBRATED,ms);}
    bool command(const char *op,int value=0){
        testImu.commands.emplace_back(op,value);
        if(testImu.duringCommand)testImu.duringCommand();
        return testImu.commandResult;
    }
    bool setCalibrationConfig(uint8_t mask){return command("calibration",mask);}
    bool saveCalibration(){return command("save_calibration");}
    bool tareNow(bool z){return command("tare_now",z);}
    bool saveTare(){return command("save_tare");}
    bool clearTare(){return command("clear_tare");}
};
