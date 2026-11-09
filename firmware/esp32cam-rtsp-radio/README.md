# ESP32CAM-RTSP with Wi-Fi and Bluetooth telemetry

This is a reproducible radio telemetry variant of
[rzeldent/esp32cam-rtsp](https://github.com/rzeldent/esp32cam-rtsp), pinned to
`14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7`.
The original RTSP, HTTP camera stream, snapshots, camera settings, and OTA
services remain in the firmware.

The variant adds a live `/telemetry` page, JSON API, and optional UDP JSON output.
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

The default board is `esp32cam_s3_wroom_n16r8`. Choose the environment that
matches the physical board, for example:

```sh
python3 firmware/esp32cam-rtsp-radio/prepare.py \
  --output /tmp/esp32cam-radio-ai-thinker \
  --build -e esp32cam_ai_thinker
```

The preparation command verifies the package hashes, checks out the exact
upstream commit, applies `integration.patch`, and copies `overlay/`. It
requires an empty destination and never overwrites an existing project.
To rebuild the already prepared source, use:

```sh
pio run -d /tmp/esp32cam-radio -e esp32cam_s3_wroom_n16r8
```

The application binary is
`.pio/build/<environment>/firmware.bin` inside the prepared project.
Use the normal upstream PlatformIO upload procedure and the correct board
environment when installing. This change does not automatically flash or
reconfigure any connected camera.

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

The S3 SDK used here enables FTM. Classic ESP32 reports FTM unsupported.
ESP32-S2 supports Wi-Fi/FTM but has no BLE radio. The responder must also
implement FTM; arbitrary phones or access points may not.

## HTTP API

| Method and path | Purpose |
| --- | --- |
| `GET /telemetry` | Live page |
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

The scoped GitHub workflow builds S3 N16R8, AI Thinker classic ESP32, and
ESP32-S2, and preserves each board's firmware files as seven-day build
artifacts. It also runs the receiver and radio model tests.

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
