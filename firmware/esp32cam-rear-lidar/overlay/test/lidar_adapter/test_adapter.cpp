#include <ArduinoJson.h>
#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <stdexcept>

// Real library declarations and data structures above; only hardware operations
// below are faked. The actual radio adapter is included unchanged further below.
uint64_t testNowUs=1000000;
int testPins[49]={};
std::vector<std::pair<int,int>> testPinWrites;
std::vector<TestTask> testTasks;
bool testTaskFailure=false;
TestSemaphore testSnapshotMutex;
TestESP ESP;
TwoWire Wire;
static esp_reset_reason_t nextResetReason=ESP_RST_POWERON;
static unsigned resetReasonCalls=0;
esp_reset_reason_t esp_reset_reason(){++resetReasonCalls;return nextResetReason;}
uint32_t esp_random(){return 0x12345678;}
static void require(bool condition,const char *message){
 if(!condition)throw std::runtime_error(message);
}
struct Allocation {size_t size;uint32_t caps;void *result;};
static std::vector<Allocation> allocations;
static std::deque<bool> allocationFailures;
void *heap_caps_calloc(size_t count,size_t size,uint32_t caps){
 bool fail=false;
 if(!allocationFailures.empty()){fail=allocationFailures.front();allocationFailures.pop_front();}
 void *result=fail?nullptr:std::calloc(count,size);
 if(!fail && !result)throw std::runtime_error("host allocator failed unexpectedly");
 allocations.push_back({count*size,caps,result});return result;
}
static size_t liveAllocations(){
 return std::count_if(allocations.begin(),allocations.end(),
                      [](const Allocation &item){return item.result!=nullptr;});
}
static void releaseAllocations(){
 // Also release an old-source leaked object after the assertion reports it.
 for(const auto &item:allocations)std::free(item.result);
 allocations.clear();allocationFailures.clear();
}
struct DriverCalls {unsigned address=0,resolution=0,hz=0,starts=0,init=0;};
static std::map<VL53L5CX_Configuration *,DriverCalls> driverCalls;
static bool driverInitFails=false;
bool SparkFun_VL53L5CX_IO::begin(byte address,TwoWire &wire){
 _address=address;_i2cPort=&wire;return true;
}
bool SparkFun_VL53L5CX_IO::isConnected(){return true;}
void SparkFun_VL53L5CX_IO::setAddress(uint8_t value){_address=value;}
void SparkFun_VL53L5CX::setWireMaxPacketSize(uint8_t size){VL53L5CX_i2c->setMaxPacketSize(size);}
bool SparkFun_VL53L5CX::setAddress(uint8_t value){
 _i2cAddress=value;VL53L5CX_i2c->setAddress(value);driverCalls[Dev].address=value;return true;
}
bool SparkFun_VL53L5CX::setResolution(uint8_t value){driverCalls[Dev].resolution=value;return true;}
bool SparkFun_VL53L5CX::setRangingFrequency(uint8_t value){driverCalls[Dev].hz=value;return true;}
bool SparkFun_VL53L5CX::startRanging(){++driverCalls[Dev].starts;return true;}
bool SparkFun_VL53L5CX::isConnected(){return true;}
bool SparkFun_VL53L5CX::isDataReady(){return false;}
bool SparkFun_VL53L5CX::getRangingData(VL53L5CX_ResultsData *){return false;}
uint8_t vl53l5cx_is_alive(VL53L5CX_Configuration *dev,uint8_t *alive){
 require(dev && dev->platform.VL53L5CX_i2c,"adapter did not bind real IO/config");
 *alive=1;return 0;
}
uint8_t vl53l5cx_init(VL53L5CX_Configuration *dev){
 ++driverCalls[dev].init;return driverInitFails?1:0;
}

#ifndef REAR_ADAPTER_SOURCE
#error Runner must select the exact candidate or baseline radio_telemetry.cpp.
#endif
#include REAR_ADAPTER_SOURCE

