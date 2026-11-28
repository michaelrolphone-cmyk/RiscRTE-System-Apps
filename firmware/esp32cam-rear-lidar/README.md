# Rear ESP32-S3-CAM with two VL53L5CX sensors

Current normal release: **1.0-rear.17**.

## Allocation recovery and reset reason (1.0-rear.17)

Each LiDAR head now retries its missing configuration/results allocation
independently while retaining an object that was already allocated. The prior
paired guard could leave results permanently unallocated, or overwrite and
leak a successful results object after the configuration allocation failed.
This defect was present in the retained .10, .11 and .16 adapter; the .11
low-level driver fixes did not cover these adapter allocations.

Authenticated `GET /api/rear` adds `reset_reason_code` (the ESP-IDF enum value) and
`reset_reason` (a stable label such as `panic`, `task_watchdog` or `brownout`).
The value is captured once at startup. It describes only that boot and cannot
recover the cause of earlier resets. Other status aliases and normal
camera/sensor payloads receive no new fields.

Actual-adapter fault tests reproduce both original allocation defects and
verify recovery, retained pointer ownership, the other head, full grids,
authentication and cached route-only reset fields. This is not evidence that
OOM caused a live reset or that delivery performance has recovered. Both
LiDAR heads, optional BNO absence handling, camera cadence, transport settings
and the prior fixes remain in place.

## Latest measured status — 2026-11-23 11:58 UTC

