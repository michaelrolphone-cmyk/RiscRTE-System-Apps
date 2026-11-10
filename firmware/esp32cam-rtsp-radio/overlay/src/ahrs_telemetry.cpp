#include "ahrs_telemetry.h"
#include "ahrs_telemetry_model.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <cstring>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This AHRS wiring profile is for the ESP32-S3 CAM only."
#endif

namespace ahrs_telemetry {
namespace {
namespace model = ahrs_telemetry_model;
// esp32-camera 2.0.4 SCCB_Init selects the controller from SDK configuration,
// not CAMERA_CONFIG_SCCB_I2C_PORT when camera SDA/SCL pins are supplied.
#if defined(CONFIG_SCCB_HARDWARE_I2C_PORT1) && CONFIG_SCCB_HARDWARE_I2C_PORT1
constexpr uint8_t CAMERA_BUS = 1;
#elif defined(CONFIG_SCCB_HARDWARE_I2C_PORT0) && CONFIG_SCCB_HARDWARE_I2C_PORT0
constexpr uint8_t CAMERA_BUS = 0;
#else
#error "Confirm the SDK camera SCCB controller before enabling the AHRS bus."
#endif
constexpr uint8_t SENSOR_BUS = 1-CAMERA_BUS;
// GPIO41/42 are also external JTAG pins; do not share them with a pin-JTAG adapter.
constexpr int SDA_PIN = 41, SCL_PIN = 42;
constexpr uint32_t BUS_HZ = 100000;
constexpr uint32_t RETRY_MS = 5000;
static_assert(SENSOR_BUS != CAMERA_BUS, "AHRS must not own the camera SCCB controller.");
static_assert(SDA_PIN != SCL_PIN, "AHRS SDA and SCL must differ.");
constexpr bool reservedPin(int pin) {
    return pin == 4 || pin == 5 || pin == 48 || (pin >= 35 && pin <= 40) ||
           pin == 19 || pin == 20 || pin == 43 || pin == 44;
}
static_assert(!reservedPin(SDA_PIN) && !reservedPin(SCL_PIN),
              "AHRS pins conflict with camera, RGB, PSRAM, SD, USB, or UART.");
#define CHECK_CAMERA_PIN(pin) static_assert(SDA_PIN != static_cast<int>(pin) && SCL_PIN != static_cast<int>(pin), "AHRS pin conflicts with " #pin)
#ifdef CAMERA_CONFIG_PIN_PWDN
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_PWDN);
#endif
#ifdef CAMERA_CONFIG_PIN_RESET
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_RESET);
#endif
#ifdef CAMERA_CONFIG_PIN_XCLK
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_XCLK);
#endif
#ifdef CAMERA_CONFIG_PIN_SCCB_SDA
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_SCCB_SDA);
#endif
#ifdef CAMERA_CONFIG_PIN_SCCB_SCL
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_SCCB_SCL);
#endif
#ifdef CAMERA_CONFIG_PIN_Y2
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y2);
#endif
#ifdef CAMERA_CONFIG_PIN_Y3
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y3);
#endif
#ifdef CAMERA_CONFIG_PIN_Y4
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y4);
#endif
#ifdef CAMERA_CONFIG_PIN_Y5
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y5);
#endif
#ifdef CAMERA_CONFIG_PIN_Y6
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y6);
#endif
#ifdef CAMERA_CONFIG_PIN_Y7
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y7);
#endif
#ifdef CAMERA_CONFIG_PIN_Y8
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y8);
#endif
#ifdef CAMERA_CONFIG_PIN_Y9
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y9);
#endif
#ifdef CAMERA_CONFIG_PIN_VSYNC
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_VSYNC);
#endif
#ifdef CAMERA_CONFIG_PIN_HREF
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_HREF);
#endif
#ifdef CAMERA_CONFIG_PIN_PCLK
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_PCLK);
#endif
#undef CHECK_CAMERA_PIN