static unsigned bnoBegins=0,rangingBegins=0,sensorBegins=0;
namespace bno_telemetry {
void begin(WebServer &,bool (*)()){++bnoBegins;}
void pause(bool){}
void writeJson(JsonObject out,uint64_t){out["present"]=false;out["state"]="absent";}
}
namespace rear_ranging {
void begin(WebServer &,bool (*)()){++rangingBegins;}
void pause(bool){}
void writeJson(JsonObject out,uint64_t){
 out["supported"]=false;out["enabled"]=false;out["status"]="disabled_in_firmware";
}
}
namespace sensor_push {
void begin(WebServer &,bool (*)()){++sensorBegins;}
void pause(bool){}
}
namespace rt=radio_telemetry;
static void resetFixture(){
 releaseAllocations();driverCalls.clear();driverInitFails=false;
 rt::devices[0]=rt::Device{};rt::devices[1]=rt::Device{};
 rt::published[0]=rt::Sample{};rt::published[1]=rt::Sample{};
 rt::snapshots=&testSnapshotMutex;testSnapshotMutex.held=false;
 rt::started=false;rt::paused=false;rt::lidarWorker=false;rt::uartWorker=false;
 rt::lastPeerRx=0;rt::uartErrors=0;rt::peerMux.depth=0;
 rt::receiver[0]=0;rt::boot[0]=0;
 rt::sensorWire.acknowledged={0x29};
 testNowUs=1000000;std::fill(std::begin(testPins),std::end(testPins),LOW);
 testPinWrites.clear();testTasks.clear();testTaskFailure=false;
 nextResetReason=ESP_RST_POWERON;resetReasonCalls=0;
 bnoBegins=rangingBegins=sensorBegins=0;
}
static void expectSuccessfulConfiguration(int i){
 auto &d=rt::devices[i];const auto &calls=driverCalls.at(d.config);
 require(d.ready && d.sample.ready && d.sample.error==nullptr,"successful head not ready");
 require(d.driver.Dev==d.config && d.driver.VL53L5CX_i2c==&d.io,
         "driver uses another head's allocations");
 require(d.config->platform.VL53L5CX_i2c==&d.io,"configuration IO pointer not restored after memset");
 require(calls.address==rt::ADDRESS[i] && calls.resolution==64 && calls.hz==5 && calls.starts>0,
         "address, full grid or 5Hz configuration changed");
 require(d.io.getMaxPacketSize()==128,"I2C packet limit changed");
}
static void healthyPeer(){
 require(rt::initialize(1),"healthy peer fixture failed");
 auto &s=rt::devices[1].sample;
 s.valid=true;s.observed=testNowUs;s.sequence=73;
 for(int z=0;z<64;++z){s.distance[z]=100+z;s.status[z]=5;}
 rt::publish(1);
}
struct PeerWitness {
 VL53L5CX_Configuration *config=rt::devices[1].config;
 VL53L5CX_ResultsData *results=rt::devices[1].results;
 rt::Sample sample=rt::devices[1].sample;
 void check() const {
  const auto &d=rt::devices[1];
  require(d.ready && d.config==config && d.results==results,"failed head changed peer allocation/readiness");
  require(d.sample.sequence==sample.sequence && d.sample.observed==sample.observed &&
          d.sample.errors==sample.errors && d.sample.retries==sample.retries &&
          d.sample.valid==sample.valid && d.sample.error==sample.error &&
          !std::memcmp(d.sample.distance,sample.distance,sizeof sample.distance) &&
          !std::memcmp(d.sample.status,sample.status,sizeof sample.status),
          "failed head changed peer sample");
  require(testPins[rt::LPN[1]]==HIGH,"healthy peer communications not restored");
  require(rt::published[1].sequence==sample.sequence,"healthy peer publication lost");
 }
};
static void memoryFailure(int i){
 const auto &d=rt::devices[i];
 require(!d.ready && !d.sample.ready && !d.sample.valid &&
         d.sample.error && std::string(d.sample.error)=="sensor_memory_unavailable",
         "partial allocation did not retain explicit unready memory error");
 require(d.retryAt==millis()+2000 && testPins[rt::LPN[i]]==LOW,
         "failed head retry deadline/communications behavior changed");
 require(rt::published[i].errors==d.sample.errors,"failure was not published");
}
static void configRetained(){
 healthyPeer();PeerWitness peer;
 allocationFailures={false,true};
 require(!rt::initialize(0),"results OOM unexpectedly initialized");
 auto *config=rt::devices[0].config;
 require(config && !rt::devices[0].results,"wrong first partial-allocation state");
 memoryFailure(0);peer.check();
 for(unsigned retry=0;retry<2;++retry){
  const size_t calls=allocations.size();allocationFailures={true};
  require(!rt::initialize(0),"repeated results OOM unexpectedly initialized");
  require(rt::devices[0].config==config,"successful configuration pointer replaced");
  require(allocations.size()==calls+1 && allocationFailures.empty(),
          "missing results allocation was not retried exactly once");
  require(liveAllocations()==3,"partial retry grew retained allocations");
  memoryFailure(0);peer.check();
 }
 allocationFailures={false};
 require(rt::initialize(0),"results allocation did not recover");
 require(rt::devices[0].config==config && rt::devices[0].results,"recovery lost retained config");
 require(liveAllocations()==4 && allocationFailures.empty(),"recovery allocation ownership incorrect");
 expectSuccessfulConfiguration(0);peer.check();
}
static void resultsRetained(){
 healthyPeer();PeerWitness peer;
 allocationFailures={true,false};
 require(!rt::initialize(0),"config OOM unexpectedly initialized");
 auto *results=rt::devices[0].results;
 require(!rt::devices[0].config && results,"wrong first partial-allocation state");
 memoryFailure(0);peer.check();
 for(unsigned retry=0;retry<2;++retry){
  const size_t calls=allocations.size();allocationFailures={true};
  require(!rt::initialize(0),"repeated config OOM unexpectedly initialized");
  require(rt::devices[0].results==results,"successful results pointer replaced or leaked");
  require(allocations.size()==calls+1 && allocationFailures.empty(),
          "missing config allocation was not retried exactly once");
  require(liveAllocations()==3,"partial retry grew retained allocations");
  memoryFailure(0);peer.check();
 }
 allocationFailures={false};
 require(rt::initialize(0),"configuration allocation did not recover");
 require(rt::devices[0].results==results && rt::devices[0].config,"recovery lost retained results");
 require(liveAllocations()==4 && allocationFailures.empty(),"recovery allocation ownership incorrect");
 expectSuccessfulConfiguration(0);peer.check();
}
static void bothNullAndDriverRetry(){
 allocationFailures={true,true};
 require(!rt::initialize(0),"both-null OOM unexpectedly initialized");
 require(!rt::devices[0].config && !rt::devices[0].results && liveAllocations()==0,
         "both-null attempt retained an allocation");
 memoryFailure(0);
 allocationFailures={false,false};
 require(rt::initialize(0),"both-null allocation did not later recover");
 expectSuccessfulConfiguration(0);
 auto *config=rt::devices[0].config;auto *results=rt::devices[0].results;
 const size_t calls=allocations.size();
 rt::fail(0,"read_failed");driverInitFails=true;
 require(!rt::initialize(0),"failed hardware initialization accepted");
 require(rt::devices[0].sample.error &&
         std::string(rt::devices[0].sample.error)=="initialization_failed","driver failure status changed");
 driverInitFails=false;
 require(rt::initialize(0),"retry with existing pair failed");
 require(rt::devices[0].config==config && rt::devices[0].results==results &&
         allocations.size()==calls && liveAllocations()==2,
         "hardware retry replaced or reallocated retained pair");
 require(rt::devices[0].sample.retries==4 && rt::devices[0].sample.errors==3,
         "actual initialization/error counters changed");
 expectSuccessfulConfiguration(0);
 for(const auto &allocation:allocations){
  require(allocation.caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),"allocation moved from PSRAM");
  require(allocation.size==sizeof(VL53L5CX_Configuration) ||
          allocation.size==sizeof(VL53L5CX_ResultsData),"allocation no longer matches real library type");
 }
}
static bool authorized=true;
static unsigned authorizations=0;
static bool authorize(){++authorizations;return authorized;}
static DynamicJsonDocument get(WebServer &server,const char *path){
 server.request(path,HTTP_GET);
 require(server.responseCode==200,"authorized status route failed");
 DynamicJsonDocument result(16384);
 require(!deserializeJson(result,server.responseBody),"status JSON parse failed");
 return result;
}
static void seedFullHeads(){
 for(int i=0;i<2;++i){
  require(rt::initialize(i),"head initialization failed");
  auto &s=rt::devices[i].sample;s.valid=true;s.observed=testNowUs;s.sequence=80+i;
  for(int z=0;z<64;++z){s.distance[z]=200+100*i+z;s.status[z]=5;}
  rt::publish(i);expectSuccessfulConfiguration(i);
 }
}
static void fullGrid(JsonObjectConst out){
 require(out["lidar"]["worker_ready"].as<bool>(),"LiDAR worker readiness lost");
 auto heads=out["lidar"]["sensors"].as<JsonArrayConst>();
 require(heads.size()==2,"both heads not retained");
 for(int i=0;i<2;++i){
  auto head=heads[i];
  require(head["id"]==(i==0?"front":"rear") && head["valid"].as<bool>() &&
          head["ready"].as<bool>() && head["resolution"].as<int>()==64,
          "head identity/readiness/grid changed");
  require(head["sequence"].as<unsigned>()==80U+i &&
          head["observed_monotonic_us"].as<uint64_t>()==testNowUs,
          "head sample identity changed");
  require(head["distance_mm"].size()==64 && head["target_status"].size()==64 &&
          head["distance_mm"][63].as<int>()==263+100*i &&
          head["target_status"][63].as<int>()==5,"full grid payload was changed or omitted");
 }
 require(out["bno"]["present"]==false && out["bno"]["state"]=="absent",
         "optional BNO snapshot was omitted");
}
static void resetCacheAndRouteScope(){
 WebServer server;authorized=true;authorizations=0;
 nextResetReason=ESP_RST_PANIC;rt::begin(server,authorize);
 require(resetReasonCalls==1,"reset reason was not captured exactly once during begin");
 require(bnoBegins==1 && rangingBegins==1 && sensorBegins==1 && testTasks.size()==2,
         "peripheral worker/startup hooks changed");
 seedFullHeads();
 nextResetReason=ESP_RST_BROWNOUT;rt::begin(server,authorize);
 require(resetReasonCalls==1 && testTasks.size()==2 && bnoBegins==1,
         "second begin reread reset reason or restarted workers");
 auto rear=get(server,"/api/rear");
 require(rear["reset_reason_code"].as<int>()==ESP_RST_PANIC && rear["reset_reason"]=="panic",
         "HTTP reported current stub instead of cached boot reason");
 require(resetReasonCalls==1,"HTTP status reread reset reason");
 require(rear["firmware"]==APP_VERSION && rear["running_partition"]=="app0" &&
         rear["receiver_id"]=="rear-1ca14b858428","status identity changed");
 fullGrid(rear.as<JsonObjectConst>());
 rear.remove("reset_reason_code");rear.remove("reset_reason");
 std::string expected;serializeJson(rear,expected);
 for(const char *path:{"/api/telemetry","/api/lidar"}){
  auto other=get(server,path);
  require(!other.containsKey("reset_reason_code") && !other.containsKey("reset_reason"),
          "reset reason leaked into another status route");
  std::string actual;serializeJson(other,actual);
  require(actual==expected,"rear diagnostic changed unrelated status content");
 }
 DynamicJsonDocument frame(16384);
 require(rt::writeFrameJson(frame.to<JsonObject>(),testNowUs,0),"optional peripheral rejected frame");
 require(!frame.containsKey("reset_reason_code") && !frame.containsKey("reset_reason"),
         "reset reason increased streaming metadata");
 require(frame["schema"]=="biome.capture.v1" && frame["frame_id"]=="1234567812345678:1000000",
         "capture schema or identity changed");
 fullGrid(frame.as<JsonObjectConst>());
 authorized=false;
 server.request("/api/rear",HTTP_GET);
 require(server.responseCode==0 && server.responseBody.empty(),"unauthorized rear status was exposed");
 require(authorizations==4 && resetReasonCalls==1,"route authorization/cache behavior changed");
}
static void resetLabels(){
 const std::pair<esp_reset_reason_t,const char *> reasons[]={
  {ESP_RST_UNKNOWN,"unknown"},{ESP_RST_POWERON,"power_on"},{ESP_RST_EXT,"external"},
  {ESP_RST_SW,"software"},{ESP_RST_PANIC,"panic"},{ESP_RST_INT_WDT,"interrupt_watchdog"},
  {ESP_RST_TASK_WDT,"task_watchdog"},{ESP_RST_WDT,"watchdog"},
  {ESP_RST_DEEPSLEEP,"deep_sleep"},{ESP_RST_BROWNOUT,"brownout"},{ESP_RST_SDIO,"sdio"},
  {static_cast<esp_reset_reason_t>(777),"unknown"}
 };
 for(const auto &reason:reasons){
  resetFixture();WebServer server;authorized=true;nextResetReason=reason.first;
  rt::begin(server,authorize);auto out=get(server,"/api/rear");
  require(out["reset_reason_code"].as<int>()==static_cast<int>(reason.first) &&
          out["reset_reason"]==reason.second,"reset enum/label mapping changed or unknown value hidden");
  require(resetReasonCalls==1,"label read touched reset source again");
 }
}
int main(int argc,char **argv){
 const std::pair<const char *,std::function<void()>> cases[]={
  {"config-retained",configRetained},{"results-retained",resultsRetained},
  {"both-null-and-driver-retry",bothNullAndDriverRetry},
  {"reset-cache-and-route-scope",resetCacheAndRouteScope},{"reset-labels",resetLabels}
 };
 unsigned ran=0;
 for(const auto &test:cases){
  if(argc>1 && std::string(argv[1])!=test.first)continue;
  ++ran;
  try{resetFixture();test.second();std::printf("PASS %s\n",test.first);}
  catch(const std::exception &error){
   std::printf("FAIL %s: %s\n",test.first,error.what());releaseAllocations();return 1;
  }
  releaseAllocations();std::fflush(stdout);
 }
 if(!ran){std::fprintf(stderr,"Unknown test case\n");return 2;}
 std::printf("PASS %u actual radio adapter groups\n",ran);return 0;
}
