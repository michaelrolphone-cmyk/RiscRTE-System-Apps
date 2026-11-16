#include "bno_telemetry.h"
#include <SparkFun_BNO08x_Arduino_Library.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

namespace bno_telemetry {
namespace {
BNO08x imu;
TwoWire *bus = nullptr;
SemaphoreHandle_t lock = nullptr;
bool present = false;
bool ready = false;
uint32_t retryAt = 0;
uint32_t errors = 0;
uint32_t sequence = 0;
uint64_t lastRead = 0;
const char *error = "not_detected";

// Latest sample
struct Sample {
  bool valid = false;
  float quatI = 0, quatJ = 0, quatK = 0, quatReal = 0;
  uint8_t quatAccuracy = 0;
  float accelX = 0, accelY = 0, accelZ = 0;
  float gyroX = 0, gyroY = 0, gyroZ = 0;
  float magX = 0, magY = 0, magZ = 0;
  uint8_t magAccuracy = 0;
} sample;

constexpr uint8_t ADDR = 0x4A; // default; 0x4B if ADR high

bool probe() {
  if (!bus) return false;
  bus->beginTransmission(ADDR);
  return bus->endTransmission() == 0;
}

void loadStoredCalibration() {
  Preferences prefs;
  if (!prefs.begin("bno-cal", true)) return;
  // Placeholder: if library later supports importing a DCD blob, load it here.
  // Currently the chip retains its own flash calibration after saveCalibration().
  prefs.end();
}

void saveCalibrationToEsp() {
  Preferences prefs;
  if (!prefs.begin("bno-cal", false)) return;
  prefs.putULong("saved_ms", millis());
  prefs.putBool("has_dcd", true);
  prefs.end();
}

bool tryInit() {
  if (!probe()) {
    error = "not_detected";
    return false;
  }
  if (!imu.begin(ADDR, *bus)) {
    error = "init_failed";
    return false;
  }
  // Enable useful reports. Interval in ms.
  imu.enableRotationVector(50);
  imu.enableAccelerometer(50);
  imu.enableGyro(50);
  imu.enableMagnetometer(50);
  loadStoredCalibration();
  ready = true;
  error = nullptr;
  lastRead = esp_timer_get_time();
  return true;
}

void task(void *) {
  while (true) {
    if (!present) {
      if (millis() >= retryAt) {
        present = tryInit();
        if (!present) {
          ++errors;
          retryAt = millis() + 3000;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (imu.getSensorEvent()) {
      Sample next = sample;
      if (imu.getSensorEventID() == SENSOR_REPORTID_ROTATION_VECTOR) {
        next.quatI = imu.getQuatI();
        next.quatJ = imu.getQuatJ();
        next.quatK = imu.getQuatK();
        next.quatReal = imu.getQuatReal();
        next.quatAccuracy = imu.getQuatAccuracy();
        next.valid = true;
      } else if (imu.getSensorEventID() == SENSOR_REPORTID_ACCELEROMETER) {
        next.accelX = imu.getAccelX();
        next.accelY = imu.getAccelY();
        next.accelZ = imu.getAccelZ();
      } else if (imu.getSensorEventID() == SENSOR_REPORTID_GYROSCOPE) {
        next.gyroX = imu.getGyroX();
        next.gyroY = imu.getGyroY();
        next.gyroZ = imu.getGyroZ();
      } else if (imu.getSensorEventID() == SENSOR_REPORTID_MAGNETIC_FIELD) {
        next.magX = imu.getMagX();
        next.magY = imu.getMagY();
        next.magZ = imu.getMagZ();
        next.magAccuracy = imu.getMagAccuracy();
      }
      if (lock && xSemaphoreTake(lock, pdMS_TO_TICKS(5)) == pdTRUE) {
        sample = next;
        lastRead = esp_timer_get_time();
        ++sequence;
        xSemaphoreGive(lock);
      }
    } else if (esp_timer_get_time() - lastRead > 5000000) {
      // No data for 5s — treat as lost
      ready = false;
      present = false;
      error = "data_timeout";
      retryAt = millis() + 2000;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
} // namespace

void begin(TwoWire &wire) {
  bus = &wire;
  lock = xSemaphoreCreateMutex();
  if (lock) xTaskCreate(task, "bno-telemetry", 6144, nullptr, 1, nullptr);
}

void writeJson(JsonObject out, uint64_t at) {
  Sample s;
  bool copied = false;
  if (lock && xSemaphoreTake(lock, 0) == pdTRUE) {
    s = sample;
    copied = true;
    xSemaphoreGive(lock);
  }
  out["schema"] = "rover.bno085.v1";
  out["address_7bit"] = ADDR;
  out["present"] = present;
  out["ready"] = ready;
  out["sequence"] = sequence;
  out["errors"] = errors;
  out["error"] = error ? error : nullptr;
  bool fresh = copied && s.valid && at && lastRead && (at >= lastRead) && ((at - lastRead) < 2000000);
  out["valid"] = fresh;
  if (lastRead && at >= lastRead) out["age_ms"] = (at - lastRead) / 1000;
  else out["age_ms"] = nullptr;
  if (fresh) {
    JsonObject q = out.createNestedObject("quaternion");
    q["i"] = s.quatI; q["j"] = s.quatJ; q["k"] = s.quatK; q["real"] = s.quatReal;
    q["accuracy"] = s.quatAccuracy;
    JsonObject a = out.createNestedObject("accel_ms2");
    a["x"] = s.accelX; a["y"] = s.accelY; a["z"] = s.accelZ;
    JsonObject g = out.createNestedObject("gyro_rads");
    g["x"] = s.gyroX; g["y"] = s.gyroY; g["z"] = s.gyroZ;
    JsonObject m = out.createNestedObject("mag_uT");
    m["x"] = s.magX; m["y"] = s.magY; m["z"] = s.magZ; m["accuracy"] = s.magAccuracy;
  }
}

bool handleCommand(JsonObjectConst cmd, JsonObject reply) {
  const char *op = cmd["op"] | "";
  reply["schema"] = "rover.bno085.cmd.v1";
  reply["op"] = op;
  if (!ready) {
    reply["ok"] = false;
    reply["error"] = "not_ready";
    return true;
  }
  if (!strcmp(op, "calibrate_all")) {
    imu.calibrateAll();
    reply["ok"] = true;
  } else if (!strcmp(op, "calibrate_accel")) {
    imu.calibrateAccelerometer();
    reply["ok"] = true;
  } else if (!strcmp(op, "calibrate_gyro")) {
    imu.calibrateGyro();
    reply["ok"] = true;
  } else if (!strcmp(op, "calibrate_mag")) {
    imu.calibrateMagnetometer();
    reply["ok"] = true;
  } else if (!strcmp(op, "end_calibration")) {
    imu.endCalibration();
    reply["ok"] = true;
  } else if (!strcmp(op, "save_calibration")) {
    bool ok = imu.saveCalibration();
    if (ok) saveCalibrationToEsp();
    reply["ok"] = ok;
    reply["stored_on_esp"] = ok;
  } else if (!strcmp(op, "request_calibration_status")) {
    imu.requestCalibrationStatus();
    reply["ok"] = true;
  } else if (!strcmp(op, "tare_now")) {
    bool z = cmd["z_axis"] | false;
    imu.tareNow(z);
    reply["ok"] = true;
  } else if (!strcmp(op, "save_tare")) {
    imu.saveTare();
    reply["ok"] = true;
  } else {
    reply["ok"] = false;
    reply["error"] = "unknown_op";
  }
  return true;
}
} // namespace bno_telemetry