struct Health {
    bool present=false, valid=false, seen=false;
    uint8_t address=0, identity=0;
    uint32_t observed=0, samples=0, errors=0, consecutive=0;
    const char *error="not_detected";
    int i2cError=0;
};
struct Snapshot {
    bool workerReady=false, paused=false, busReady=false;
    const char *workerError="not_started";
    Health imu, mag, barometer;
    model::ImuReading imuValue;
    model::Vec3 correctedGyro, magneticField, magScale, gyroBias;
    model::BarometerReading barometerValue;
    bool magTrimValid=false, gyroBiasValid=false;
    uint32_t magOverruns=0;
    model::Attitude attitude;
    model::CalibrationState calibration=model::CalibrationState::Uncalibrated;
    uint32_t calibrationSamples=0;
};

TwoWire sensorWire(SENSOR_BUS);
SemaphoreHandle_t snapshotMutex=nullptr;
Snapshot latest;
bool requestedPause=false, calibrationQueued=false;
bool started=false;
int lastBusError=0; // worker-owned; never accessed by the HTTP caller

struct Lock {
    Lock() { xSemaphoreTake(snapshotMutex,portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(snapshotMutex); }
};
void increment(uint32_t &value) { if (value != UINT32_MAX) ++value; }

void publish(const Snapshot &value) {
    Lock lock;
    latest=value;
    latest.paused=requestedPause;
    if (calibrationQueued) latest.calibration=model::CalibrationState::Queued;
}
void controls(bool &paused, bool &calibrate) {
    Lock lock;
    paused=requestedPause;
    calibrate=calibrationQueued;
    calibrationQueued=false;
}
bool pauseRequested() {
    Lock lock;
    return requestedPause;
}
void fail(Health &health, const char *error) {
    health.valid=false;
    health.error=error;
    health.i2cError=lastBusError;
    increment(health.errors);
    increment(health.consecutive);
}
void observed(Health &health, uint32_t now) {
    health.valid=health.seen=health.present=true;
    health.observed=now;
    health.error=nullptr;
    health.i2cError=0;
    health.consecutive=0;
    increment(health.samples);
}
bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    if (pauseRequested()) { lastBusError=-3; return false; }
    sensorWire.beginTransmission(address);
    const size_t registerBytes=sensorWire.write(reg);
    const size_t valueBytes=sensorWire.write(value);
    const size_t bytes=registerBytes+valueBytes;
    const uint8_t result=sensorWire.endTransmission(true);
    lastBusError=bytes == 2 ? result : 1;
    return lastBusError == 0;
}
bool readRegisters(uint8_t address, uint8_t reg, uint8_t *bytes, size_t count) {
    if (pauseRequested()) { lastBusError=-3; return false; }
    sensorWire.beginTransmission(address);
    const size_t written=sensorWire.write(reg);
    if (written != 1) {
        sensorWire.endTransmission(true);
        lastBusError=1;
        return false;
    }
    const uint8_t result=sensorWire.endTransmission(false);
    if (result != 0) { lastBusError=result; return false; }
    const size_t received=sensorWire.requestFrom(address,count,true);
    if (received != count) {
        while (sensorWire.available()) sensorWire.read();
        lastBusError=-1;
        return false;
    }
    for (size_t i=0;i<count;++i) {
        const int value=sensorWire.read();
        if (value < 0) { lastBusError=-2; return false; }
        bytes[i]=static_cast<uint8_t>(value);
    }
    lastBusError=0;
    return true;
}
bool writeVerified(uint8_t address, uint8_t reg, uint8_t value, uint8_t mask=0xff) {
    uint8_t actual=0;
    if (!writeRegister(address,reg,value) || !readRegisters(address,reg,&actual,1)) return false;
    if ((actual & mask) != (value & mask)) { lastBusError=-4; return false; }
    return true;
}
void waitMs(uint32_t milliseconds) { vTaskDelay(pdMS_TO_TICKS(milliseconds)); }

