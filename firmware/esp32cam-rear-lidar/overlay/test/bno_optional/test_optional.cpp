#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include "Arduino.h"
#include "Update.h"
#include "SparkFun_BNO08x_Arduino_Library.h"
#include "freertos/task.h"
#include "../../src/bno_telemetry.cpp"

uint64_t testNowUs=1000000;
int testPins[49];
std::vector<std::pair<int,int>> testPinModes,testPinWrites;
int testSpiBegins=0,testSpiEnds=0;
std::vector<std::array<int,4>> testSpiRoutes;
TestUpdate Update;
TestImu testImu;
sh2_SensorCallback_t *testCallback=nullptr;
void *testCallbackCookie=nullptr;
std::vector<TestTask> testTasks;
bool testTaskFailure=false,testQueueFailure=false;
WebServer server;
bool testAuthorized=true;

extern "C" int sh2_setSensorCallback(sh2_SensorCallback_t *callback,void *cookie){
    testCallback=callback;testCallbackCookie=cookie;return SH2_OK;
}
extern "C" int sh2_decodeSensorEvent(sh2_SensorValue_t *value,const sh2_SensorEvent_t *){
    ++testImu.decodes;assert(!testImu.events.empty());
    *value=testImu.events.front().value;return testImu.events.front().decodeStatus;
}
extern "C" void sh2_close(){
    ++testImu.closes;testCallback=nullptr;testCallbackCookie=nullptr;
}
bool authorize(){
    if(!testAuthorized)server.send(403,"application/json","{\"error\":\"unauthorized\"}");
    return testAuthorized;
}

using namespace bno_telemetry;
void reset(bool start=true){
    if(commands)delete commands;
    commands=nullptr;paused=false;workerReady=false;nextCommand=0;
    started=pinsReady=spiStarted=sessionStarted=false;
    phase=Phase::Backoff;nextAction=lastEvent=0;
    current=Snapshot();published=Snapshot();snapshotLock={};
    testNowUs=1000000;std::fill(testPins,testPins+49,HIGH);
    testPinModes.clear();testPinWrites.clear();
    testSpiBegins=testSpiEnds=0;testSpiRoutes.clear();
    testImu=TestImu();testCallback=nullptr;testCallbackCookie=nullptr;
    testTasks.clear();testTaskFailure=testQueueFailure=false;
    Update.running=false;testAuthorized=true;server=WebServer();
    if(start)bno_telemetry::begin(server,authorize);
}
DynamicJsonDocument status(uint64_t at=0){
    DynamicJsonDocument result(4096);
    writeJson(result.to<JsonObject>(),at?at:testNowUs);return result;
}
void noValues(JsonObjectConst output){
    assert(!output["valid"].as<bool>());
    for(const char *key:{"quaternion","accel_ms2","gyro_rads","mag_uT"}){
        assert(!output[key]["valid"].as<bool>());
        assert(output[key]["x"].isNull()&&output[key]["i"].isNull());
    }
}
void startDetection(){
    workerStep();
    assert(phase==Phase::ResetLow);
    testNowUs+=RESET_US;
    workerStep();
    assert(phase==Phase::WaitReady);
}
void connect(){
    startDetection();testPins[INT_PIN]=LOW;workerStep();testPins[INT_PIN]=HIGH;
    assert(phase==Phase::Running);
    auto d=status();assert(d["present"].as<bool>()&&d["ready"].as<bool>());
    assert(testImu.reports.size()==4);
    for(const auto &report:testImu.reports)assert(report.second==50);
    assert(testSpiRoutes.size()==1);
    assert((testSpiRoutes.front()==std::array<int,4>{{14,47,2,40}}));
}
void event(sh2_SensorId_t id,float x=1,float y=2,float z=3,float w=1){
    TestEvent item;item.value.sensorId=id;item.value.status=3;
    switch(id){
    case SH2_ROTATION_VECTOR:
        item.value.un.rotationVector.i=x;item.value.un.rotationVector.j=y;
        item.value.un.rotationVector.k=z;item.value.un.rotationVector.real=w;
        item.value.un.rotationVector.accuracy=.125f;break;
    case SH2_ACCELEROMETER:item.value.un.accelerometer={x,y,z};break;
    case SH2_GYROSCOPE_CALIBRATED:item.value.un.gyroscope={x,y,z};break;
    case SH2_MAGNETIC_FIELD_CALIBRATED:item.value.un.magneticField={x,y,z};break;
    default:break;
    }
    testImu.events.push_back(item);
}
void serviceEvents(){testPins[INT_PIN]=LOW;workerStep();testPins[INT_PIN]=HIGH;}
bool enqueue(const char *op,const char *id="test-command",bool z=false){
    DynamicJsonDocument input(512),reply(512);
    input["op"]=op;input["command_id"]=id;input["z_axis"]=z;
    return enqueueCommand(input.as<JsonObjectConst>(),reply.to<JsonObject>());
}
void pass(const char *name){std::cout<<"PASS "<<name<<std::endl;}

