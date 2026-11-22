# Rear ESP32-S3-CAM with two VL53L5CX sensors
Derived from the sibling front-camera firmware package at its pinned upstream revision.
The rear variant is prepared from pinned camera sources and retains RTSP, authenticated
capture at /api/capture, provisioning, exposure/light controls and password-protected
ArduinoOTA on port3232. Two 6MiB OTA application slots are provided.

Camera frames are delivered directly over Wi-Fi regardless of UART or LiDAR state.
Each optional peripheral has an independent worker; no sensor or UART operations
run in the capture/HTTP path. Capture returns null/unavailable sensor evidence rather
than inventing measurements or rejecting the image. Unplugged devices are retried,
stale samples are excluded and future samples cannot be attached to older captures.
UART is dedicated UART2. Debug logging uses the programming UART0, so startup
and network operation do not depend on a USB-OTG host; camera wiring is retained.
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

For updates on a slow station link, run the verified uploader on the maintenance
host with the enrolled IPv4 address, receiver ID, prepared normal build directory
and independently verified SHA-256 of the application:

`python3 upload_ota.py HOST RECEIVER_ID BUILD_DIR IMAGE_SHA256`

Supply the existing ArduinoOTA password in `REAR_OTA_PASSWORD`. For devices
that require HTTP authentication, also supply `REAR_CAMERA_USER` and
`REAR_CAMERA_PASSWORD` from the host enrollment. The uploader checks the physical
identity before sending, maintains one transfer while the TCP socket is backpressured,
and verifies the running release, alternate partition and new boot identity afterward.
The camera's authenticated capture and live stream still require separate qualification.

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


## Independent sensor WebSocket and disabled Wi-Fi ranging
The rear camera implements the front camera's authenticated outbound
WebSocket protocol: /sensors, Authorization: Sensor <token>, boot identity,
biome.sensor_tick.v1, clock exchanges and sequence acknowledgements.
A separate low-priority task emits rear LiDAR, UART health and ranging availability;
it never invokes the camera or front AHRS. Missing devices do not stop ticks.
A connected sensor sender publishes at its own cadence. A late or missing application acknowledgment does not pause publication or close the connection.
These ticks carry the latest valid ranges, not a retained history of every range.
Payload documents and send buffers use PSRAM. OTA pauses optional publication.

Configure persistently using authenticated POST /api/telemetry/stream:
{"host":"<native gateway LAN address>","port":9878,"token":"<rear-specific secret of at least 32 characters>"}
GET the same route for connection/worker/clock status (never returns the token).
The host's rear receiver must use matching receiver_id, port and token.
GET /api/sensors/capture returns the identical tick schema for diagnostics.
The front receiver remains on its existing port and identity.

Version 1.0-rear.6 disables Wi-Fi FTM initiation and response on this rear board.
No ranging worker, FTM event handler, or C3-TOF-ROVER-REAR ranging AP is started.
The normal Wi-Fi station and IotWebConf provisioning/recovery AP keep their existing
behavior. Both VL53L5CX LiDAR heads remain enabled at their existing settings;
camera capture remains 1280×720 at a configured 200 ms interval.

Authenticated POST /api/telemetry/ftm and POST /api/telemetry/config return HTTP501
with reason disabled_in_firmware. Saved rear-ranging preferences cannot re-enable
the feature. GET /api/telemetry/config and GET /api/ranging report immutable disabled
configuration. Rear telemetry explicitly advertises ftm, ftm_initiator and
ftm_responder capabilities as false, with null range, RTT, age and target fields.
A zero sample count represents unavailable ranging; it is never a zero-distance
measurement. The front-camera firmware is not changed by this rear-only adapter.

A separate small task preserves the existing requested maximum transmit power of
15 dBm (60 quarter-dBm SDK units). /api/telemetry reports the measured readback.
Actual transmit power remains subject to the SDK/radio's per-rate behavior.
The executable host regression uses the production adapter and the prepared build's
ArduinoJson dependency:

`python3 test_ranging_disabled.py --prepared /absolute/new/rear-build`

The rear firmware CI runs this check after the normal firmware build.

## One-time USB Wi-Fi provisioning
For a board without saved camera settings, prepare a private factory image:
`python3 prepare.py --output /absolute/new/factory-build --provisioning-json /private/wifi.json --build`
The input JSON supplies ssid and password. This initializes IotWebConf's network
and AP settings, saves them, and reboots once. The generated credential header
is owner-readable; treat the factory binary as private and never publish it.
Flash that image by USB, then upload the normal build over Wi-Fi using espota.
Normal OTA images omit the factory seed and retain the saved network settings.
Version 1.0-rear.4 preserves the Wi-Fi driver profile on an authenticated
camera restart. It allows ten seconds of inactivity per OTA receive attempt so
brief wireless stalls can recover before the receiver gives up.
It also migrates the rear camera's saved capture profile once to 1280×720 at
200 ms per frame, matching the deployed front camera, and retains those defaults
for new provisioning. Later camera settings changes remain configurable.
The rear sensor tasks have priority over the video task. Video yields between
fragments, and a socket with no TCP write progress is closed independently so its
next session can resume with the newest sample. The stream status reports the
configured capture interval and the dimensions of the last captured JPEG.
Version 1.0-rear.5 uses the same nonblocking, progress-bounded writer for
WebSocket handshakes, heartbeat pings, pong replies and data fragments. Its
stream status distinguishes camera connection/readiness, capture attempts and
duration, completed frames, and per-socket write stalls/progress. A host
regression uses real backpressured sockets and inherited control-frame dispatch
to verify that a stalled video writer closes only that connection while the
sensor connection remains usable.

GET /api/rear reports the actual running partition, firmware version, flash/PSRAM
sizes and whether an OTA target partition exists. Verify a real network update
and a post-update camera capture before relying on OTA.

The rear build routes application logging to UART0 (USB CDC on boot disabled).
This avoids a disconnected USB-OTG debug channel stalling first-run setup.

## Verify delivery at the viewer

On Python 3.11 or newer, install `aiohttp` and `Pillow`. Run the following
against the same vision WebSocket used by the viewer. If the endpoint requires
a bearer credential, supply its full Authorization value in
`ROVER_VISION_AUTHORIZATION` through the existing credential mechanism.

```sh
python3 firmware/esp32cam-rear-lidar/qualify_viewer.py "$ROVER_VIEWER_VISION_URL"
```

Compression remains disabled by default (`--compression 0`). To match a viewer
that requests WebSocket `permessage-deflate`, select it explicitly:

```sh
python3 firmware/esp32cam-rear-lidar/qualify_viewer.py "$ROVER_VIEWER_VISION_URL" --compression 15
```

Each attachment reports `compression_requested` and `compression_negotiated`.
The server can decline compression: the negotiated value is then `0`, or `null`
if the WebSocket handshake did not complete. Keep these fields with the firmware
identity when comparing runs. Selecting compression changes the transport only;
all qualification thresholds and sample-identity checks remain the same.

The command opens two successive 60-second connections and reports a sample count,
rate, and longest delivery gap for each camera and each LiDAR head. It decodes
each JPEG and verifies 1280×720 pixels. Each camera must deliver 5 ±0.5 fps,
and their rates must agree within 0.5 fps. Each LiDAR head must advance from
dedicated rear sensor ticks with both 64-cell arrays present. A gap over one
second, a premature socket close, or an invalid image fails the run. The gap
includes startup and the end of the observation. Replayed samples are counted once
across both connections. Use `--help` for other front profiles or test durations.
CI exercises the checker with loopback fixtures. A deployed release needs its
own run on the viewer connection.
