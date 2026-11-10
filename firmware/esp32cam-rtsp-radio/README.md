# ESP32-S3 CAM with RF and AHRS telemetry

This version targets the ESP32-S3 CAM N16R8 camera only. It is based on
[rzeldent/esp32cam-rtsp](https://github.com/rzeldent/esp32cam-rtsp), pinned to
`14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7`.
The original RTSP, HTTP camera stream, snapshots, camera settings, and OTA
services remain in the firmware.

Version `1.0-radio.5` uses GPIO41 (SDA) and GPIO42 (SCL) for the Stratux AHRS
v1.1 sensor. It includes the live `/telemetry` page, light controls, JSON API,
and optional UDP JSON output.
It observes 2.4 GHz Wi-Fi access points and Bluetooth LE advertisers. It does
not connect to the discovered devices.

## Build the version

Requirements: Python 3, Git, and PlatformIO. Validation uses PlatformIO 6.1.19,
Espressif32 7.0.1 / Arduino 2.0.17, NimBLE-Arduino 2.3.7, and ArduinoJson 6.21.5.

From the repository root:

```sh
python3 firmware/esp32cam-rtsp-radio/prepare.py \
  --output /tmp/esp32cam-radio --build
```

The only build target is `esp32cam_s3_wroom_n16r8`, matching the S3 CAM used here.

The preparation command verifies the package hashes, checks out the exact
upstream commit, applies `integration.patch`, and copies `overlay/`. It
requires an empty destination and never overwrites an existing project.
To rebuild the already prepared source, use:

```sh
pio run -d /tmp/esp32cam-radio -e esp32cam_s3_wroom_n16r8
```

The application binary is
`.pio/build/esp32cam_s3_wroom_n16r8/firmware.bin` inside the prepared project.
Use the normal PlatformIO upload procedure for this S3 target when installing. This change does not automatically flash or
reconfigure any connected camera.

## Connect the Stratux AHRS sensor

The supported v1.1 profile is MPU9250 + AK8963 + BMP280, identified at runtime.
It supplies acceleration, angular velocity, magnetic field, pressure,
temperature, and six-axis attitude with explicitly relative yaw.
Open `http://<camera-ip>/telemetry#ahrs` for live readings and a stationary
gyro-calibration button. The full API is `GET /api/ahrs`; AHRS also appears
in `GET /api/telemetry` and separate UDP packets (default 10 Hz).

Use GPIO41 for SDA and GPIO42 for SCL on the free I²C controller. The previous
`1.0-radio.4` build used GPIO21/47; reflash when moving to the new pair. Read
[AHRS.md](AHRS.md) for the full Stratux physical-header power/ground mapping,
3.3 V pull-ups, units/frames, API and UDP details, and post-flash test steps.

## See signal strength

After installation and connection to Wi-Fi, open:

```text
http://<camera-ip>/telemetry
```

The camera's normal status page includes a link. Radio routes use the same
authentication callback as the protected camera routes. Configure the existing
authentication username/password to protect them.

For each retained observation, the page/API exposes:

- Wi-Fi BSSID, SSID when present, channel, and advertised FTM responder support.
- BLE address and address type, advertised name when present, optional
  advertised TX power, and decoded iBeacon identifier/calibration information.
- Latest and smoothed RSSI in dBm, sample count, and the age of that device's
  most recent observation.
- Approximate RSSI range, its reference RSSI, path-loss exponent, calibration
  flag, and method.
- Receiver identity, boot identity, uptime, and synchronized timestamp when
  available. A missing synchronized clock is represented explicitly.

There are 32 retained Wi-Fi entries and 64 BLE entries. Entries expire after five minutes without an observation. Old entries are replaced
when capacity is needed, and replacement counts are exposed. Radio-driver
scan-result storage is managed by the SDK; these limits describe the retained
application tables.

Wi-Fi scans list access points, not every client transmitter. BLE scanning
lists advertising LE devices, not Classic Bluetooth connections. Private BLE
addresses can rotate and short scan windows can miss advertisements. A device
missing from one scan is not proof that it left the area.

## Send telemetry to a computer

Start the included standard-library receiver on the receiving computer:

```sh
python3 /tmp/esp32cam-radio/tools/telemetry_receiver.py
```

In the camera's telemetry settings, enter that computer's IPv4 address and
UDP port `9876`. An empty destination disables UDP output. The receiver must be
reachable from the camera, with inbound UDP permitted on that port.

To see a live terminal table:

```sh
python3 /tmp/esp32cam-radio/tools/telemetry_receiver.py --view
```

The default mode prints JSONL to standard output and creates no files.
Optional rotating text logs have an explicit storage limit:

```sh
python3 /tmp/esp32cam-radio/tools/telemetry_receiver.py \
  --log radio.jsonl --max-bytes 1048576 --backups 3
```

This keeps one active file and at most three backups, approximately 4 MiB in
total. Each received record retains its receive time, source address, and the
original firmware message. Multiple receivers are kept separate. Displayed
observation age advances after receipt.

UDP is best-effort delivery. There is no retained disk queue or retry backlog
on the camera. Each datagram is below 1200 bytes and contains one observation
or a summary. A sequence number and boot identity let a consumer distinguish
packet order and camera restarts. Summary packets include FTM and scan status.
Repeated publication of an old observation does not reset its sample age.

## Radio scheduling and camera coexistence

Defaults:

| Setting | Value |
| --- | --- |
| Wi-Fi scan period | 60 seconds |
| Passive Wi-Fi dwell | 120 ms per channel |
| BLE scan period | 10 seconds |
| BLE scan duration | 2 seconds |
| BLE scan window / interval | 30 ms / 300 ms |
| UDP publication period | 5 seconds |
| UDP destination | Empty; configure to enable |

Wi-Fi scans, BLE scans, and FTM sessions are serialized by a separate
low-priority worker. The streaming loop does not wait for a scan to finish. Bluetooth initialization
has its own single task so a host/controller sync stall does not stop Wi-Fi
scanning or UDP output.
Telemetry requests pause during OTA and resumes after a failed update. In-flight operations can briefly finish; pause is not an instantaneous quiescence barrier.

The radios still share a physical RF resource. These choices limit contention;
they do not establish a measured video latency or throughput guarantee. A live
camera/RTSP coexistence check remains part of hardware qualification.

Telemetry configuration is stored in a separate Preferences namespace, so the
upstream camera/Wi-Fi settings layout and configuration version are preserved.

## Range information

### RSSI estimate

```text
distance_m = 10 ** ((reference_rssi_1m_dbm - rssi_filtered_dbm)
                   / (10 * path_loss_exponent))
```

Defaults use an assumed reference of -45 dBm for Wi-Fi and -59 dBm for BLE
at one meter, with path-loss exponent 2.5. They are explicitly marked
`rssi_assumed`, not measured distances.

A decoded iBeacon advertisement can supply a signed one-meter reference.
A configured calibrated reference is an explicit user assumption and must fit
the transmitter being observed; one calibration does not make heterogeneous
transmitters equivalent. Even calibrated RSSI remains an approximate range
model. Advertised BLE TX power is shown separately and is never substituted
for a one-meter reference.

This receiver does not measure bearing or produce a unique 2D/3D position.

### Wi-Fi FTM

On a board and SDK with FTM initiator support, click **Measure** for an
access point advertising responder support. The API also accepts an explicit
known responder MAC and channel. The report contains result status,
round-trip time in nanoseconds, measured distance in meters, and report age.
A rejected, timed-out, unavailable, or failed measurement does not become a
fabricated distance. After cancellation the receiver drains that session's report
before accepting another FTM request, preventing stale-response misattribution.
If the SDK never delivers the cancellation report, status reports
`retirement_stalled`; a camera restart clears the uncertain session. Ordinary
Wi-Fi/BLE observation and UDP output continue.

The S3 SDK used here enables FTM. The responder must also implement FTM;
arbitrary phones or access points may not.

## Camera light controls

Firmware `1.0-radio.3` exposes the light controls directly in the device's
web portal. Open **Camera light and RF telemetry** from the camera's home
page, or go to `http://<camera-ip>/telemetry#light`.

To test after flashing:

1. Choose **White** (or use the color picker).
2. Set brightness with the slider or the numeric field, from 0 through 255.
3. Select **Turn on** and check the camera view or the LED itself.
4. Adjust the color/brightness and select **Apply color & brightness**.
5. Select **Turn off** when finished.

**White** only selects the color; **Turn on** sends it with the selected
brightness. **Apply color & brightness** preserves the current on/off state.
**Turn off** sends only the off command, so incomplete brightness edits cannot
prevent it. Brightness zero outputs black even if the on command is selected.

The panel reads state on page load, after each command, and when you select
**Reload light state**. It never changes the LED merely by loading the page.
Radio polling does not overwrite light edits. A command temporarily disables
the light controls until its response or an eight-second request timeout;
there is no automatic command retry. A timeout does not switch the LED off.

The panel displays driver/command status, including unknown output after a
driver failure. A successful command is not an optical measurement. Check the
physical LED/camera view to confirm the result. **Reload light state** also
loads changes made by another API client and replaces pending form edits.

### Light API

This S3 build adds authenticated `GET /api/light` and `POST /api/light`.
The selected light profile is one WS2812-compatible RGB pixel on GPIO48,
with GRB byte order. White is produced by setting all three color components
to the same value. Brightness is separate from the chosen color.

The FORIOT listing identifies ASIN B0GD2BZ9B1 as an N16R8/OV3660 S3 board.
The [seller's pinout image](https://m.media-amazon.com/images/I/81e8TEDdnsL._AC_SL1500_.jpg)
labels GPIO48 as WS2812 and GPIO4 as CAM_SIOD. Its board layout matches the
[OV3660 listing photo](https://m.media-amazon.com/images/I/71v0J4UcR1L._AC_SL1500_.jpg);
the pinout itself pictures an OV2640 camera variant. This supports the chosen
GPIO48 profile, while the fitted LED and routing on the user's physical PCB
remain untested. API driver readiness and a completed command are separate
from physical LED detection and measured illumination. The response explicitly
reports that physical verification/readback is absent.

### GPIO4 is a camera signal on this S3

The linked [GPIO4 flash tutorial](https://www.makerguides.com/control-esp32-cam-flash-led/)
uses the original **AI Thinker ESP32-CAM**. Its white-flash wiring and SD-card
GPIO4 conflict must not be copied to this S3 configuration. Here GPIO4 is
`CAMERA_CONFIG_PIN_SCCB_SDA`, the camera's control-data line; GPIO5 is its
control clock. The light module checks for camera-pin conflicts at compile
time and never uses GPIO4 as a light output.

The selected pixel is driven with RMT, leaving the camera's LEDC clock
timer/channel untouched. This is a color/brightness interface, not a claim
that the board has a high-power white flash. Illumination range is unmeasured.

### Requests

Use the camera's configured authentication credentials. If the existing
camera username is empty, the upstream authentication callback allows
unauthenticated requests, as it does for the other protected camera routes.

Read the current commanded state and driver status:

```sh
curl -u USER:PASSWORD http://CAMERA_IP/api/light
```

Turn on white light at brightness 128 out of 255:

```sh
curl -u USER:PASSWORD -H 'Content-Type: application/json' \
  --data '{"on":true,"brightness":128,"rgb":[255,255,255]}' \
  http://CAMERA_IP/api/light
```

Change brightness without changing the selected color or on/off command:

```json
{"brightness":64}
```

Turn off:

```sh
curl -u USER:PASSWORD -H 'Content-Type: application/json' \
  --data '{"on":false}' http://CAMERA_IP/api/light
```

The POST body is a nonempty partial object, at most 256 bytes, with these fields:

| Field | Accepted value |
| --- | --- |
| `on` | JSON boolean |
| `brightness` | Integer from 0 through 255 |
| `rgb` | Array of exactly three integers from 0 through 255, in red/green/blue order |

Unknown fields, malformed JSON, and invalid types/ranges are rejected before
a hardware command is issued. Mutations require `application/json` (HTTP415
otherwise); invalid updates return HTTP400, and a body over 256 bytes returns
HTTP413. A driver failure returns HTTP503 with its error stage and sets
`command_state_known` false. A 100ms transmission-fault deadline prevents a
stalled RMT transfer from holding the camera service indefinitely; it does not
limit how long the light stays on.
Changes to brightness/color while off remain remembered in RAM and output
black. Brightness zero also outputs black. The light initializes off; its
initial remembered color is white and brightness is 128. State is not
persisted across reboot. There is no automatic light-duration limit.

See the [FORIOT listing](https://www.amazon.com/dp/B0GD2BZ9B1),
[Keyestudio's matching S3 camera-family documentation](https://docs.keyestudio.com/projects/MB0184/en/latest/docs/MB0184%20ESP32-S3%20CAM%20Development%20Board.html),
and the [pinned S3 camera BSP GPIO definitions](https://github.com/larryli/esp-idf_esp32_s3_cam/blob/d677304d482a0aee9bc609cf7127aa2f6e0fe519/include/bsp/esp32_s3_cam.h).
These references support the selected profile without substituting for a
test on the actual FORIOT board.

## HTTP API

| Method and path | Purpose |
| --- | --- |
| `GET /api/light` | Light command state and driver status |
| `POST /api/light` | Set light on/off, brightness, and RGB color |
| `GET /telemetry` | Camera light controls, AHRS and live RF telemetry page |
| `GET /api/ahrs` | Sensor identities, cached measurements, attitude, and calibration status |
| `POST /api/ahrs/calibrate` | Queue stationary gyro-bias calibration |
| `GET /api/telemetry` | Current bounded snapshot and capabilities |
| `GET /api/telemetry/config` | Persistent radio configuration |
| `POST /api/telemetry/config` | Validate and apply partial JSON settings |
| `POST /api/telemetry/ftm` | Request an asynchronous FTM measurement |

Use `Content-Type: application/json` for mutations. Unknown/invalid settings
are rejected. Configuration examples:

```json
{"udp_host":"192.168.1.60","udp_port":9876,"publish_interval_ms":5000}
```

An explicit FTM request:

```json
{"address":"aa:bb:cc:dd:ee:ff","channel":1}
```

The MAC above illustrates the request shape; use a real responder's address.

The snapshot contains `schema`, `receiver_id`, `boot_id`,
`uptime_ms`, `timestamp_ms`, `paused`, `capabilities`, `config`,
`status`, `wifi`, `ble`, and `ftm`. UDP observations have
`type: "observation"`, a `radio`, and an `observation` object.
UDP summaries have `type: "summary"`.

## Validation

Run the receiver tests from the prepared project:

```sh
python3 -m unittest discover -s tests -p 'test_telemetry_receiver.py' -v
```

Run the pure C++ radio model tests:

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  test/radio_telemetry_model/test_model.cpp -o /tmp/radio-model-test
/tmp/radio-model-test
```

After the firmware build has installed ArduinoJson, run the light API model
tests from the prepared project:

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude \
  -I.pio/libdeps/esp32cam_s3_wroom_n16r8/ArduinoJson/src \
  test/camera_light_model/test_model.cpp -o /tmp/camera-light-model-test
/tmp/camera-light-model-test
```

The scoped GitHub workflow builds only the ESP32-S3 CAM N16R8 target and
preserves its firmware files as seven-day build artifacts. It also runs the
receiver, radio model, light API model, and AHRS model tests.

See `VALIDATION.md` for the exact verified results and remaining hardware
checks. Source/compile tests do not establish physical scan reception, FTM
accuracy, or uninterrupted live streaming.

## Provenance and license

The upstream project is GPL-3.0; its license is retained as `LICENSE`.
This firmware variant is distributed under GPL-3.0 with the upstream source
reference and patch. Third-party dependencies retain their own licenses.

Technical references:

- [Upstream firmware](https://github.com/rzeldent/esp32cam-rtsp/tree/14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7)
- [Espressif RF coexistence](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/coexist.html)
- [ESP-IDF 4.4.7 FTM example](https://github.com/espressif/esp-idf/tree/v4.4.7/examples/wifi/ftm)
- [Arduino 2.0.17 FTM initiator](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/examples/FTM/FTM_Initiator/FTM_Initiator.ino)
- [Bluetooth TX Power data definition](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/CSS_v11/out/en/supplement-to-the-bluetooth-core-specification/data-types-specification.html)