bool findImu(Health &health) {
    bool responded=false;
    const uint8_t addresses[]={0x68,0x69};
    for (uint8_t address : addresses) {
        uint8_t id=0;
        if (!readRegisters(address,0x75,&id,1)) continue;
        responded=true;
        health.address=address; health.identity=id;
        if (!model::imuIdentity(id)) continue;
        health.present=true;
        return true;
    }
    health.present=false;
    fail(health,responded ? "unsupported_identity" : "not_detected");
    return false;
}
bool initializeImu(Snapshot &state) {
    Health &health=state.imu;
    health.valid=false;
    if (!findImu(health)) return false;
    const uint8_t address=health.address;
    if (!writeRegister(address,0x6b,0x80)) { fail(health,"reset_failed"); return false; }
    waitMs(100);
    // Clock PLL, all axes enabled; gyro +/-250 deg/s; accel +/-4g.
    // Gyro DLPF20Hz, accel DLPF21Hz; sample divisor19 =>50Hz.
    if (!writeVerified(address,0x6b,0x01,0x7f) ||
        !writeVerified(address,0x6c,0x00,0x3f) ||
        !writeVerified(address,0x1b,0x00,0x1b) ||
        !writeVerified(address,0x1c,0x08,0x18) ||
        !writeVerified(address,0x1a,0x04,0x07) ||
        !writeVerified(address,0x1d,0x04,0x0f) ||
        !writeVerified(address,0x19,19) ||
        !writeVerified(address,0x6a,0x00,0x20) ||
        !writeVerified(address,0x37,0x02,0x02) ||
        !writeVerified(address,0x38,0x01,0x01)) {
        fail(health,"configuration_failed");
        return false;
    }
    waitMs(10);
    health.seen=false;
    health.error="awaiting_sample";
    health.consecutive=0;
    state.gyroBias=model::Vec3();
    state.gyroBiasValid=false;
    state.calibration=model::CalibrationState::Uncalibrated;
    state.calibrationSamples=0;
    state.mag.valid=false;
    state.mag.present=false;
    state.magTrimValid=false;
    state.attitude.reset(state.attitude.generation() ? model::ResetReason::Reconnected : model::ResetReason::Startup);
    return true;
}
bool initializeMag(Snapshot &state) {
    Health &health=state.mag;
    health.valid=false;
    health.address=0x0c;
    uint8_t id=0;
    if (!readRegisters(health.address,0x00,&id,1)) {
        health.present=false; fail(health,"not_detected"); return false;
    }
    health.identity=id;
    health.present=model::magIdentity(id);
    if (!health.present) { fail(health,"unsupported_identity"); return false; }
    // MPU bypass enabled, I2C master disabled by initializeImu.
    if (!writeRegister(health.address,0x0a,0x00)) { fail(health,"power_down_failed"); return false; }
    waitMs(10);
    if (!writeRegister(health.address,0x0a,0x0f)) { fail(health,"fuse_mode_failed"); return false; }
    waitMs(10);
    uint8_t asa[3]={};
    if (!readRegisters(health.address,0x10,asa,3)) { fail(health,"fuse_read_failed"); return false; }
    const bool blank=(asa[0]==0 && asa[1]==0 && asa[2]==0) ||
                     (asa[0]==0xff && asa[1]==0xff && asa[2]==0xff);
    if (blank) { lastBusError=0; fail(health,"invalid_factory_trim"); return false; }
    state.magScale=model::magFactoryScale(asa);
    if (!writeRegister(health.address,0x0a,0x00)) { fail(health,"power_down_failed"); return false; }
    waitMs(10);
    if (!writeRegister(health.address,0x0a,0x16)) { fail(health,"configuration_failed"); return false; }
    waitMs(10);
    uint8_t mode=0;
    if (!readRegisters(health.address,0x0a,&mode,1) || (mode & 0x1f) != 0x16) {
        fail(health,"configuration_failed"); return false;
    }
    state.magTrimValid=true;
    health.seen=false;
    health.error="awaiting_sample";
    health.consecutive=0;
    return true;
}
bool initializeBarometer(Snapshot &state, model::BmpCalibration &calibration) {
    Health &health=state.barometer;
    health.valid=false;
    bool responded=false, found=false;
    const uint8_t addresses[]={0x76,0x77};
    for (uint8_t address : addresses) {
        uint8_t id=0;
        if (!readRegisters(address,0xd0,&id,1)) continue;
        responded=true;
        health.address=address; health.identity=id;
        if (!model::barometerIdentity(id)) continue;
        found=true;
        break;
    }
    health.present=found;
    if (!found) { fail(health,responded ? "unsupported_identity" : "not_detected"); return false; }
    if (!writeRegister(health.address,0xe0,0xb6)) { fail(health,"reset_failed"); return false; }
    waitMs(5);
    bool copied=false;
    for (unsigned i=0;i<20;++i) {
        uint8_t status=0;
        if (!readRegisters(health.address,0xf3,&status,1)) { fail(health,"status_read_failed"); return false; }
        if (!(status & 0x01)) { copied=true; break; }
        waitMs(1);
    }
    if (!copied) { lastBusError=0; fail(health,"nvm_timeout"); return false; }
    uint8_t coefficients[24]={};
    if (!readRegisters(health.address,0x88,coefficients,sizeof(coefficients)) ||
        !model::decodeBmpCalibration(coefficients,calibration)) {
        fail(health,"invalid_factory_trim"); return false;
    }
    // 62.5ms standby, IIR x4; temperature x1, pressure x4, normal mode.
    if (!writeVerified(health.address,0xf5,0x28,0xfd) ||
        !writeVerified(health.address,0xf4,0x2f)) {
        fail(health,"configuration_failed"); return false;
    }
    health.seen=false;
    health.error="awaiting_sample";
    health.consecutive=0;
    return true;
}

