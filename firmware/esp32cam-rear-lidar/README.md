# Rear ESP32-S3-CAM with two VL53L5CX sensors

Current normal release: **1.0-rear.12**.

Derived from the sibling front-camera firmware package at its pinned upstream revision.
The rear variant is prepared from pinned camera sources and retains RTSP, authenticated
capture at /api/capture, provisioning, exposure/light controls and password-protected
ArduinoOTA on port3232. Two 6MiB OTA application slots are provided.

Camera frames are delivered directly over Wi-Fi regardless of UART or LiDAR state.
LiDAR acquisition, BNO and UART use dedicated workers; both LiDAR heads share the
acquisition worker. No sensor or UART operations run in the capture/HTTP path. Capture returns null/unavailable sensor evidence rather
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

## Bounded LiDAR recovery (1.0-rear.11)

This release repairs the pinned VL53L5CX driver's error paths. A response poll
terminates on its two-second elapsed deadline, transport error or sensor MCU
error. Initialization, configuration and ranging start return at the first failed
operation. Zero-byte and short I2C reads are errors; readiness and range parsing
do not reuse a failed read as fresh evidence.

The materializer checks the exact SparkFun VL53L5CX 1.0.3 source blobs and applies
`patch_lidar_library.py` only to the rear dependency. Both dependency pre-scripts
verify their reviewed patched bytes on subsequent PlatformIO builds. The BNO,
camera, LiDAR pins and rates, and radio/transport settings remain unchanged.

Both LiDAR heads still share one acquisition worker. A bounded recovery or sensor
firmware download can temporarily delay the other head; this is not a claim of
complete head independence or a measured fix for camera delivery. After a failed
attempt the adapter disables that head's communications. A retry probes its
retained/default address, then explicitly selects register bank zero before
checking identity and restarting initialization.

Run `python3 test_lidar_driver.py --prepared /absolute/new/rear-build` for the
actual-driver behavioral tests and exact unchanged-source negative controls.
Host tests use fake Wire/time boundaries; an installed release still needs live
camera and both-head qualification.

## Build
python3 prepare.py --output /absolute/new/rear-build
pio run -d /absolute/new/rear-build -e esp32cam_s3_wroom_n16r8

First installation must include the partition table. Subsequent updates use espota
with the configured device hostname/IP and existing OTA password. Rebuild with
upload_protocol=espota or pass --upload-protocol espota. Serial commands do not
enable OTA or grant permission to reconfigure the front camera.

For updates on a slow station link, run the verified uploader on the maintenance
Mac or Linux host with the enrolled IPv4 address, receiver ID, prepared normal build directory
and independently verified SHA-256 of the application:

`python3 upload_ota.py HOST RECEIVER_ID BUILD_DIR IMAGE_SHA256`

Supply the existing ArduinoOTA password in `REAR_OTA_PASSWORD`. For devices
that require HTTP authentication, also supply `REAR_CAMERA_USER` and
`REAR_CAMERA_PASSWORD` from the host enrollment. The uploader checks the physical
identity before sending and maintains one transfer while the TCP socket is backpressured.

Before inviting the board, it atomically saves a credential-free receipt containing the
expected image SHA-256 and release, receiver identity, and original boot and partition.
The default path is `BUILD_DIR/ota-receipts/RECEIVER_ID-IMAGE_SHA256.json`; use
`--receipt /absolute/new-attempt.json` to choose a new path. Transfer acceptance and
physical boot confirmation are separate states. A 90-second observation window that
expires leaves confirmation pending and exits with code 3, with a command to resume:

`python3 upload_ota.py --verify-only /absolute/saved-receipt.json`

This mode reads the original receipt and observes HTTP identity only. It sends no OTA
invitation or image, needs no `REAR_OTA_PASSWORD`, and can run after the original process
exits; a retained receipt also works after the build directory is removed. HTTP
credentials are still needed if configured.
Confirmation requires the expected receiver and release, a changed boot ID, and the
originally expected alternate partition. The image digest was checked before upload and
is retained in the receipt; the HTTP endpoint does not provide a running-image hash.