int main(){
    reset();
    bno_telemetry::begin(server,authorize);
    assert(testTasks.size()==1&&testTasks[0].name=="rear-bno");
    assert(testTasks[0].priority==0);
    for(int attempt=0;attempt<4;++attempt){
        startDetection();
        testNowUs+=DETECT_US;workerStep();
        auto d=status();assert(d["error"]=="not_detected");
        noValues(d.as<JsonObjectConst>());
        assert(!enqueue("calibrate_all"));
        testNowUs+=RETRY_US;
    }
    assert(testImu.begins==0&&testSpiBegins==4&&testImu.services==0);
    assert(testImu.closes==0&&testSpiEnds==4);
    pass("absent module retries detection without selecting the sensor or opening SH2 or valid values");

    reset();testImu.beginResult=false;
    for(int attempt=0;attempt<3;++attempt){
        startDetection();testPins[INT_PIN]=LOW;workerStep();testPins[INT_PIN]=HIGH;
        assert(testImu.begins==attempt+1&&testImu.closes==attempt+1&&testSpiEnds==attempt+1);
        auto d=status();assert(d["error"]=="initialization_failed");
        workerStep();assert(testImu.begins==attempt+1);
        testNowUs+=RETRY_US;
    }
    pass("failed initialization closes its session and waits before retry");

    reset();testImu.reportResults={true,false};startDetection();
    testPins[INT_PIN]=LOW;workerStep();
    auto failed=status();assert(failed["error"]=="report_configuration_failed");
    assert(testImu.reports.size()==2&&testImu.closes==1&&testSpiEnds==1);
    noValues(failed.as<JsonObjectConst>());
    pass("failed report configuration never publishes ready or stale data");

    reset();connect();testNowUs+=1000;
    event(SH2_ROTATION_VECTOR,0,0,0,1);
    event(SH2_ACCELEROMETER,1,2,3);
    event(SH2_GYROSCOPE_CALIBRATED,4,5,6);
    event(SH2_MAGNETIC_FIELD_CALIBRATED,7,8,9);
    serviceEvents();
    assert(testImu.decodes==4);
    DynamicJsonDocument probe(1024);
    writeProbe(probe.to<JsonObject>(),testNowUs);
    assert(!probe.overflowed() && probe["product"]==10003660 && probe["sequence"]==4);
    assert(probe["quaternion"]["valid"] && probe["quaternion"]["real"]==1);
    assert(probe["spi"].size()==10);
    const uint64_t quaternionAt=testNowUs;
    auto all=status();assert(all["sequence"].as<unsigned>()==4);
    assert(all["quaternion"]["real"].as<float>()==1);
    assert(all["accel_ms2"]["x"].as<float>()==1);
    assert(all["gyro_rads"]["x"].as<float>()==4);
    assert(all["mag_uT"]["x"].as<float>()==7);
    testNowUs+=FRESH_US+1;event(SH2_ACCELEROMETER,10,11,12);serviceEvents();
    auto aged=status();assert(!aged["valid"].as<bool>());
    assert(!aged["quaternion"]["valid"].as<bool>()&&aged["quaternion"]["i"].isNull());
    assert(aged["accel_ms2"]["valid"].as<bool>());
    auto past=status(quaternionAt-1);noValues(past.as<JsonObjectConst>());
    assert(past["accel_ms2"]["age_ms"].isNull());
    pass("all packet reports are decoded and each rejects stale or post-capture data");

    event(SH2_ACCELEROMETER,std::numeric_limits<float>::quiet_NaN(),1,2);
    serviceEvents();auto bad=status();
    assert(!bad["accel_ms2"]["valid"].as<bool>()&&bad["errors"].as<unsigned>()==1);
    pass("nonfinite report invalidates that report without inventing a measurement");

    reset();connect();event(SH2_ROTATION_VECTOR,0,0,0,1);serviceEvents();
    testImu.reset=true;workerStep();auto restarted=status();
    assert(restarted["resets"].as<unsigned>()==1&&testImu.reports.size()==8);
    noValues(restarted.as<JsonObjectConst>());
    testNowUs+=LOST_US+1;workerStep();auto detached=status();
    assert(detached["error"]=="data_timeout"&&!detached["ready"].as<bool>());
    assert(testImu.closes==1&&testSpiEnds==1);noValues(detached.as<JsonObjectConst>());
    pass("reset and report timeout invalidate the old sensor session");

    reset();connect();testImu.reset=true;testImu.reportResults={false};workerStep();
    assert(status()["error"]=="report_configuration_failed");
    assert(testImu.closes==1);pass("failed reset reconfiguration closes the session");

    reset();connect();
    for(int i=0;i<4;++i){std::string id="q"+std::to_string(i);assert(enqueue("tare_now",id.c_str(),i%2));}
    assert(!enqueue("tare_now","overflow"));
    assert(testImu.commands.empty());
    for(int i=0;i<4;++i){
        workerStep();auto result=status();
        assert(result["last_command"]["state"]=="completed");
        assert(result["last_command"]["command_id"].as<std::string>()=="q"+std::to_string(i));
        assert(testImu.commands.back().second==i%2);
    }
    assert(testImu.commands.size()==4&&commands->items.empty());
    pass("bounded command queue executes only in worker order with terminal identities");

    reset();connect();assert(enqueue("save_calibration","save-one"));
    testImu.commandResult=false;workerStep();auto commandFailed=status();
    assert(commandFailed["last_command"]["state"]=="failed");
    assert(commandFailed["last_command"]["command_id"]=="save-one");
    assert(commandFailed["last_command"]["error"]=="sensor_command_failed");
    assert(!commandFailed["ready"].as<bool>()&&testImu.closes==1);
    assert(!commandFailed["esp_calibration_backup_supported"].as<bool>());
    assert(commandFailed["calibration_storage"]=="sensor_flash_on_explicit_save");
    pass("failed calibration reports failure and does not claim an ESP backup");

    reset();connect();assert(enqueue("calibrate_all","lost-before-run"));
    testNowUs+=LOST_US+1;workerStep();workerStep();
    auto lost=status();assert(testImu.commands.empty());
    assert(lost["last_command"]["command_id"]=="lost-before-run");
    assert(lost["last_command"]["error"]=="not_ready");
    pass("accepted work loses readiness explicitly without calling a missing sensor");

    reset();connect();event(SH2_ROTATION_VECTOR,0,0,0,1);serviceEvents();
    assert(enqueue("calibrate_gyro","pause-queued"));
    pause(true);
    const int services=testImu.services,begins=testImu.begins;
    const size_t writes=testPinWrites.size();
    workerStep();workerStep();
    auto halted=status();assert(halted["state"]=="paused");
    noValues(halted.as<JsonObjectConst>());
    assert(testImu.commands.empty()&&testImu.services==services&&testImu.begins==begins);
    assert(testPinWrites.size()==writes&&!enqueue("calibrate_all"));
    pause(false);workerStep();assert(testImu.commands.size()==1);
    Update.running=true;workerStep();assert(status()["paused"].as<bool>());
    assert(!enqueue("clear_tare"));
    pass("OTA pause gates reports, commands and new sensor work while retaining queued custody");

    reset();pause(true);workerStep();
    assert(testPinModes.empty()&&testPinWrites.empty()&&testSpiBegins==0&&testImu.begins==0);
    pass("OTA pause before first worker step performs no GPIO or bus setup");

    reset();testImu.duringBegin=[](){pause(true);};
    startDetection();testPins[INT_PIN]=LOW;workerStep();
    assert(testImu.reports.empty()&&testImu.closes==1);
    assert(status()["paused"].as<bool>());
    pass("pause arriving during initialization prevents report configuration");

    reset(false);testQueueFailure=true;bno_telemetry::begin(server,authorize);
    assert(!workerReady.load()&&testTasks.empty());
    assert(status()["error"]=="worker_unavailable");
    reset(false);testTaskFailure=true;bno_telemetry::begin(server,authorize);
    assert(!workerReady.load()&&testTasks.empty());
    assert(status()["error"]=="worker_unavailable");
    pass("worker or queue allocation failure remains honest and nonblocking");

    reset();testAuthorized=false;server.request("/api/bno",HTTP_GET);
    assert(server.responseCode==403);
    server.request("/api/bno",HTTP_POST,"{\"op\":\"calibrate_all\"}");
    assert(server.responseCode==403&&testImu.commands.empty());
    testAuthorized=true;server.request("/api/bno",HTTP_POST,"not json");
    assert(server.responseCode==400);
    connect();server.request("/api/bno",HTTP_POST,"{\"op\":\"clear_tare\",\"command_id\":\"http-one\"}");
    assert(server.responseCode==202&&testImu.commands.empty());
    server.request("/api/bno",HTTP_GET);
    assert(server.responseCode==200&&testImu.services==0);
    workerStep();assert(testImu.commands.size()==1);
    pass("HTTP authorization, input rejection and queued acknowledgement touch no sensor inline");

    reset();connect();
    DynamicJsonDocument ownedReply(512);
    {
        DynamicJsonDocument temporaryInput(512);
        temporaryInput["op"]=std::string("calibrate_mag");
        temporaryInput["command_id"]=std::string("owned-request-id");
        assert(enqueueCommand(temporaryInput.as<JsonObjectConst>(),ownedReply.to<JsonObject>()));
    }
    std::vector<std::string> overwrite(64,std::string(128,'X'));
    std::string serializedReply;serializeJson(ownedReply,serializedReply);
    assert(ownedReply["op"]=="calibrate_mag");
    workerStep();
    auto ownedStatus=status();
    for(int i=0;i<32;++i){auto other=status();assert(other["ready"].as<bool>());}
    std::string serializedStatus;serializeJson(ownedStatus,serializedStatus);
    assert(ownedStatus["last_command"]["command_id"]=="owned-request-id");
    assert(ownedStatus["last_command"]["op"]=="calibrate_mag");
    pass("queued and terminal JSON own strings after caller and snapshot lifetimes end");

    reset(false);
    std::cout<<"PASS BNO production adapter boundary suite"<<std::endl;
}
