#include <ArduinoJson.h>
#include "bootstrap/Runtime.h"
#define private public
#include "ports/esp32s3/CpuPort.h"
#undef private
#include "../../Services/infra_node/Station.h"
#include <RiscProviderV2.h>
#ifdef NDEBUG
#error Infrastructure tests require active assertions after product headers
#endif
#include <cassert>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdio>
using namespace RiscCpu;
extern "C" const risc_driver_v2* t5_driver_get(uint32_t);
static bool active=false;static unsigned begins=0,lastKind=0;
static risc_native_radio_async_v1 backend{
 sizeof(backend),RISC_RADIO_ASYNC_TAG,1,
 [](const risc_radio_request_v1* r,uint32_t* op)->int32_t{assert(!active);assert(r->kind==RISC_RADIO_REQUEST_AP||r->kind==RISC_RADIO_REQUEST_JOIN);assert(r->ssid[0]&&strlen(r->password)>=8);lastKind=r->kind;++begins;active=true;*op=begins;return RISC_RADIO_ACCEPTED;},
 [](uint32_t id,risc_radio_progress_v1* p)->int32_t{*p={};p->struct_size=sizeof(*p);p->operation_id=id;p->scan.struct_size=sizeof(p->scan);p->phase=active?RISC_RADIO_CONNECTED:RISC_RADIO_IDLE;p->owned=active;p->quiescent=!active;p->station_state=active?2:0;return active?RISC_RADIO_PENDING:RISC_RADIO_QUIESCENT;},
 [](uint32_t)->int32_t{active=false;return RISC_RADIO_QUIESCENT;},
 [](){return true;},[](){return true;},[](){return true;},[](){}
};
int main(int argc,char**argv){
 assert(argc==2);std::ifstream input(argv[1]);std::string bytes{std::istreambuf_iterator<char>(input),{}};JsonDocument board;assert(!deserializeJson(board,bytes.c_str()));
 auto config=board["devices"][0]["config"];assert(config["unit"]==0&&config["features"]==3);
 Hardware hardware{};hardware.owner=[](){return true;};hardware.radioIdle=[](){return !active;};hardware.radioAsync=&backend;
 Port port(hardware);auto& radio=port.radios_[0];radio.port=&port;radio.config={sizeof(radio.config),0,config["features"].as<uint32_t>()};
 garden_radio_async_v1 api{};api.base.api_version=1;api.base.struct_size=sizeof(api);api.base.context=&radio;api.base.claim=Port::radioClaim;api.base.release=Port::radioRelease;api.async_tag=RISC_RADIO_ASYNC_TAG;api.async_version=1;api.begin=Port::radioAsyncBegin;api.poll=Port::radioAsyncPoll;api.cancel=Port::radioAsyncCancel;
 risc_hw_radio_v1 typed=radio.config;
 risc_hardware_device_v1 device{};device.struct_size=sizeof(device);device.instance_id=board["devices"][0]["instance_id"];device.compatible="espressif,esp32s3-wifi";device.config_type="radio.integrated";device.config_version=1;device.config_size=sizeof(typed);device.config=&typed;
 risc_provider_dependency_v1 deps[]={{"platform.radio",1,&api},{"hardware.device",1,&device}};auto driver=t5_driver_get(2);assert(driver);
 // The product must reject a station-only installation before claiming radio.
 typed.features=1;assert(!driver->start(deps,2));assert(!radio.token&&!begins);typed.features=3;
 assert(driver->start(deps,2));auto station=static_cast<const infra_station_v1*>(driver->capability);
 // Reproduce the old board/Core mismatch: real CpuPort denies AP before SDK I/O.
 radio.config.features=1;assert(station->access_point(station->context,"RiscRTE-Setup","RiscRTE-Setup")<0);assert(!begins&&!radio.token);
 radio.config.features=typed.features;
 assert(station->access_point(station->context,"RiscRTE-Setup","RiscRTE-Setup")==1);assert(lastKind==RISC_RADIO_REQUEST_AP);
 assert(station->access_point(station->context,"RiscRTE-Setup","RiscRTE-Setup")==0);assert(active);
 assert(station->close(station->context)==0);assert(!active&&!radio.token);
 assert(station->connect(station->context,"Home","test-password")==1);assert(lastKind==RISC_RADIO_REQUEST_JOIN);
 assert(station->poll(station->context)==0);assert(station->close(station->context)==0);
 assert(driver->quiesce());driver->stop();
 puts("Generated board + actual infra-station + real CpuPort: station-only AP denied; portal AP and subsequent station join admitted; cleanup PASS");
}