**The incident remains open; the unchanged delivery gates failed.**
[Tracked incident](https://tasker-workboard.cantikash.chatgpt.site/?task=7b8aa6d4-bc05-49c0-9b35-359323fa1c53).

The measured board was **1.0-rear.16**, app0, boot `51ceb200aeaf372d`.
The .17 allocation/reset-diagnostic candidate above had not been installed.
Strict run `rearperf-v16-lidar-db-isolation-public-native-20261123-70`
ran the two 60-second public/native windows at **11:56:35.764895–11:58:35.864952 UTC**.
Native camera rates were:

| Metric | First 60 seconds | Reconnect 60 seconds |
|---|---:|---:|
| Front camera, fps | 4.950 | 3.147 |
| Rear camera, fps | 2.483 | 1.748 |

A common native LiDAR delivery gap reached **13.297 s**. The outside-core
counter bracket recorded **695 captures, 375 completed sends and 320
superseded camera frames**; mean send wall time was about **366 ms**, with
about **73%** in backpressure wait. Those cumulative-counter deltas cover a
different span from the two viewer windows. No new transport errors were recorded; boot, map and
calibration identities were unchanged in the run. Expected absent-BNO behavior passed all
four route/window checks.

The receiver database-lock change was present in this run, but these results
do not prove it caused or resolved a particular live gap. The conditional
allocation defect likewise has no observed live OOM trigger or reset
attribution. Delivery, localization and unexplained resets remain separate
open questions.

Run 70 ended with exit 1 and a quiescent managed receipt; passive capture
ended at 11:58:50.515356 UTC with no additional live run implied. Full stdout
SHA256: `2e26864e6fb95ca05d6fb283b5c8c8dc12626f4ffb7a5499973c7b34ba54a485`.

## Historical measured status — 2026-11-23 08:38 UTC

**The incident remains open; the unchanged delivery gates failed.**
[Tracked incident](https://tasker-workboard.cantikash.chatgpt.site/?task=7b8aa6d4-bc05-49c0-9b35-359323fa1c53).

The rear is restored to **1.0-rear.14**, app1, boot
`a0c71ed56e7f48d6`. Its preserved 1,608,960-byte image has SHA256
`e0bdc24af5113873f56e27e09cc90d2f878a65c4648ed7377151ddd718b594d1`.
This retains the .11 LiDAR driver fixes, .13 WebSocket timeout-unit fix,
sender timing diagnostics, BNO absence handling and other proven fixes.
The .15 static-buffer selection was rolled back; its failed comparison is
retained below.

The strict public/native two-window run covered
**08:05:43.507197–08:07:43.532840 UTC**. Native results were:

| Metric | First 60 seconds | Reconnect 60 seconds |
|---|---:|---:|
| Front camera, fps | 4.967 | 4.933 |
| Rear camera, fps | 2.767 | 2.767 |
| Fresh LiDAR scans, front/rear heads | 291 / 289 | 295 / 292 |
| Maximum LiDAR gaps, front/rear heads | 2.068 / 2.048 s | 1.204 / 1.182 s |

All four route/window gates failed. Runtime, map and calibration identities
were unchanged within this run; the map differed from earlier runs, so
cross-run comparisons are not controlled capacity tests.

The outside-core device-counter bracket showed camera capture at
**4.9958 Hz** and completed sends at **2.9100 Hz**.
**72.3163%** of send wall time was backpressure wait; successful-fragment
yield time was zero. No new reconnects or write-progress timeouts occurred.
These bracket rates use a different time span from the strict viewer windows.

Saved source/wire correlation separates acquisition from delivery. During
the 2.068-second native front-head gap, the next head observation advanced
by **186.968 ms** (sequence 3007→3008), while its sensor message took
**1.908725 s** from first to last captured TCP byte. Native acceptance
followed byte coverage by 71.058 ms. This does not identify every delay as
radio propagation or CPU work. For 385 rear JPEGs without an observed
TCP-hole overlap, median first-to-last-byte assembly was **304.950 ms**;
2 of 387 complete JPEGs overlapped observed holes. All 387 had matching
application ACKs. Slow baseline assembly therefore remains relevant
alongside the occasional sequence-hole recovery.

A separate concurrent **BE-only** ICMP control at
08:02:03.045–08:02:19.973 UTC received 30/30 replies per target
(request TOS 0, 56-byte payload, 0.5-second interval):

| Target | Minimum / average / maximum RTT, ms |
|---|---:|
| Front | 4.811 / 13.430 / 45.600 |
| Rear | 14.345 / 203.186 / 1022.512 |
| Router | 1.643 / 6.473 / 38.976 |

Exact cached MAC associations guarded the camera targets. That earlier
BE-only control did not capture reply TOS or include a CS5 comparison.
BE round trips are not a class-neutral latency measure of the CS5-marked
rear streams; this contrast does not establish the responsible queue,
radio condition or per-device capacity.

The guarded front API point snapshot at 08:13:03.674–03.781 UTC identified
`esp32cam-98574b858428`, boot `6bfc8aa84df8ec65`. Its status reported a last sensor
packet of **6,455 bytes**, latest preparation of 14 ms and last send of 16 ms
(RSSI −55 dBm, channel 3), versus the rear's 4,458-byte point sample.
The front socket sampling period was not exposed, so no traffic rate is
inferred from this snapshot. The named configuration values
`publish_interval_ms=5000` and `ahrs_publish_interval_ms=100` are not
relabeled as a socket period. Nested `ftm_responder.firmware` reported
`1.0-radio.16-capture-imu`; this is not flashed-image attestation.

### Captured BE versus CS5 comparison (08:35 UTC)

One simultaneous six-case run used the front, rear and router with requested
TOS 0/160, 30 pings per case, 56-byte payloads and 0.5-second intervals.
Traffic began at 08:35:29.054344 UTC; cleanup completed in 16.920 seconds,
within the 25-second limit.
All six ping processes reported **30 sent / 30 received**. The header-only
capture recorded 352 packets with zero kernel drops, but retained only
**sequences 0–28: 29 matched study pairs per case**. Final sequence 29 was
absent for every case. Those uncaptured pairs are not counted as network loss.

The original strict diagnostic returned **exit 1**; it was not rerun.
Saved-file reanalysis resolved printed versus wire identifier byte order
(for example 47585 becomes 57785 after byte swapping). Two additional router
BE requests with wire ID 2935 were unrelated to the six study processes and
were excluded. Every captured study request and reply had the intended TOS,
including CS5 sequence 0. The failed full 30-packet capture/association gate
remains recorded.

Mac kernel-header RTTs for the 29-pair captured subsets:

| Target | Observed class | Minimum / mean / median / maximum RTT, ms |
|---|---|---:|
| Front | BE, TOS 0 | 7.098 / 25.266 / 15.626 / 132.354 |
| Front | CS5, TOS 160 | 6.268 / 15.383 / 12.776 / 46.602 |
| Rear | BE, TOS 0 | 32.055 / 227.863 / 96.840 / 1205.351 |
| Rear | CS5, TOS 160 | 11.766 / 63.765 / 55.550 / 240.861 |
| Router | BE, TOS 0 | 2.070 / 15.231 / 7.084 / 120.046 |
| Router | CS5, TOS 160 | 1.399 / 8.145 / 6.133 / 41.862 |

**Class sensitivity was observed; the rear remained slower under matched
CS5.** RTT includes both directions and possible Mac/AP/device scheduling.
This does not identify a unique queue or firmware cause, prove that the
camera-priority change helps or harms throughput, or pass the delivery gate.
The original report and headers remain unchanged. Requested spacing was
0.5 seconds; observed mean request spacing was approximately 565 ms.

Managed evidence (stdout SHA256 unless marked report):

- `rearperf-v14-rollback-public-native-20261123-46`:
  `985dfcc76aabb37c7c758144e13b09c0f2cd9b4330106958608d43cf27637ab2`.
- `rearperf-v14-rollback-icmp-control-20261123-47`:
  `eec8aebda632983b1c39f2a0755c6ba4b2bad2eca1da1150e1d65ede4366bfe3`;
  report `e07f5bce772a5097425c70847665df1e1fefdbe05e7876a57924394c6fe636b0`.
- `rearperf-v14-rollback-saved-wire-head-correlation-20261123-48`:
  `c691be5fea20c7bffeee3fc2ac2afbe1659e379be1cbb8bb8af93b9d80b9fbfc`;
  report `567ab859b47a2a5d17ab57facc220ac990d328f4735b8393ec281728c38c51ca`.
- `front-load-snapshot-20261123-review-01`:
  `1493cf16f7801081782ac8e3fc5c0b28456019d900573a3c67eebe94c1e82ffe`.

- `rear-icmp-class-comparison-20261123-live-01`:
  stdout `064dd768a185d251487b8d9d3a24a153b61b5c2a2204920c4a0cb807426b962b`;
  original report `0bcb6d51f29bf5374e1206ba23cb0440290c84c5f0f4b0d8fcb6f88631f097f0`;
  header-only records `ce3c63cc1fd50105b81c43d4ca9b18f3b280e745938d330bd6b3eae65a728309`.
- `rear-icmp-class-20261123-saved-reanalysis-01`:
  stdout `ee3bf1e2c4a6b63fc52e190bf9037dc94fad3ff8bd133c89ac9b623d263f618a`;
  report `f2de9997da68a743221ce1315aac797985c646a42795bf532b43816227c4d152`.

## Firmware overview

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

## Bounded TCP diagnostic (1.0-rear.16)

This release adds observation only. The authenticated
GET `/api/telemetry/tcp` returns separate cached camera/sensor TCP
snapshots, bound to receiver ID, boot ID, firmware and the current `esp_timer`
timestamp. It does not enqueue work from HTTP or enlarge normal JPEG/sensor
messages. Capture, sensor rates, both LiDAR heads, BNO absence handling, CS5,
four-second write recovery, fragment pacing and Wi-Fi settings remain unchanged.

Two global contexts each allocate one persistent callback message on their first
established WebSocket connection, after lwIP initialization. Allocation failure
disables that diagnostic without blocking streaming. The socket owner caches
the exact IPv4 tuple once on connection; EAGAIN performs no allocation or socket
getter. Attempts, including rejected queue posts, are at least 250 ms apart.
One immutable request/result slot stays in callback custody until the owner
consumes it. Reconnect changes the current generation without clearing pending
custody; results from older generations are discarded. No PCB pointer is kept.

Only the tcpip-thread callback reads PCBs. It walks at most 32 active PCBs and
16 total unacked/unsent segments, with explicit not-found/truncated/not-visited
flags. It copies scalar send-buffer, congestion/window, ACK/sequence, queue,
timer and retry state. The HTTP cache has an independent short lock; the
callback performs no allocation, socket call, JSON, logging or application lock.

`source_snapshot_us`/`source_complete_us` bound the callback's observation.
Post-attempt-to-callback wall time includes submission and scheduling, not only
mailbox residence. Pending includes a completed callback awaiting owner
consumption. Age, generation and a declared one-second stale threshold prevent
old samples from appearing current. The trigger says whether the request followed
EAGAIN. Aggregates describe sampled observations, not continuously monitored
occupancy or an event history.

`tcp_slow_tick_ms=500` applies to raw `rto_ticks`, `rtime_ticks`
and `rttest_ticks`. `sa_raw` and `sv_raw` are scaled estimator
accumulators; they are not milliseconds. `nrtx` is current retry state
that resets on advancing ACK, not a lifetime retransmission count. Segment byte
sums cover visited payload only and may be truncated.

`check_tcp_diagnostic_sdk.py` verifies the actual Arduino 2.0.17
ESP32-S3 qio_opi configuration, four lwIP headers and liblwip.a before builds.
Shared/front hooks are disabled by default. Run
`python3 test_tcp_diagnostic.py --prepared /absolute/new/rear-build`
for delayed/reentrant callback, reconnect, queue-full, traversal-limit and
writer-boundary tests plus changed-SDK rejection. Host boundaries are simulated;
the target build binds the real ABI. This diagnostic is not a performance fix
or evidence that the delivery gates pass.

## Build
python3 prepare.py --output /absolute/new/rear-build
pio run -d /absolute/new/rear-build -e esp32cam_s3_wroom_n16r8

The serial installation must include the two-slot partition table if the board
does not already have it. On an existing rear camera, preserve its settings and
partition table. Build a private serial image once to enroll a per-device key:

```sh
umask 077
openssl rand -hex 32 > /private/path/rear-ota.key
python3 prepare.py --output /absolute/new/serial-build --ota-key-file /private/path/rear-ota.key --build
```

Keep the key and serial image on the maintenance host. The normal build has no
embedded key and reads the existing enrollment from NVS. If first installing
Wi-Fi credentials as well, use the same command with `--provisioning-json`.

For subsequent network updates, use the normal build and its checked image hash:

```sh
python3 upload_ota.py HOST RECEIVER_ID BUILD_DIR IMAGE_SHA256 --recovery-key-file /private/path/rear-ota.key
```

The uploader sends an authenticated reboot request to the independent maintenance
service on UDP or TCP 3233. The recovery boot uses the saved Wi-Fi credentials
and skips camera, RTSP, LiDAR, IMU, and streaming startup.

Rear.23 and later transfer the image in authenticated blocks over UDP 3233.
The receiver acknowledges committed blocks and reports which blocks it has
buffered. The uploader retries only missing blocks. The camera checks the whole
image SHA-256 before selecting it as the next boot slot. Five minutes without
valid update traffic returns the camera to normal operation on its current slot.
The uploader retains ArduinoOTA on UDP 3232 for earlier recovery builds and
requires the TCP callback for those builds.

For legacy rear firmware without an enrolled recovery key, omit
`--recovery-key-file` and supply its ArduinoOTA password in `REAR_OTA_PASSWORD`.
If that firmware requires HTTP authentication, use `REAR_CAMERA_USER` and
`REAR_CAMERA_PASSWORD` from the existing host enrollment.

Before inviting the board, it atomically saves a credential-free receipt containing the
expected image SHA-256 and release, receiver identity, and original boot and partition.
The default path is `BUILD_DIR/ota-receipts/RECEIVER_ID-IMAGE_SHA256.json`; use
`--receipt /absolute/new-attempt.json` to choose a new path. Transfer acceptance and
physical boot confirmation are separate states. A 90-second observation window that
expires leaves confirmation pending and exits with code 3, with a command to resume:

`python3 upload_ota.py --verify-only /absolute/saved-receipt.json`

This mode reads the original receipt and observes device identity. It sends no OTA
invitation or image, needs no `REAR_OTA_PASSWORD`, and can run after the original process
exits; a retained receipt also works after the build directory is removed. HTTP
credentials are still needed if configured.
Confirmation requires the expected receiver and release, a changed boot ID, and the
originally expected alternate partition. The image digest was checked before upload and
is retained in the receipt; the status endpoint does not provide a running-image hash.

Starting with `1.0-rear.28-qos-bno-health`, a pending OTA slot with a bootable
fallback gets 75 seconds to qualify. The camera keeps the slot only after four
consecutive five-second samples with Wi-Fi connected, advancing receiver camera
acknowledgements, and advancing sensor sends. A stalled trial restores the previous
slot automatically. The updater waits for the reported `ota_trial: accepted`
before confirming these releases. A first installation without a previous slot
reports `unprotected` and has no automatic rollback.

An existing receipt prevents an upload replay, including after an interrupted process.
A receiver lock under `~/.local/state/rear-camera-ota/` allows one uploader for
that camera across build directories and receipt paths on the maintenance account.
Verification uses a separate receipt lock. The OS releases both locks on exit.
If the camera returns to its original slot on a new normal boot while entering
recovery, the uploader stops. Resume observation with the existing receipt.
Resume observation with `--verify-only`; choose a new receipt only for an independently
reconciled new upload attempt. Exit code 0 means boot confirmation succeeded or the
requested release was already running in normal mode. Trial-capable releases
must explicitly report `ota_trial: accepted` in both cases. Every `--verify-only` run obtains
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
Version 1.0-rear.8 was a single-variable trial: the rear JPEG WebSocket requests
IP_TOS 0xa0 (CS5 / AC_VI), matching the existing sensor WebSocket. A rear-only
materialization patch applies this on each successful camera connection; the
shared front source and disconnect behavior remain unchanged.

The trial retained Wi-Fi ranging disabled, actual driver diagnostics, requested
15 dBm power, both LiDAR heads, 1280×720 / 200 ms capture and the one-second
TCP write-progress recovery policy. It does not claim that traffic priority
caused the prior stalls. Verify the actual outgoing TOS and repeat the same
fresh-frame and LiDAR qualification before accepting the trial. Keep the verified
1.0-rear.7 application available for rollback if there is no material benefit or
either stream regresses.

The outgoing CS5 treatment was subsequently verified on the wire. Raw native
rear JPEG counts improved in later .8 observations, but all four aggregate
health gates still failed; host and observer state changes confounded a causal
comparison. The priority change was neither an accepted causal improvement
nor a proved regression. See the [PR272 validation record](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/pull/272).

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
For an isolated OTA recovery installation, use the same private provisioning
build to enroll the recovery key with `--ota-key-file`.
Version 1.0-rear.4 preserves the Wi-Fi driver profile on an authenticated
camera restart. It allows ten seconds of inactivity per OTA receive attempt so
brief wireless stalls can recover before the receiver gives up.
It also migrates the rear camera's saved capture profile once to 1280×720 at
200 ms per frame, matching the deployed front camera, and retains those defaults
for new provisioning. Later camera settings changes remain configurable.
The rear sensor tasks have priority over the video task. Through version .13,
video also slept one tick between successfully written fragments; version .14
removes that extra pacing as described below. A socket with no TCP write progress
is closed independently so its next session can resume with the newest sample. The stream status reports the
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

The `1.0-rear.28-qos-bno` startup repair keeps the existing harness. The worker
owns the reset pulse and initial INT check. SH-2 reads each SPI header and
payload with CS held low and bounds requests that receive no response.

The duplex revision also retains packets clocked in while sending an SH-2
command. The authorized `GET /api/bno` includes `spi_startup` fields for SH-2
open, product-ID exchange, INT wait, and receive activity. The product-ID
status is `-9999` if that request did not run.

In `1.0-rear.29-bno-start` the clock enters mode 3 while reset is asserted.
After reset the worker checks INT every millisecond during detection. The same
SPI diagnostics travel in the sensor stream. A GET on `/api/bno` returns the
firmware version and maintenance boot ID alongside the measurements. An
enrolled unit also accepts a read-only UDP probe on port 3233 while in normal
mode. Send `rear-bno-probe-v1 ` followed by the lowercase hex HMAC-SHA256 of
`rear-bno-probe-v1:<receiver_id>:<boot_id>` using the enrolled key. The reply
identifies the boot and returns counters, product number, and a timestamped
quaternion. Its `spi` array follows the field order in `spi_startup`, ending
with the first header as a little-endian word and the first payload byte.

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
the sensor or the SH2 singleton. It detects INT after reset before opening SH2,
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
| `fragment_yield_us` | Actual elapsed time around optional one-tick interfragment sleeps; zero when disabled in .14 |
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

## Rear WebSocket read-timeout unit correction (1.0-rear.13)

The rear materializer applies `patch_websocket_library.py` to the local pinned
WebSockets 2.7.2 dependency. Its connection callback previously supplied the
intended 5000-millisecond timeout to ESP32 Arduino 2.0.17's
`WiFiClient::setTimeout(seconds)`. That set the Stream read timeout to
5,000,000 ms, allowing a partial HTTP upgrade line to hold a sender worker for
about 83 minutes before its idle timeout. The outer five-second handshake check
cannot execute while that blocking line read is in progress.

The correction supplies seconds to the socket/client API and explicitly supplies
milliseconds to `Stream::setTimeout`, before any HTTP response line is read.
It preserves the intended 5000 ms idle deadline, existing TCP connect/write
budgets, heartbeat settings and all capture/sensor rates. Exact WebSockets and
ESP32 framework source blobs guard the API assumption. Only the prepared rear
project's dependency is changed; shared/front dependency copies are untouched.
The PlatformIO pre script verifies the same transformation on later builds.

This correction is **not** an incremental or nonblocking reader. A partial HTTP
line can still occupy its sender until the intended idle deadline, and the
existing established-frame reader retains its own five-second no-progress
wait. Byte progress renews the original idle behavior. No live outage is
attributed to this defect without corresponding receive-state evidence.

Run `python3 test_websocket_timeout.py --prepared /absolute/rear-build`.
An optional `--framework /absolute/framework-arduinoespressif32` selects a
non-default framework installation. The harness compiles verbatim, checksum-
verified production `connectedCb`, `handleClientData`, `WiFiClient::setTimeout`
and Stream read methods with fake clock/socket boundaries. The same missing-
newline test must observe 5,000,000 ms and fail on unchanged code, then observe
5000 ms and pass after the patch. Complete and in-budget fragmented lines,
existing idle-renewal behavior, patch idempotence and drift rejection are checked.
This does not simulate Wi-Fi, TLS, or the full established-frame parser.

## Rear JPEG fragment pacing experiment (1.0-rear.14)

This release changes one rear-only behavior:
`CAMERA_SOCKET_YIELD_BETWEEN_FRAGMENTS` is explicitly false. After a successful
fragment write, the camera sender continues to its next fragment without the
extra one-tick sleep. The existing bounded writer still sleeps on
EAGAIN/EWOULDBLOCK/EINTR, retains its four-second no-progress limit, and services
the inherited WebSocket loop between fragments. The outer worker's two-ms
background sleep is unchanged.

The documented front-camera producer also has no successful-fragment sleep.
In the current rear SDK, TCP_MSS is 1436 bytes; a full fragment uses 1422 payload
bytes plus its actual eight-byte masked WebSocket header, or 1430 wire bytes.
The pinned WebSocket connection setup requests TCP_NODELAY, and the sender
uses MSG_DONTWAIT without MSG_MORE. Spacing these promptly eligible writes can
change downstream queue occupancy, aggregation opportunities or ACK timing.
The direct sleep duration alone cannot measure those indirect effects.

The .13 HTTP timing sample recorded about 79% of send wall time in backpressure
waits, 16% in send calls, and 2.3% in explicit fragment sleeps, while captures
continued at about five per second. Those observations motivate this comparison;
they do not establish that fragment pacing caused the slow delivery or that
disabling it improves performance. Actual Wi-Fi aggregation was not measured.

Camera and sensor IP_TOS 0xa0 (CS5 / AC_VI) remain unchanged. AC_VI supports
AMPDU according to the pinned Espressif guidance; this experiment does not
attribute an aggregation disable to CS5. Camera size/200-ms capture interval,
sensor cadence and priority, BNO absence handling, both LiDAR heads, disabled
FTM, requested transmit power, read-timeout correction and recovery behavior
remain unchanged. Shared/front source is not modified.

Run the existing `test_transport_budget.py`,
`test_send_timing.py --prepared /absolute/rear-build`, and
`test_websocket_timeout.py --prepared /absolute/rear-build` checks, then build
and check the OTA image. Compare actual .14 sender timing, bytes and throughput
with the retained .13 evidence, and repeat the unchanged viewer/LiDAR acceptance
thresholds after a separately verified boot. A passing build is not a delivery
pass; retain the .13 image for comparison or rollback.

Reference:
[ESP-IDF 4.4.7 Wi-Fi QoS](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html#wifi-qos).

## Rear Wi-Fi static-buffer comparison (1.0-rear.15, reverted)

The .15 trial's only runtime change from .14 was `WiFi.useStaticBuffers(true)` as the first
statement in the rear materialized `setup()`, before camera setup,
`WiFi.setSleep(false)`, IotWebConf initialization, or any telemetry worker.
Arduino 2.0.17's setter selects the profile used by the first
`wifiLowLevelInit()`; it does not restart or reconfigure a running interface.
The reviewed IotWebConf 3.2.1 constructor only binds configuration objects,
and its `init()` loads settings. Connection/AP work occurs later in its
network state machine. No front/shared file is changed.

The pinned Arduino initializer otherwise replaces several SDK defaults
when static buffers have not been requested. The actual rear N16R8 qio_opi
configuration and Arduino 2.0.17 source select:

| Buffer field | Arduino default used through .14 | Static profile requested in .15 |
|---|---:|---:|
| Static RX buffers | 4 | 8 |
| Dynamic RX buffers | 32 | 32 |
| TX buffer type | Dynamic (1) | Static (0) |
| Static TX buffers | 0 | 8 |
| Dynamic TX buffers | 32 | 0 (not selected by static TX type) |
| Cached TX buffers | 4 | 16 |

These are initialization values, not a measurement of live occupancy or drops.
ESP-IDF 4.4.7 advises against fewer than six static RX buffers with AMPDU
and recommends static TX buffers when PSRAM is enabled. Arduino documents
static allocation as the performance-oriented choice for multitasking,
with greater memory use than its default dynamic allocation.
This comparison uses that supported API rather than editing SDK constants.

The .14 strict viewer test still failed rear throughput/gap checks, and sender
timing remained dominated by backpressure waits with explicit fragment-yield
time at zero. A later root-page snapshot reported 120.94 KB free internal heap
and a 109.99 KB largest internal block. Those rounded point-in-time values do
not rule out transient allocation pressure and do not establish this buffer
profile as the cause of slow delivery. The verified .15 boot and unchanged
camera/LiDAR acceptance checks below record the actual comparison.

The .14 no-yield setting, CS5, four-second write-progress budget, read-timeout
correction, camera 1280×720/200-ms profile, sensor cadence/priorities, both
LiDARs, BNO absence handling, disabled FTM, requested power and OTA behavior
remain unchanged. No additional metadata is added to normal messages.

Materialize and build in a new directory, require the production-file
comparison to contain only this setup insertion and the release identity,
then run the existing transport-budget, materialized timing and actual
timeout regressions plus the OTA-image check. Preserve the .14 image and
receipts. The build also records the exact initializer/SDK identities and
the setup ordering; host tests do not simulate Wi-Fi buffer performance.

References:

- [Arduino 2.0.17 initializer and setter](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/src/WiFiGeneric.cpp)
- [Pinned N16R8 SDK configuration](https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32s3/qio_opi/include/sdkconfig.h)
- [ESP-IDF 4.4.7 Wi-Fi buffer guidance](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html#wi-fi-buffer-usage)
- [Arduino useStaticBuffers API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html#usestaticbuffers)


### Observed result and return to .14

The .15 image (SHA256
`4742a72e93e38ac13a4ed53a79630a7a50abcd2d36ab12cb5922521fc1d2e015`)
was verified running in app0 with boot ID `f4ba2336af91d083` on
2026-11-23. The subsequent unchanged public/native two-window qualification
failed. Rear delivery was 0.533 and 0.600 fps on both routes; the native front
delivered 5.000 and 4.950 fps. The observer ended at 07:40:28.332510 UTC and
the concurrent passive capture ended at 07:40:43.138712 UTC.

Outside-core counter snapshots recorded 684 camera captures, 87 sends and
597 superseded frames; the sensor producer prepared 1,370 packets, sent
455 and superseded 915. Sender accounting recorded 87 attempts and
135.384855 seconds of send wall time, of which 87.0657% was backpressure
wait time. Successful-fragment yield time remained zero. There were no new
socket disconnects or write-progress timeouts in that bracket.

This result did not justify retaining the static-buffer selection. The
source returns to the exact .14 production inputs from main
`571af2b0ab1cb6f8426462499bf3cd3295ed7626`: the added setup call is
removed, and the normal/factory release identities return to .14. All other
camera, sensor, BNO, LiDAR, transport and recovery behavior is preserved.
The .15 source, image and observations remain in history.

The prior .14 qualification also failed rear delivery checks. This is a
return to the previous comparison baseline, not a health fix or proof that
static buffers caused the difference. These sequential live observations
do not control every link or scheduling condition.

Production equality permits reuse of the preserved normal .14 image:
**1,608,960 bytes**, SHA256
`e0bdc24af5113873f56e27e09cc90d2f878a65c4648ed7377151ddd718b594d1`.
The rollback verifies all repository production inputs against the .14
source and rechecks the existing image size/hash; it does not rebuild or
claim a new image. The original full build, transport/timing/timeout tests
and OTA-image check remain the qualification evidence. Physical rollback
is a separate guarded OTA action and requires a fresh .14 boot witness.
