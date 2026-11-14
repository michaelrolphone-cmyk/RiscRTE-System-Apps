# ESP32-S3 camera sensor and FTM firmware

This variant streams camera frames with capture-aligned AHRS and four sonar measurements. It retains explicit Wi-Fi FTM requests and removes the old Wi-Fi/Bluetooth RSSI location approximation.

There is no Bluetooth initialization, advertising scan, scheduled Wi-Fi scan, RSSI smoothing, path-loss range estimate, radio fingerprint table, or Wi-Fi/BLE observation payload. The Wi-Fi station stays awake for camera traffic. Normal network connectivity and provisioning remain available.

## Build

Run `python3 prepare.py --output /absolute/empty/project`, then `pio run -d /absolute/empty/project -e esp32cam_s3_wroom_n16r8`. The manifest pins the upstream revision and package checksums. Firmware version: `1.0-radio.16-capture-imu`. Each frame includes up to 32 distinct IMU samples from the preceding 650 ms on its boot clock. The worker checks the MPU acceleration range, gyro range and bypass configuration once per second and reinitializes the sensor if those registers lose their settings.

## Interfaces

| Interface | Purpose |
| --- | --- |
| GET /api/telemetry | Sensor, FTM and delivery status; no scan arrays |
| GET/POST /api/telemetry/config | UDP destination/port and status/AHRS delivery cadence |
| POST /api/telemetry/ftm | Request a known responder using address and channel |
| GET /api/ahrs | IMU, attitude, magnetometer and pressure |
| POST /api/ahrs/calibrate | Request stationary gyro calibration |
| GET /api/ultrasonic | Four mounted HC-SR04 slots and their original sample ages |
| GET/POST /api/light | Existing authenticated light controls |
| GET /telemetry | Light, AHRS, sonar and FTM portal |

Existing authentication, camera exposure controls, capture identifiers, sensor timestamps, LED wiring and sonar pin assignments are preserved. Old scan/calibration settings are discarded when loading saved delivery settings; they cannot re-enable collection.

FTM requests contain `{"address":"14:63:93:94:0f:29","channel":9}`. The asynchronous result reports target, RTT, distance, sample count, age, cancellation and failures. A failed measurement never supplies a fabricated distance. Cancelling a session still drains its final report before another request can start.

The four C3 anchors supply their identities and home channels to the coordinator; the rover no longer scans for channel discovery. A full location solution requires qualified anchor geometry and fresh ranges. ToF is asynchronous and is not represented as a camera-aligned position or an automatic SLAM world-map replacement.

UDP publication retains summary/FTM, AHRS and ultrasonic packets. The included receiver rejects the retired observation packet type. It creates no files unless an explicit rotating log is requested.

See [AHRS.md](AHRS.md), [ULTRASONIC.md](ULTRASONIC.md), [CAMERA_EXPOSURE.md](CAMERA_EXPOSURE.md) and [SYNCHRONIZATION.md](SYNCHRONIZATION.md) for the retained sensor interfaces and physical mounting.

## Deployment evidence

The S3 build passed without NimBLE. An authenticated OTA upload changed the camera boot identity and live telemetry confirmed absent scan arrays/settings, working FTM support, valid IMU data and four sonar channels. Wi-Fi/FTM ranging through the house remains a separate accuracy and coverage qualification; firmware installation does not establish a navigation fix.

This package retains the GPL-3.0 upstream license and pinned source reference.

## Reciprocal ranging

This version adds an FTM responder while preserving the station connection. See
[reciprocal ranging](../esp32c3-tof-anchor/RECIPROCAL.md) for directional data,
quality rules, configuration and compatibility with the deployed anchors.