An existing receipt prevents an upload replay, including after an interrupted process.
A nonblocking process lock serializes upload and verification of the same receipt;
cancelling or exiting the process releases the lock automatically.
Resume observation with `--verify-only`; choose a new receipt only for an independently
reconciled new upload attempt. Exit code 0 means boot confirmation succeeded or the
requested release was already running before any upload. Every `--verify-only` run obtains
a fresh HTTP observation; the receipt retains the most recent positive boot witness
separately from the current verification status. The camera's authenticated capture
and live stream still require separate qualification.

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

Version 1.0-rear.7 adds a `wifi_driver` snapshot to rear telemetry. It reads actual
driver mode, power-save mode and AP FTM configuration; it never publishes an SSID
or password. Unknown values are null with the SDK read error, and snapshot sequence
and age distinguish current evidence from a deferred sample. Mode values are
0=off, 1=STA, 2=AP and 3=APSTA; power-save values are 0=none, 1=minimum modem
sleep and 2=maximum modem sleep.

The existing maintenance task conditionally restores actual power save to NONE
only while the station is connected. It clears an observed AP FTM flag while
preserving all other AP settings, and retires an AP only when its SSID exactly
matches this board's former generated ranging identity. A shared ownership mutex
serializes these corrections with the sole IotWebConf main-loop call. The main
loop never waits for the mutex and continues camera/OTA work when it is busy;
maintenance waits at most 20 ms for ownership, then defers without driver changes.
Paused or disconnected maintenance performs no corrective writes. A correction
counts as successful only after driver readback confirms it. These diagnostics
test for a state mismatch; no such mismatch is assumed to explain slow delivery.

## Experimental rear camera traffic priority
Version 1.0-rear.8 is a single-variable trial: the rear JPEG WebSocket requests
IP_TOS 0xa0 (CS5 / AC_VI), matching the existing sensor WebSocket. A rear-only
materialization patch applies this on each successful camera connection; the
shared front source and disconnect behavior remain unchanged.

The trial retains Wi-Fi ranging disabled, actual driver diagnostics, requested
15 dBm power, both LiDAR heads, 1280×720 / 200 ms capture and the one-second
TCP write-progress recovery policy. It does not claim that traffic priority
caused the prior stalls. Verify the actual outgoing TOS and repeat the same
fresh-frame and LiDAR qualification before accepting the trial. Keep the verified
1.0-rear.7 application available for rollback if there is no material benefit or
either stream regresses.

## Experimental rear write-progress budget
Version 1.0-rear.9 changes only the rear transport's no-progress budget from one
second to four seconds. The rear materializer patches `live_write_progress.h`;
the shared front source and its one-second default remain unchanged. Camera and
sensor sockets each keep their own progress tracking, bounded nonblocking writer
and independent task. Handshakes, heartbeat pings, pong replies and data fragments
continue through that same writer. Any successful byte write restarts its idle
budget; four seconds is not a total frame-delivery deadline.

This is an experiment to test whether allowing more time for TCP recovery reduces
write-timeout and reconnect churn. It does not establish a cause or a live-delivery
fix. Both LiDAR heads, disabled FTM, actual driver diagnostics, requested 15 dBm,
camera/sensor CS5 priority, 1280×720 / 200 ms capture and task priorities are
unchanged from the verified .8 source.

Run `python3 test_transport_budget.py` to exercise the production writer with
real backpressured host sockets and a deterministic scheduler clock. The same
three-second peer pause must fail under the unchanged front budget and recover
under the materialized rear budget. Regressions also check complete stalls,
progress restarting the idle budget, control frames, clean reconnects and
independent sensor writes while the camera is stalled. These host tests do not
simulate Wi-Fi or establish a real retransmission schedule.

Build and check the OTA image using the commands above, then repeat the unchanged
two-window viewer qualification after a separately verified new boot. Preserve
the verified .8 image for comparison or rollback. A longer wait may retain an old
in-flight frame longer; use measured capture-age bounds and actual delivery gaps
when deciding whether the experiment helps.

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

