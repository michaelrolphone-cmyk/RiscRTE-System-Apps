#pragma once
#include <ArduinoJson.h>

namespace ahrs_telemetry {
enum class CalibrationRequest { Accepted, Busy, Unavailable };
// Owns the I2C controller opposite the SDK camera SCCB controller; no HTTP routes.
void begin();
// Requests an asynchronous pause. An in-flight bounded I2C operation may finish.
void pause(bool paused);
// Copies the latest bounded snapshot. Never touches the sensor bus.
void writeJson(JsonObject output, bool compact = false);
// Queues a two-second stationary gyro-bias window; no bus work on the caller.
CalibrationRequest requestGyroCalibration();
} // namespace ahrs_telemetry
