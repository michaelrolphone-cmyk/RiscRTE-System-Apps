#include "bno_telemetry.h"
#include <Arduino.h>
#include <SPI.h>
#include <SparkFun_BNO08x_Arduino_Library.h>
#include <Update.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <initializer_list>

namespace bno_telemetry {
namespace {
// Rear-only board profile: camera, LiDAR, UART, LEDs and USB retain their pins.
// GPIO3 selects JTAG at reset; GPIO46 must remain low for ROM download with GPIO0 low.
// See README before soldering. GPIO45 (VDD_SPI strap) is deliberately not used.
constexpr int SCK_PIN=14, MOSI_PIN=2, MISO_PIN=47, CS_PIN=40, INT_PIN=3, RST_PIN=46;
constexpr uint32_t SPI_HZ=1000000, REPORT_MS=50;
constexpr uint64_t FRESH_US=500000, LOST_US=2000000, RETRY_US=5000000;
constexpr uint64_t RESET_US=10000, DETECT_US=500000;
constexpr size_t COMMAND_DEPTH=4;
SPIClass sensorSpi(HSPI);
BNO08x imu;
QueueHandle_t commands=nullptr;
std::atomic<bool> paused{false}, workerReady{false};
std::atomic<uint32_t> nextCommand{0};
bool started=false, pinsReady=false, spiStarted=false, sessionStarted=false;
enum class Phase { Backoff, ResetLow, WaitReady, Running };
Phase phase=Phase::Backoff;
uint64_t nextAction=0, lastEvent=0;
struct Report {
    uint64_t observed=0;
    uint32_t sequence=0;
    uint8_t accuracy=0;
    float x=0,y=0,z=0,w=0,radians=0;
};
struct Command {
    char op[24]={};
    char id[64]={};
    bool zAxis=false;
};
struct CommandResult {
    char op[24]={};
    char id[64]={};
    const char *state="none";
    const char *error=nullptr;
    uint64_t completed=0;
};
struct Snapshot {
    bool present=false, ready=false;
    const char *state="starting";
    const char *error=nullptr;
    uint32_t attempts=0, errors=0, resets=0, sequence=0;
    uint32_t product=0;
    uint8_t swMajor=0,swMinor=0;
    uint16_t swPatch=0;
    Report quaternion, accel, gyro, mag;
    CommandResult command;
} current, published;
portMUX_TYPE snapshotLock=portMUX_INITIALIZER_UNLOCKED;
uint64_t nowUs(){return static_cast<uint64_t>(esp_timer_get_time());}
bool fresh(uint64_t at,const Report &r){return r.observed && at>=r.observed && at-r.observed<=FRESH_US;}
bool isPaused(){return paused.load() || Update.isRunning();}
void publish(){
    portENTER_CRITICAL(&snapshotLock);
    published=current;
    portEXIT_CRITICAL(&snapshotLock);
}
Snapshot snapshot(){
    Snapshot result;
    portENTER_CRITICAL(&snapshotLock);
    result=published;
    portEXIT_CRITICAL(&snapshotLock);
    return result;
}
void invalidate(){
    current.quaternion.observed=0;current.accel.observed=0;
    current.gyro.observed=0;current.mag.observed=0;
}
void closeSession(){
    // The rear build patches upstream sh2_close to tolerate a failed/null open.
    if(sessionStarted){sh2_close();sessionStarted=false;}
    if(spiStarted){sensorSpi.end();spiStarted=false;}
    digitalWrite(CS_PIN,HIGH);
}
void fail(const char *reason,bool countError=true){
    closeSession();
    current.present=false;current.ready=false;
    current.state="unavailable";current.error=reason;
    if(countError)++current.errors;
    invalidate();
    phase=Phase::Backoff;nextAction=nowUs()+RETRY_US;
    publish();
}
// Decode every SH2 report, including multiple reports in one SHTP packet.
// The wrapper's getSensorEvent() exposes only the last report; do not use it here.
void onSensor(void *,sh2_SensorEvent_t *event){
    if(isPaused())return;
    sh2_SensorValue_t v{};
    if(sh2_decodeSensorEvent(&v,event)!=SH2_OK){++current.errors;return;}
    Report *r=nullptr;
    float x=0,y=0,z=0,w=0,radians=0;
    switch(v.sensorId){
    case SH2_ROTATION_VECTOR:
        r=&current.quaternion;x=v.un.rotationVector.i;y=v.un.rotationVector.j;
        z=v.un.rotationVector.k;w=v.un.rotationVector.real;radians=v.un.rotationVector.accuracy;break;
    case SH2_ACCELEROMETER:
        r=&current.accel;x=v.un.accelerometer.x;y=v.un.accelerometer.y;z=v.un.accelerometer.z;break;
    case SH2_GYROSCOPE_CALIBRATED:
        r=&current.gyro;x=v.un.gyroscope.x;y=v.un.gyroscope.y;z=v.un.gyroscope.z;break;
    case SH2_MAGNETIC_FIELD_CALIBRATED:
        r=&current.mag;x=v.un.magneticField.x;y=v.un.magneticField.y;z=v.un.magneticField.z;break;
    default:return;
    }
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
       !std::isfinite(w)||!std::isfinite(radians)){
        r->observed=0;++current.errors;return;
    }
    r->x=x;r->y=y;r->z=z;r->w=w;r->radians=radians;r->accuracy=v.status;
    r->observed=nowUs();++r->sequence;++current.sequence;lastEvent=r->observed;
}
bool reports(){
    // Each call is confined to the worker. The pinned SH2 patch bounds absent replies.
    return !isPaused() && imu.enableRotationVector(REPORT_MS) &&
           !isPaused() && imu.enableAccelerometer(REPORT_MS) &&
           !isPaused() && imu.enableGyro(REPORT_MS) &&
           !isPaused() && imu.enableMagnetometer(REPORT_MS);
}
void commandResult(const Command &cmd,bool ok,const char *error){
    memcpy(current.command.op,cmd.op,sizeof(cmd.op));
    memcpy(current.command.id,cmd.id,sizeof(cmd.id));
    current.command.state=ok?"completed":"failed";
    current.command.error=error;current.command.completed=nowUs();
    publish();
}
void runCommand(const Command &cmd){
    if(!current.ready || isPaused()){
        commandResult(cmd,false,isPaused()?"paused":"not_ready");return;
    }
    bool ok=false;
    if(!strcmp(cmd.op,"calibrate_all"))ok=imu.setCalibrationConfig(7);
    else if(!strcmp(cmd.op,"calibrate_accel"))ok=imu.setCalibrationConfig(1);
    else if(!strcmp(cmd.op,"calibrate_gyro"))ok=imu.setCalibrationConfig(2);
    else if(!strcmp(cmd.op,"calibrate_mag"))ok=imu.setCalibrationConfig(4);
    else if(!strcmp(cmd.op,"end_calibration"))ok=imu.setCalibrationConfig(0);
    else if(!strcmp(cmd.op,"save_calibration"))ok=imu.saveCalibration();
    else if(!strcmp(cmd.op,"tare_now"))ok=imu.tareNow(cmd.zAxis);
    else if(!strcmp(cmd.op,"save_tare"))ok=imu.saveTare();
    else if(!strcmp(cmd.op,"clear_tare"))ok=imu.clearTare();
    commandResult(cmd,ok,ok?nullptr:"sensor_command_failed");
    if(!ok)fail("sensor_command_failed");
}
// One worker iteration is separately exercised by the host boundary tests.
void workerStep(){
    if(!pinsReady){
        pinMode(INT_PIN,INPUT_PULLUP);
        pinMode(CS_PIN,OUTPUT);digitalWrite(CS_PIN,HIGH);
        pinMode(RST_PIN,OUTPUT);digitalWrite(RST_PIN,HIGH);
        pinsReady=true;
    }
    if(isPaused()){
        // Never start another sensor operation while OTA has requested pause.
        invalidate();publish();return;
    }
    const uint64_t now=nowUs();
    if(phase==Phase::Backoff){
        // Drain accepted work that lost its device before execution.
        Command cmd;
        if(commands && xQueueReceive(commands,&cmd,0)==pdTRUE)commandResult(cmd,false,"not_ready");
        if(now<nextAction)return;
        ++current.attempts;current.state="detecting";current.error=nullptr;
        digitalWrite(RST_PIN,LOW);phase=Phase::ResetLow;nextAction=now+RESET_US;
        publish();return;
    }
    if(phase==Phase::ResetLow){
        if(now<nextAction)return;
        digitalWrite(RST_PIN,HIGH);phase=Phase::WaitReady;nextAction=now+DETECT_US;return;
    }
    if(phase==Phase::WaitReady){
        // With no module, the pulled-up INT stays high: no SPI or SH2 call is made.
        if(digitalRead(INT_PIN)!=LOW){
            if(now>=nextAction)fail("not_detected",false);
            return;
        }
        current.state="initializing";publish();
        sensorSpi.begin(SCK_PIN,MISO_PIN,MOSI_PIN,CS_PIN);spiStarted=true;
        sessionStarted=true;
        if(!imu.beginSPI(CS_PIN,INT_PIN,RST_PIN,SPI_HZ,sensorSpi)){
            fail("initialization_failed");return;
        }
        // Install before report configuration can dispatch a sensor event.
        sh2_setSensorCallback(onSensor,nullptr);
        imu.wasReset();
        if(isPaused()){fail("paused",false);return;}
        current.present=true;
        if(imu.prodIds.numEntries){
            current.product=imu.prodIds.entry[0].swPartNumber;
            current.swMajor=imu.prodIds.entry[0].swVersionMajor;
            current.swMinor=imu.prodIds.entry[0].swVersionMinor;
            current.swPatch=imu.prodIds.entry[0].swVersionPatch;
        }
        if(!reports()){fail(isPaused()?"paused":"report_configuration_failed");return;}
        current.ready=true;current.state="ready";current.error=nullptr;
        lastEvent=nowUs();phase=Phase::Running;publish();return;
    }
    // Service first so a reset notification invalidates this same published snapshot.
    bool reset=imu.wasReset();
    if(!reset && digitalRead(INT_PIN)==LOW){imu.serviceBus();reset=imu.wasReset();}
    // A reset requires fresh reports; old values must not bridge sensor sessions.
    if(reset){
        ++current.resets;invalidate();
        current.ready=false;current.state="reconfiguring";publish();
        if(!reports()){fail("report_configuration_failed");return;}
        current.ready=true;current.state="ready";lastEvent=nowUs();
    }
    if(nowUs()-lastEvent>LOST_US){fail("data_timeout");return;}
    // onSensor may have seen multiple different report types.
    publish();
    if(isPaused())return;
    Command cmd;
    if(commands && xQueueReceive(commands,&cmd,0)==pdTRUE)runCommand(cmd);
}
void worker(void *){
    for(;;){
        workerStep();
        vTaskDelay(pdMS_TO_TICKS(phase==Phase::Running?2:20));
    }
}
void writeReport(JsonObject out,const Report &r,uint64_t at,bool available,bool quaternion=false){
    const bool valid=available && fresh(at,r);
    out["valid"]=valid;out["sequence"]=r.sequence;
    out["observed_monotonic_us"]=r.observed?r.observed:0;
    if(r.observed && at>=r.observed)out["age_ms"]=(at-r.observed)/1000;
    else out["age_ms"]=nullptr;
    if(!valid)return;
    out["accuracy"]=r.accuracy;
    if(quaternion){
        out["i"]=r.x;out["j"]=r.y;out["k"]=r.z;out["real"]=r.w;
        out["accuracy_rad"]=r.radians;
    }else{out["x"]=r.x;out["y"]=r.y;out["z"]=r.z;}
}
bool knownCommand(const char *op){
    for(const char *name:{"calibrate_all","calibrate_accel","calibrate_gyro","calibrate_mag",
                         "end_calibration","save_calibration","tare_now","save_tare","clear_tare"})
        if(!strcmp(op,name))return true;
    return false;
}
}
void writeJson(JsonObject out,uint64_t at){
    const Snapshot s=snapshot();
    const bool halted=isPaused();
    const bool available=workerReady.load() && s.ready && !halted;
    out["schema"]="rover.bno085.v1";out["module"]="GY-BNO08x";out["optional"]=true;
    out["interface"]="spi";out["spi_hz"]=SPI_HZ;out["report_interval_ms"]=REPORT_MS;
    out["worker_ready"]=workerReady.load();out["present"]=s.present;
    out["ready"]=available;out["paused"]=halted;
    out["state"]=halted?"paused":s.state;out["error"]=s.error;
    out["valid"]=available && fresh(at,s.quaternion);
    out["initialization_attempts"]=s.attempts;out["errors"]=s.errors;
    out["resets"]=s.resets;out["sequence"]=s.sequence;out["retry_interval_ms"]=RETRY_US/1000;
    out["timestamp_semantics"]="host_receipt_esp_timer_per_report";
    out["coordinate_frame"]="sensor_body_unmounted";
    out["esp_calibration_backup_supported"]=false;
    out["calibration_storage"]="sensor_flash_on_explicit_save";
    JsonObject pins=out.createNestedObject("pins");
    pins["sck"]=SCK_PIN;pins["mosi"]=MOSI_PIN;pins["miso"]=MISO_PIN;
    pins["cs"]=CS_PIN;pins["int"]=INT_PIN;pins["rst"]=RST_PIN;
    if(s.product){
        JsonObject product=out.createNestedObject("product");
        product["part_number"]=s.product;product["sw_major"]=s.swMajor;
        product["sw_minor"]=s.swMinor;product["sw_patch"]=s.swPatch;
    }
    writeReport(out.createNestedObject("quaternion"),s.quaternion,at,available,true);
    writeReport(out.createNestedObject("accel_ms2"),s.accel,at,available);
    writeReport(out.createNestedObject("gyro_rads"),s.gyro,at,available);
    writeReport(out.createNestedObject("mag_uT"),s.mag,at,available);
    JsonObject result=out.createNestedObject("last_command");
    result["command_id"]=s.command.id;result["op"]=s.command.op;
    result["state"]=s.command.state;result["error"]=s.command.error;
    result["completed_monotonic_us"]=s.command.completed;
}
bool enqueueCommand(JsonObjectConst input,JsonObject reply){
    const char *op=input["op"]|"";
    reply["schema"]="rover.bno085.cmd.v1";reply["op"]=op;reply["accepted"]=false;
    const char *error=nullptr;
    if(!knownCommand(op))error="unknown_op";
    else if(isPaused())error="paused";
    else if(!workerReady.load() || !snapshot().ready)error="not_ready";
    const char *id=input["command_id"]|"";
    if(strlen(id)>=sizeof(Command{}.id))error="command_id_too_long";
    if(error){reply["error"]=error;return false;}
    Command cmd;
    snprintf(cmd.op,sizeof(cmd.op),"%s",op);
    if(*id)snprintf(cmd.id,sizeof(cmd.id),"%s",id);
    else snprintf(cmd.id,sizeof(cmd.id),"bno-%lu",static_cast<unsigned long>(++nextCommand));
    cmd.zAxis=input["z_axis"]|false;
    if(!commands || xQueueSend(commands,&cmd,0)!=pdTRUE){reply["error"]="queue_full";return false;}
    reply["accepted"]=true;reply["state"]="queued";reply["command_id"]=cmd.id;
    return true;
}
void begin(WebServer &server,bool (*authorize)()){
    if(started)return;
    started=true;
    commands=xQueueCreate(COMMAND_DEPTH,sizeof(Command));
    // Idle priority shares the background budget with video; LiDAR/Wi-Fi remain higher priority.
    workerReady=commands && xTaskCreate(worker,"rear-bno",6144,nullptr,0,nullptr)==pdPASS;
    if(!workerReady.load()){
        current.state="unavailable";current.error="worker_unavailable";publish();
    }
    server.on("/api/bno",HTTP_GET,[&server,authorize](){
        if(!authorize || !authorize())return;
        DynamicJsonDocument d(4096);writeJson(d.to<JsonObject>(),nowUs());
        String text;serializeJson(d,text);server.send(200,"application/json",text);
    });
    server.on("/api/bno",HTTP_POST,[&server,authorize](){
        if(!authorize || !authorize())return;
        DynamicJsonDocument input(512),output(512);
        if(deserializeJson(input,server.arg("plain"))){
            server.send(400,"application/json","{\"error\":\"invalid_json\"}");return;
        }
        const bool accepted=enqueueCommand(input.as<JsonObjectConst>(),output.to<JsonObject>());
        String text;serializeJson(output,text);server.send(accepted?202:409,"application/json",text);
    });
}
void pause(bool value){paused=value;}
}