void invalidateImu(Snapshot &state, model::StationaryCalibration &calibration) {
    if (state.imu.i2cError != 0) state.imu.present=false;
    state.imu.valid=false;
    if (!state.imu.present) state.mag.present=false;
    state.mag.valid=false;
    state.mag.error="imu_unavailable";
    state.magTrimValid=false;
    state.gyroBiasValid=false;
    state.gyroBias=model::Vec3();
    calibration.interrupt();
    if (state.calibration == model::CalibrationState::Collecting ||
        state.calibration == model::CalibrationState::Queued)
        state.calibration=model::CalibrationState::Interrupted;
    state.attitude.reset(model::ResetReason::Reconnected);
}

void readImu(Snapshot &state, model::StationaryCalibration &calibration) {
    uint8_t status=0;
    if (!readRegisters(state.imu.address,0x3a,&status,1)) {
        fail(state.imu,"read_failed"); return;
    }
    if (!(status & 0x01)) return;
    uint8_t bytes[14]={};
    if (!readRegisters(state.imu.address,0x3b,bytes,sizeof(bytes))) {
        fail(state.imu,"read_failed"); return;
    }
    const model::ImuReading reading=model::decodeImu(bytes);
    const uint32_t now=millis();
    if (reading.saturated || !model::finite(reading.accel) || !model::finite(reading.gyro)) {
        lastBusError=0;
        fail(state.imu,"saturated");
        state.attitude.reset(model::ResetReason::InvalidSample);
        return;
    }
    if (calibration.active()) {
        const model::CalibrationState result=calibration.add(reading.accel,reading.gyro,now);
        state.calibration=result;
        state.calibrationSamples=calibration.samples();
        if (result == model::CalibrationState::Succeeded) {
            state.gyroBias=calibration.bias();
            state.gyroBiasValid=true;
            state.attitude.reset(model::ResetReason::Calibration);
        }
    }
    state.imuValue=reading;
    state.correctedGyro=model::subtract(reading.gyro,state.gyroBias);
    observed(state.imu,now);
    state.attitude.update(reading.accel,state.correctedGyro,now);
}
void readMag(Snapshot &state) {
    uint8_t bytes[8]={};
    if (!readRegisters(state.mag.address,0x02,bytes,sizeof(bytes))) {
        fail(state.mag,"read_failed"); return;
    }
    model::Vec3 field;
    const model::MagResult result=model::decodeMag(bytes,state.magScale,field);
    if (result == model::MagResult::NotReady) return;
    if (result != model::MagResult::Valid) {
        lastBusError=0;
        fail(state.mag,result == model::MagResult::Overflow ? "magnetic_overflow" : "wrong_resolution");
        return;
    }
    if (bytes[0] & 0x02) increment(state.magOverruns);
    state.magneticField=field;
    observed(state.mag,millis());
}
void readBarometer(Snapshot &state, const model::BmpCalibration &calibration) {
    uint8_t status=0;
    if (!readRegisters(state.barometer.address,0xf3,&status,1)) {
        fail(state.barometer,"read_failed"); return;
    }
    if (status & 0x09) return;
    uint8_t bytes[6]={};
    if (!readRegisters(state.barometer.address,0xf7,bytes,sizeof(bytes))) {
        fail(state.barometer,"read_failed"); return;
    }
    model::BarometerReading reading;
    if (!model::decodeBmp(bytes,calibration,reading)) {
        lastBusError=0; fail(state.barometer,"invalid_compensation"); return;
    }
    state.barometerValue=reading;
    observed(state.barometer,millis());
}

