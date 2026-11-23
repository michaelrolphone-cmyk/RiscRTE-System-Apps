# Rear LiDAR driver boundary regression

Run after materializing the rear dependency:

```sh
python firmware/esp32cam-rear-lidar/test_lidar_driver.py --prepared /path/to/prepared
```

The harness includes the actual SparkFun VL53L5CX 1.0.3 IO, platform, ULD and
Arduino wrapper source files. Only Wire and time are replaced. It covers exact
read lengths, chunked success, transport errors, timeout and clock wrap,
delayed replies, MCU errors, initialization/configuration failure propagation,
readiness identity, range parsing, and service of a healthy second address
after two failed first-head initialization attempts.

The same harness is compiled against the exact unchanged source saved in the
patch receipt. Seven selected cases must fail there. A deterministic safety
guard turns the old infinite polls into test failures without hanging the host.
The patch guard also rejects drift before writing and validates repeat builds.

This is not a physical I2C test or a complete FreeRTOS/acquisition-task model.
It proves that calls return and that independent per-head data remains usable.
The rear still shares one acquisition worker: an initialization or firmware
download temporarily delays the other head. The two-second limit applies to
each response poll, plus its bounded in-flight I2C operation; it is not a
two-second limit for every complete successful initialization. No pins, sensor
rates, task priorities, camera settings, BNO behavior or front firmware change.