## Optional GY-BNO08x orientation module (1.0-rear.10)

The rear-only build includes the recovered BNO08x prototype's SPI/report support,
with an independent optional-sensor worker. It boots normally without a soldered
module: an unasserted, pulled-up INT yields `present:false`, `ready:false`,
`valid:false`, and `error:"not_detected"` after the detection window. A brief
`detecting` state occurs during each retry. It does not generate zero orientations
or reject camera images. Camera, both VL53L5CX heads, UART, LED, USB and the .9
transport configuration retain their assignments and settings.

### Wiring profile

This is the implemented signal profile; attached hardware is not yet qualified.

| GY-BNO08x label | SPI function | Rear GPIO / connection |
|---|---|---:|
| SCL | SCK |14|
| ADD / SA0 | MOSI |2|
| SDA | MISO |47|
| CS | Chip select |40|
| INT | Active-low attention |3|
| RST | Active-low reset |46|
| PS0 | SPI selection |3.3 V|
| PS1 | SPI selection |3.3 V|
| VCC | Supply |3.3 V|
| GND | Common ground |GND|

Confirm the actual carrier labels before soldering. This corrects the old draft's
INT45 assignment to INT3, avoiding the ESP32-S3 VDD_SPI voltage strap. GPIO3 is a
JTAG-selection strap; GPIO46 is also a boot strap. Normal flash boot with GPIO0
high permits either GPIO46 level, but ROM download with GPIO0 low requires GPIO46
low. The carrier's RST pull-up must not defeat that level during USB/serial
recovery; isolate that connection or hold the shared reset signal low for ROM
download. Native USB19/20 and UART0 recovery43/44 are not reassigned.

PS0/PS1 high follows the pinned SparkFun continuous-report SPI example; separate
WAKE/sleep control is not exposed by this six-signal profile. Keep BOOT in normal
application mode according to the actual carrier, and leave the SD slot empty.
The source pin map avoids camera15/4/5/6–13/16–18, LiDAR41/42/1/21, peer UART38/39,
LED48, and flash/PSRAM26–37. Module orientation has not been measured: telemetry
uses `sensor_body_unmounted`, not an invented rover/world alignment.