void worker(void *) {
    Snapshot state;
    state.workerReady=true;
    state.workerError=nullptr;
    bool imuReady=false, magReady=false, barometerReady=false;
    bool wasPaused=false;
    model::BmpCalibration bmpCalibration;
    model::StationaryCalibration gyroCalibration;
    uint32_t nextBus=0,nextImu=0,nextMag=0,nextBarometer=0,lastBarometer=0;
    uint32_t imuConfigured=0,magConfigured=0,barometerConfigured=0;
    TickType_t wake=xTaskGetTickCount();
    const TickType_t period=pdMS_TO_TICKS(20);
    publish(state);
    for (;;) {
        bool paused=false, calibrate=false;
        controls(paused,calibrate);
        state.paused=paused;
        uint32_t now=millis();
        if (paused) {
            gyroCalibration.interrupt();
            if (calibrate || state.calibration == model::CalibrationState::Collecting)
                state.calibration=model::CalibrationState::Interrupted;
            state.attitude.reset(model::ResetReason::Paused);
            wasPaused=true;
            publish(state);
            waitMs(20);
            wake=xTaskGetTickCount();
            continue;
        }
        if (wasPaused) {
            state.attitude.reset(model::ResetReason::Paused);
            wasPaused=false;
        }
        if (calibrate) {
            if (imuReady && model::fresh(state.imu.valid,state.imu.seen,now,state.imu.observed,model::IMU_FRESH_MS)) {
                gyroCalibration.start(now);
                state.calibration=model::CalibrationState::Collecting;
                state.calibrationSamples=0;
            } else state.calibration=model::CalibrationState::Unavailable;
        }
        if (!state.busReady && static_cast<int32_t>(now-nextBus) >= 0) {
            nextBus=now+RETRY_MS;
            state.busReady=sensorWire.begin(SDA_PIN,SCL_PIN,BUS_HZ);
            sensorWire.setTimeOut(20);
            state.workerError=state.busReady ? nullptr : "i2c_init_failed";
        }
        if (state.busReady) {
            if (!imuReady && static_cast<int32_t>(now-nextImu) >= 0) {
                nextImu=now+RETRY_MS;
                imuReady=initializeImu(state);
                imuConfigured=millis();
                magReady=false;
                nextMag=millis();
            }
            if (imuReady && !magReady && static_cast<int32_t>(now-nextMag) >= 0) {
                nextMag=now+RETRY_MS;
                magReady=initializeMag(state);
                magConfigured=millis();
            }
            if (!barometerReady && static_cast<int32_t>(now-nextBarometer) >= 0) {
                nextBarometer=now+RETRY_MS;
                barometerReady=initializeBarometer(state,bmpCalibration);
                barometerConfigured=millis();
            }
            if (imuReady && !pauseRequested()) readImu(state,gyroCalibration);
            if (magReady && !pauseRequested()) readMag(state);
            now=millis();
            if (barometerReady && now-lastBarometer >= 100 && !pauseRequested()) {
                lastBarometer=now;
                readBarometer(state,bmpCalibration);
            }
            now=millis();
            const uint32_t lastImu=state.imu.seen ? state.imu.observed : imuConfigured;
            const uint32_t lastMag=state.mag.seen ? state.mag.observed : magConfigured;
            const uint32_t lastBaro=state.barometer.seen ? state.barometer.observed : barometerConfigured;
            if (imuReady && (state.imu.consecutive >= 3 || now-lastImu > 1000)) {
                if (now-lastImu > 1000 && !state.imu.consecutive) { lastBusError=0; fail(state.imu,"data_timeout"); }
                invalidateImu(state,gyroCalibration);
                imuReady=magReady=false;
                nextImu=now+RETRY_MS;
            }
            if (magReady && (state.mag.consecutive >= 3 || now-lastMag > 2000)) {
                if (now-lastMag > 2000 && !state.mag.consecutive) { lastBusError=0; fail(state.mag,"data_timeout"); }
                if (state.mag.i2cError != 0) state.mag.present=false;
                state.mag.valid=false;
                magReady=false; nextMag=now+RETRY_MS;
            }
            if (barometerReady && (state.barometer.consecutive >= 3 || now-lastBaro > 5000)) {
                if (now-lastBaro > 5000 && !state.barometer.consecutive) { lastBusError=0; fail(state.barometer,"data_timeout"); }
                if (state.barometer.i2cError != 0) state.barometer.present=false;
                state.barometer.valid=false;
                barometerReady=false; nextBarometer=now+RETRY_MS;
            }
        }
        if (gyroCalibration.active()) {
            gyroCalibration.checkTimeout(millis());
            state.calibration=gyroCalibration.state();
            state.calibrationSamples=gyroCalibration.samples();
        }
        publish(state);
        // Avoid catch-up loops after a bounded sensor reset or bus timeout.
        const TickType_t ticks=xTaskGetTickCount();
        if (static_cast<TickType_t>(ticks-wake) >= period) wake=ticks;
        vTaskDelayUntil(&wake,period);
    }
}

