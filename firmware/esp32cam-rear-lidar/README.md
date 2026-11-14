# Rear ESP32-S3-CAM with two VL53L5CX sensors
Derived from the sibling front-camera firmware package at its pinned upstream revision.
The front package is not modified. The rear variant retains RTSP, authenticated
capture at /api/capture, provisioning, exposure/light controls and password-protected
ArduinoOTA on port3232. Two 6MiB OTA application slots are provided.

Camera frames are delivered directly over Wi-Fi regardless of UART or LiDAR state.
Each optional peripheral has an independent worker; no sensor or UART operations
run in the capture/HTTP path. Capture returns null/unavailable sensor evidence rather
than inventing measurements or rejecting the image. Unplugged devices are retried,
stale samples are excluded and future samples cannot be attached to older captures.
UART is dedicated UART2; USB debug, UART0 and camera wiring are retained.
Do not flash the SDR board or the existing front camera with this variant.

## Wiring on the REAR board
| Signal | GPIO |
|---|---:|
| Both VL53L5CX SDA |41|
| Both VL53L5CX SCL |42|
| Front-mounted ToF LPn |1|
| Rear-mounted ToF LPn |21|
| UART TX to front GPIO39 RX |38|
| UART RX from front GPIO38 TX |39|
Both ToFs and boards share ground; confirm the purchased carrier's supply/logic voltage.
Keep SD slots empty. INT is unused; I2C_RST follows the carrier's required pulldown.
The rear-camera pins differ from the abandoned plan that used38/39 for LPn.

## Build
python3 prepare.py --output /absolute/new/rear-build
pio run -d /absolute/new/rear-build -e esp32cam_s3_wroom_n16r8

First installation must include the partition table. Subsequent updates use espota
with the configured device hostname/IP and existing OTA password. Rebuild with
upload_protocol=espota or pass --upload-protocol espota. Serial commands do not
enable OTA or grant permission to reconfigure the front camera.

## UART
921600baud,8N1,3.3V; newline-delimited JSON:
{"schema":"rover.uart.v1","op":"time_sync","request_id":"unique","t1_primary_us":123}
Reply carries rear boot identity,t2_rear_us,t3_rear_us and the echoed request identity.
The primary must record t4, estimate offset/drift and retain uncertainty. These are
software-boundary timestamps, not hardware-edge precision. No primary UART client
is installed by this package. A missing client is reported disconnected.
ping/status return sensor status as well. Replies never grow an unbounded queue.
Camera packets include their original boot-clock capture timestamp even offline.

## SLAM
Mount geometry belongs in host configuration, not this firmware. Rear default is
same height,180degree yaw,0.0762m behind the FRONT CAMERA OPTICAL ORIGIN, provisional
until measured. Rear intrinsics must be independently calibrated.