Primary references:
- [Pinned SPI example and mode straps](https://github.com/sparkfun/SparkFun_BNO08x_Arduino_Library/blob/v1.0.6/examples/SPI/Example_01_SPI_RotationVector/Example_01_SPI_RotationVector.ino)
- [ESP32-S3 GPIO capabilities](https://github.com/espressif/esp-idf/blob/v4.4.7/components/soc/esp32s3/include/soc/soc_caps.h)
- [ESP32-S3 boot strapping](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)
- [BNO08x datasheet, SPI and interrupt timing](https://docs.sparkfun.com/SparkFun_VR_IMU_Breakout_BNO086_QWIIC/assets/component_documentation/BNO080_085-Datasheet_v1.16.pdf)

### Failure isolation and data contract

A dedicated HSPI instance runs at 1 MHz. Only the idle-priority BNO worker touches
the sensor or the SH2 singleton. It detects INT after reset before opening SPI,
retries an unavailable device every 5 seconds, closes failed SH2 sessions, checks
every report-enable result, and reconfigures after sensor reset. A ready worker
checks for data every 2 ms; a missing/paused device uses a 20 ms background sleep.
These are sensor schedules and do not change the camera's 200 ms interval.

The pinned dependency contains unbounded missing-response paths. The materializer
resolves version 1.0.6, verifies original Git blob IDs, and applies narrow rear-only
patches: a 2-second fallback for operations with no upstream deadline, a 2-second
write-retry budget, negative SPI write failure, safe close after failed open, and
the missing HAL receipt timestamp. Existing nonzero upstream operation deadlines
are preserved. A PlatformIO pre script verifies the same patched bytes on later
builds, and rejects an unexpected or missing dependency rather than compiling it. Deadlines are checked between HAL calls: an in-flight INT wait can
extend an operation beyond its nominal budget. All such work stays outside
camera/HTTP/socket publication. No new sensor work begins while OTA is paused;
an already-started bounded operation may finish before the worker stops.

`GET /api/bno`, rear status, dedicated sensor ticks and camera metadata contain
`rover.bno085.v1` under `bno` (the dedicated endpoint returns that object itself).
Every report has its own sequence, host receipt timestamp, age and validity.
Rotation quaternion, acceleration in m/s², calibrated gyro in rad/s and magnetic
field in µT are requested every 50 ms. An event callback decodes every report in a
packet. A report more than 500 ms old, from after the requested camera timestamp,
from an unavailable/paused device, or containing non-finite values contributes
no numeric measurements. Missing another report type never refreshes its age.
After 2 seconds without recognized reports, the session closes and retries.

Authenticated `POST /api/bno` and existing sensor WebSocket messages with
`type:"bno"` accept `op`, optional `command_id` (up to 63 characters), and
`z_axis` for tare. Operations are `calibrate_all`, `calibrate_accel`,
`calibrate_gyro`, `calibrate_mag`, `end_calibration`, `save_calibration`,
`tare_now`, `save_tare`, and `clear_tare`. A four-entry nonblocking queue
serializes all commands with reads. HTTP 202 / `accepted:true,state:"queued"`
is acceptance only; telemetry `last_command` reports the latest command's actual
completion or failure and ID. Absent/paused sensors and full queues are rejected
immediately. Saving calibration explicitly requests the sensor's flash save;
ESP32 calibration payload backup/restore is not claimed or implemented.

Software/build tests and an OTA boot with no module verify absent-device behavior.
They cannot establish the carrier's wiring, physical sensor readings, heading
calibration, mounting alignment, or successful sensor-present operation.

## Rear sender timing diagnostics (1.0-rear.12)

The rear build opts into HTTP-only sender timing at authenticated
`GET /api/telemetry/stream`, under `camera_send_timing`. Normal JPEG metadata
and sensor messages do not contain these diagnostics and keep their existing
size/content. This instrumentation changes no capture interval, fragment delay,
write timeout, socket setting or task priority. Shared/front builds omit it by
default.

The snapshot contains the most recent **completed attempt**, its capture/start/
completion timestamps, completion age, outcome and cumulative counters for both
successful and failed attempts since boot. It is copied under a short lock;
network work and JSON serialization never hold that lock. A blocked in-flight
send does not replace the last completed sample, so check its age. A success
means the writer accepted the whole message, not that the Mac decoded it.

All time fields are elapsed **wall-clock microseconds**, including descheduling
inside the measured operation. The disjoint buckets are:

| Field | Included work |
|---|---|
| `loop_us` | Inherited WebSocket loop calls between fragments, including their control writes |
| `fragment_yield_us` | Actual elapsed time around the existing one-tick interfragment sleeps |
| `send_syscall_us` | Raw nonblocking data-send calls, including any descheduling during them |
| `send_wait_us` | Existing sleeps after data-write EAGAIN/EWOULDBLOCK/EINTR |
| `other_us` | Nonnegative remainder: framing/masking, checks, accounting and other unassigned time |
| `total_us` | Whole application-payload send attempt; equals the five disjoint buckets when valid |

An inherited heartbeat/pong/close write is included only in `loop_us`; its
bytes, syscall time and waits are not counted again as JPEG data. Data
`wire_bytes` counts bytes accepted by send, including WebSocket headers.
`payload_bytes` is the requested complete application message (metadata plus
JPEG); `completed_payload_bytes` counts only fully written fragments and can
understate partial progress on a failed fragment. Fragment, syscall, retry and
yield counts distinguish more calls from longer waits. An invalid accounting
sample is flagged and counted rather than reporting an underflowed remainder.

Run `python3 test_send_timing.py` for deterministic production-writer checks;
add `--prepared /absolute/rear-build` to verify exact materialized headers too.
The tests inject syscall/wakeup elapsed time and control/data failures to verify
accounting and unchanged wire/pacing decisions. They do not reproduce Wi-Fi,
CPU scheduling in the device, or prove which bucket explains live throughput.