double rounded(double value, double factor) {
    return std::round(value*factor)/factor;
}
void putVector(JsonObject parent, const char *key, const model::Vec3 &vector, bool valid, double factor) {
    if (!valid || !model::finite(vector)) { parent[key]=nullptr; return; }
    JsonArray array=parent.createNestedArray(key);
    array.add(rounded(vector.x,factor)); array.add(rounded(vector.y,factor)); array.add(rounded(vector.z,factor));
}
void putNumber(JsonObject object, const char *key, double value, bool valid, double factor) {
    if (valid && std::isfinite(value)) object[key]=rounded(value,factor);
    else object[key]=nullptr;
}
void putHealth(JsonObject object, const Health &health, bool valid, uint32_t now, bool compact) {
    object["present"]=health.present;
    object["valid"]=valid;
    if (health.seen) object["age_ms"]=static_cast<uint32_t>(now-health.observed);
    else object["age_ms"]=nullptr;
    if (!compact) {
        object["sample_count"]=health.samples;
        if (health.seen) object["observed_uptime_ms"]=health.observed;
        else object["observed_uptime_ms"]=nullptr;
        if (health.address) object["address"]=health.address;
        else object["address"]=nullptr;
        if (health.identity) object["who_am_i"]=health.identity;
        else object["who_am_i"]=nullptr;
        object["error"]=health.error;
        object["i2c_error"]=health.i2cError;
        object["error_count"]=health.errors;
    }
}
} // namespace

