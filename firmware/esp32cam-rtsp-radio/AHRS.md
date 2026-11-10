# Stratux AHRS v1.1 telemetry on the ESP32-S3 CAM

Firmware version: `1.0-radio.4`. Target: `esp32cam_s3_wroom_n16r8`.

This adds acquisition from the Stratux sensor board to the same firmware that
provides the camera, light controls, Wi-Fi/BLE observations, and FTM. The
ESP32 reads the sensor chips over I²C; a Raspberry Pi or Stratux network service
is not required.

## Supported sensor profile and evidence

The expected v1.1 profile is **MPU-9250 accelerometer/gyroscope, its AK8963
magnetometer, and BMP280 pressure sensor**. The
[vendor's v1.1 installation guide](https://crewdogelectronics.com/wp-content/uploads/instructions/ahrs.pdf)
requires Stratux v1.3b2 or later. That
[historical firmware's sensor code](https://github.com/cyoung/stratux/blob/c6e96b5d094f77330a611fe49a79ffe58c208000/main/sensors.go)
initializes MPU9250 and BMP280 explicitly.

The installation PDF has no schematic or printed component list. This profile
is therefore based on the documented firmware compatibility and is checked
against actual chip IDs at runtime. It is not a claim that the physical board
has been tested. A different IMU or barometer is reported as unavailable;
the driver does not assume that every Stratux board revision uses these chips.

| Sensor | I²C address checked | Identity |
| --- | --- | --- |
| MPU-9250 | `0x68`, then `0x69` | Register `0x75` = `0x71` |
| AK8963 | `0x0C` through MPU bypass | Register `0x00` = `0x48` |
| BMP280 | `0x76`, then `0x77` | Register `0xD0` = `0x58` |

An absent or failed sensor does not stop the camera or the other telemetry.
The driver retries discovery and reports each sensor's presence, validity,
age, and errors separately.

## Wiring to the FORIOT N16R8

Use the **physical Raspberry Pi header positions** that the Stratux module
normally occupies, not Raspberry Pi BCM GPIO numbers. The v1.1 guide places it
over physical pins 1–12.

Power down while connecting. The following reproduces the power rails and
I²C pins supplied by the intended Raspberry Pi installation:

| Stratux connector physical position | Function | FORIOT ESP32-S3 CAM connection |
| --- | --- | --- |
| 1 | 3.3 V supply | `3V3` |
| 2 and 4 | 5 V supply | `5V` |
| 3 | SDA, 3.3 V logic | `GPIO21` |
| 5 | SCL, 3.3 V logic | `GPIO47` |
| 6 and 9 | Ground | `GND` |
| 7, 8, 10, 11, 12 | Other Pi GPIO functions | Leave disconnected |
| Fan connector | Fan output | Leave unused |

This supply mapping is inferred from the intended Pi header connection; the
internal v1.1 power schematic was not available. Supplying both documented
rails avoids assuming that the module works from 3.3 V alone. **5 V goes only
to the power contacts, never SDA, SCL, or an ESP32 GPIO.** Use the FORIOT's
existing supply rails rather than joining independent powered supplies.

The vendor's component-side Pi photo shows pins 1–12 at the SD-card end.
The mating connector viewed from underneath is mirrored. Identify pin 1 from
that documented orientation before attaching wires.

GPIO21 and GPIO47 are exposed on the FORIOT's right header in the
[seller's pinout](https://m.media-amazon.com/images/I/81e8TEDdnsL._AC_SL1500_.jpg).
From the bottom upward: GND, GPIO19, GPIO20, GPIO21, GPIO47.
The selected pair avoids camera signals, native USB, GPIO48 RGB LED, SD pins,
and the N16R8 octal-PSRAM pins.

### I²C pull-ups and controller

Use short wiring and a 100 kHz bus. SDA and SCL need pull-ups to **3.3 V**.
If the AHRS module has no fitted pull-ups, add one **4.7 kΩ resistor from SDA
to 3V3** and one **4.7 kΩ resistor from SCL to 3V3**. The Raspberry Pi supplies
its own fixed I²C pull-ups; the module may have relied on those. Do not assume
that ESP32 weak internal pulls replace them.

The [pinned Arduino S3 SDK configuration](https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32s3/sdkconfig)
assigns camera SCCB to hardware I²C controller **1**. AHRS selects the other
controller, **0**, using GPIO21/47. It does not take over the camera's
GPIO4/5 control bus.

Pi pin and pull-up reference:
[Raspberry Pi GPIO documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#gpio).

## Web portal and first check

Open `http://<camera-ip>/telemetry#ahrs`, or choose
**Camera light, AHRS and RF telemetry** on the camera home page.

The AHRS panel shows:

- Roll, pitch, and relative yaw in degrees.
- Acceleration in m/s², including gravity.
- Angular velocity in rad/s.
- Magnetic field in µT.
- Pressure in hPa; API pressure is in Pa.
- IMU and barometer die temperatures in °C.
- Pressure altitude relative to standard sea-level pressure.
- Per-sensor age, presence/validity, gyro calibration state, and errors.

The panel refreshes the AHRS snapshot independently of the radio tables.
A disconnected API or stale/invalid measurement displays unavailable values.

To test after wiring and flashing:

1. Confirm that the panel reports IMU samples and shows the expected sensor
   identities in **Open AHRS JSON**.
2. Leave the board stationary and select **Calibrate gyro while still**.
3. Keep it still for the approximately two-second collection window. Check for
   `succeeded`; motion, missing samples, or a pause rejects the new bias.
4. Tilt the board slowly and watch roll/pitch and acceleration change.
5. Rotate it and watch relative yaw/angular velocity. The magnetic-field
   vector is displayed separately.

Calibration is an explicit stationary measurement, not an automatic assumption
that the board is stationary at boot. It applies an accepted measured gyro
bias in RAM. A rejected stationary window or pause keeps the previous bias. Reboot or
IMU reinitialization clears it, so calibrate again when appropriate.

## API

These routes use the existing camera authentication callback:

| Method / path | Purpose |
| --- | --- |
| `GET /api/ahrs` | Cached full AHRS snapshot with receiver/boot/time envelope |
| `POST /api/ahrs/calibrate` | Queue stationary gyro-bias calibration |
| `GET /api/telemetry` | Existing radio snapshot plus nested `ahrs` state |
| `POST /api/telemetry/config` | Configure UDP destination and AHRS publication interval |

Read:

```sh
curl -u USER:PASSWORD http://CAMERA_IP/api/ahrs
```

Queue calibration:

```sh
curl -u USER:PASSWORD -H 'Content-Type: application/json' \
  --data '{}' http://CAMERA_IP/api/ahrs/calibrate
```

The calibration route returns HTTP202 when queued, HTTP409 if a calibration
is already pending, and HTTP503 when no fresh running IMU is available.
Read the subsequent snapshot for the actual calibration result.
There is no sensor I/O or blocking calibration work inside an HTTP handler.

The AHRS object has `worker_ready`, `paused`, `imu`, `mag`, `barometer`,
`attitude`, and `calibration`. Full HTTP output includes bus configuration,
chip identity, acquisition diagnostics, frame/units metadata, and sample
uptime. Primary measurement fields are:

| Object | Measurements / quality fields |
| --- | --- |
| `imu` | `present`, `valid`, `age_ms`, `sample_count`, `accel_m_s2[3]`, `gyro_rad_s[3]`, `temp_c` |
| `mag` | `present`, `valid`, `age_ms`, `field_uT[3]`, `calibration` |
| `barometer` | `present`, `valid`, `age_ms`, `pressure_pa`, `temp_c`, `pressure_altitude_m` |
| `attitude` | `valid`, `age_ms`, `quaternion_wxyz[4]`, `roll_deg`, `pitch_deg`, `yaw_deg`, `reference`, `generation`, `yaw_origin_uptime_ms` |
| `calibration` | `status`, `gyro_bias_valid`, `gyro_bias_rad_s[3]`, `samples` |

Invalid or stale measurement values are JSON `null`, not zero. Observation
age is independent of publication time. Presence means a recognized sensor was
found; it does not by itself establish a current valid measurement.

## UDP and the computer receiver

AHRS uses an additional schema-1 packet type, `"type":"ahrs"`, with the
same receiver ID, boot ID, sequence, uptime, and synchronized timestamp envelope
as the radio messages. The compact measurements are in `"ahrs":{...}`.

The default AHRS UDP interval is **100 ms (10 Hz)**, independently of radio
publication. The acquisition worker targets **50 Hz**; UDP carries the latest
snapshot rather than a backlog of every sample. Timing is best-effort while
sharing the camera's CPU/network. No hard real-time or lossless guarantee is
claimed.

Use the existing UDP receiver address/port. For example:

```json
{"udp_host":"192.168.1.60","udp_port":9876,"ahrs_publish_interval_ms":100}
```

An empty destination keeps UDP output disabled. The interval can be set in
the web settings or API from 50 ms through 86,400,000 ms. Existing radio
settings and packet formats remain compatible.

The included `tools/telemetry_receiver.py` accepts the new packets, preserves
the whole record in JSONL/optional rotating logs, and shows attitude/pressure
in `--view`. It validates the new measurements and keeps per-receiver sequence
ordering and sample age. The live view marks stale attitude and pressure
unavailable instead of keeping old values looking live.

## Frames, interpretation, and limitations

The accelerometer/gyro axes are the MPU9250 native axes. The magnetic vector
uses **AK8963 native axes**; do not treat it as aligned with the IMU or robot
without applying the appropriate mounting/axis transform.

The attitude estimator uses gyro and accelerometer only. Roll/pitch are
gravity-referenced estimates; linear acceleration and vibration affect them.
Yaw is relative to the filter's startup/reset orientation and drifts. It is
not north-referenced heading. `attitude.generation` and
`yaw_origin_uptime_ms` identify a new filter/yaw origin after initialization,
a sample gap, or reinitialization.

The magnetometer has its factory sensitivity adjustment applied. It has no
installation-specific hard/soft-iron calibration and is not fused as a compass.
Nearby motors, wiring, magnets, and steel can change its measurements.

Pressure altitude uses **101325 Pa** as the standard reference. Weather changes
shift it; it is not ground clearance or a measured geographic altitude.
The sensor temperatures describe the chips, not a calibrated room thermometer.

This is additional sensor telemetry, not a complete robot localization system.
No translational velocity, XY position, or automatic camera-to-IMU mounting
calibration is claimed.

## Implementation references

- [TDK MPU9250 product documentation](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ps-mpu-9250a-01-v1.1.pdf)
- [SparkFun's MPU9250/AK8963 driver](https://github.com/sparkfun/SparkFun_MPU-9250_Breakout_Arduino_Library/blob/master/src/MPU9250.cpp)
- [Bosch BMP280 datasheet and compensation formula](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf)
- [Camera SCCB driver and controller selection](https://github.com/espressif/esp32-camera/blob/v2.0.4/driver/sccb.c)

The software is built and checked through the package's scoped S3 workflow.
Physical wiring, sensor reception, attitude accuracy, vibration behavior,
and simultaneous camera/radio/sensor operation still require a test on the
actual board; see `VALIDATION.md` for the recorded qualification scope.