void begin() {
    if (started) return;
    started=true;
    snapshotMutex=xSemaphoreCreateMutex();
    if (!snapshotMutex) return;
    if (xTaskCreate(worker,"ahrs-telemetry",6144,nullptr,1,nullptr) != pdPASS) {
        Lock lock;
        latest.workerError="worker_start_failed";
    }
}
void pause(bool paused) {
    if (!snapshotMutex) return;
    Lock lock;
    requestedPause=paused;
    latest.paused=paused;
}
CalibrationRequest requestGyroCalibration() {
    if (!snapshotMutex) return CalibrationRequest::Unavailable;
    Lock lock;
    if (calibrationQueued || latest.calibration == model::CalibrationState::Queued ||
        latest.calibration == model::CalibrationState::Collecting)
        return CalibrationRequest::Busy;
    if (!latest.workerReady || requestedPause ||
        !model::fresh(latest.imu.valid,latest.imu.seen,millis(),latest.imu.observed,model::IMU_FRESH_MS))
        return CalibrationRequest::Unavailable;
    calibrationQueued=true;
    latest.calibration=model::CalibrationState::Queued;
    return CalibrationRequest::Accepted;
}

void writeJson(JsonObject output, bool compact) {
    Snapshot state;
    if (snapshotMutex) { Lock lock; state=latest; }
    else state.workerError="state_memory_unavailable";
    const uint32_t now=millis();
    const bool active=state.workerReady && !state.paused;
    const bool imuValid=active && model::fresh(state.imu.valid,state.imu.seen,now,state.imu.observed,model::IMU_FRESH_MS);
    const bool magValid=active && model::fresh(state.mag.valid,state.mag.seen,now,state.mag.observed,model::MAG_FRESH_MS);
    const bool baroValid=active && model::fresh(state.barometer.valid,state.barometer.seen,now,state.barometer.observed,model::BARO_FRESH_MS);
    const bool attitudeValid=imuValid && state.attitude.valid() &&
        static_cast<uint32_t>(now-state.attitude.observedMs()) <= model::IMU_FRESH_MS;
    output["worker_ready"]=state.workerReady;
    output["paused"]=state.paused;
    if (!compact) {
        output["profile"]="mpu9250_ak8963_bmp280";
        output["error"]=state.workerError;
        output["hardware_verified"]=false;
        output["sample_rate_hz"]=50;
        JsonObject freshness=output.createNestedObject("freshness_ms");
        freshness["imu"]=model::IMU_FRESH_MS; freshness["attitude"]=model::IMU_FRESH_MS;
        freshness["mag"]=model::MAG_FRESH_MS; freshness["barometer"]=model::BARO_FRESH_MS;
        JsonObject bus=output.createNestedObject("bus");
        bus["controller"]=SENSOR_BUS; bus["camera_controller"]=CAMERA_BUS;
        bus["sda"]=SDA_PIN; bus["scl"]=SCL_PIN; bus["clock_hz"]=BUS_HZ;
        bus["ready"]=state.busReady;
    }
    JsonObject imu=output.createNestedObject("imu");
    putHealth(imu,state.imu,imuValid,now,compact);
    imu["sample_count"]=state.imu.samples;
    putVector(imu,"accel_m_s2",state.imuValue.accel,imuValid,1000);
    putVector(imu,"gyro_rad_s",state.correctedGyro,imuValid,10000);
    putNumber(imu,"temp_c",state.imuValue.temperature,imuValid,100);
    if (!compact) {
        imu["frame"]="mpu9250_native_xyz";
        imu["accelerometer_calibrated"]=false;
        imu["gyro_bias_applied"]=state.gyroBiasValid;
        imu["accel_range_g"]=4;
        imu["gyro_range_deg_s"]=250;
        putVector(imu,"gyro_raw_rad_s",state.imuValue.gyro,imuValid,10000);
    }
    JsonObject mag=output.createNestedObject("mag");
    putHealth(mag,state.mag,magValid,now,compact);
    putVector(mag,"field_uT",state.magneticField,magValid,100);
    mag["calibration"]=state.magTrimValid ? "factory_sensitivity_only" : "unavailable";
    if (!compact) {
        mag["frame"]="ak8963_native_xyz";
        mag["hard_iron_calibrated"]=false; mag["soft_iron_calibrated"]=false;
        mag["used_for_heading"]=false;
        mag["data_overruns"]=state.magOverruns;
        putVector(mag,"factory_scale",state.magScale,state.magTrimValid,100000);
    }
    JsonObject barometer=output.createNestedObject("barometer");
    putHealth(barometer,state.barometer,baroValid,now,compact);
    putNumber(barometer,"pressure_pa",state.barometerValue.pressure,baroValid,100);
    putNumber(barometer,"temp_c",state.barometerValue.temperature,baroValid,100);
    putNumber(barometer,"pressure_altitude_m",state.barometerValue.altitude,baroValid,100);
    if (!compact) {
        barometer["sea_level_reference_pa"]=101325;
        barometer["altitude_reference"]="ISA_standard_pressure_not_AGL";
        barometer["compensation_method"]="bmp280_factory_coefficients";
    }
    JsonObject attitude=output.createNestedObject("attitude");
    attitude["valid"]=attitudeValid;
    if (state.attitude.generation()) attitude["age_ms"]=static_cast<uint32_t>(now-state.attitude.observedMs());
    else attitude["age_ms"]=nullptr;
    if (attitudeValid) {
        const model::Quaternion q=state.attitude.quaternion();
        JsonArray quaternion=attitude.createNestedArray("quaternion_wxyz");
        quaternion.add(rounded(q.w,100000)); quaternion.add(rounded(q.x,100000));
        quaternion.add(rounded(q.y,100000)); quaternion.add(rounded(q.z,100000));
    } else attitude["quaternion_wxyz"]=nullptr;
    const model::Vec3 angles=state.attitude.anglesDeg();
    putNumber(attitude,"roll_deg",angles.x,attitudeValid,1000);
    putNumber(attitude,"pitch_deg",angles.y,attitudeValid,1000);
    putNumber(attitude,"yaw_deg",angles.z,attitudeValid,1000);
    attitude["reference"]="relative_yaw";
    attitude["generation"]=state.attitude.generation();
    if (state.attitude.generation()) attitude["yaw_origin_uptime_ms"]=state.attitude.originMs();
    else attitude["yaw_origin_uptime_ms"]=nullptr;
    if (!compact) {
        attitude["filter"]="mahony_6dof_proportional";
        attitude["frame"]="MPU_axes_to_gravity_aligned_initial_yaw";
        attitude["reset_reason"]=model::resetReasonName(state.attitude.resetReason());
        attitude["accel_correction_used"]=attitudeValid && state.attitude.accelCorrection();
        attitude["heading_absolute"]=false;
        attitude["mounting_alignment_applied"]=false;
    }
    JsonObject calibration=output.createNestedObject("calibration");
    calibration["status"]=model::calibrationName(state.calibration);
    putVector(calibration,"gyro_bias_rad_s",state.gyroBias,true,100000);
    calibration["gyro_bias_valid"]=state.gyroBiasValid;
    calibration["samples"]=state.calibrationSamples;
    if (!compact) {
        calibration["window_ms"]=2000;
        calibration["persistent"]=false;
        calibration["requirement"]="Keep the module motionless; low constant yaw motion cannot be distinguished from gyro bias";
        calibration["max_gyro_rate_deg_s"]=5;
        calibration["max_accel_std_m_s2"]=0.15;
        calibration["max_gyro_std_rad_s"]=0.015;
    }
}
} // namespace ahrs_telemetry
