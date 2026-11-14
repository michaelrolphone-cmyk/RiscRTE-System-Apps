# ESP32-C3 FTM anchors for the rover

One identical image runs on each 4 MB ESP32-C3. It advertises an 802.11mc FTM
responder AP, discovers other C3-TOF anchors, measures their distances as an FTM
initiator, and sends latest-state reports to the LAN on UDP 7446. The rover's
existing ESP32-S3 camera is the ranging initiator; it needs no new radio.

## Setup

Build the current source with PlatformIO, then use pio run -t upload with
the exact USB port. This writes the bootloader, partitions and current
application while preserving NVS. A standalone application image belongs at
0x10000; only a correctly merged factory image belongs at 0x0.

After flashing, press RESET once if native USB leaves the board in download
mode. Do not hold BOOT. Each device generates a unique C3-TOF-<AP MAC> SSID.

Send a single newline-terminated JSON object over USB at 115200:
{"ssid":"YOUR_WIFI","password":"YOUR_PASSWORD","target_host":"RECEIVER_LAN_ADDRESS"}

Then send status followed by a newline. Wait for connected=true and a LAN IP
before disconnecting USB. Settings persist. The public image contains no
Wi-Fi or SSH credentials.

Without USB, join the device's open C3-TOF access point and use its gateway
address to open the setup page. The gateway is reported by the Wi-Fi client.
Configuration is accepted over the anchor AP or USB. Firmware 1.0.3 also permits LAN configuration with the provisioned per-board OTA authentication; anonymous LAN writes remain rejected.
All anchors and the rover should use the same 2.4 GHz LAN.

Optional settings:
- height_m: antenna height above the same floor datum, meters.
- offset_m: receiver ranging offset to subtract, measured by calibration.
- target_host: collector address, in addition to LAN broadcast.

Keep all three anchors powered and spread them around the room in a broad
triangle. A nearly straight line gives poor position observability. USB data
is unnecessary after provisioning.

## Telemetry

GET /api/status returns rover.tof.anchor/1. Identity is the AP BSSID, with boot
ID and sequence. Ranges name their target BSSID and include distance_m,
rtt_ns, samples, age_ms, status and valid. Failed, absent and stale readings
are never converted to zero distance. Latest state is bounded to twelve peers;
there is no accumulating measurement journal.

The same status is published over USB and UDP 7446 every two seconds.
Peer scans and FTM sessions are serialized. A cancelled session must deliver
its retirement report before another measurement starts; a stalled retirement
is explicit and does not fabricate a new range.

## Rover integration

tools/tof_coordinator.py listens for anchor reports and requests serialized
measurements through the rover camera's existing /api/telemetry/ftm endpoint.
It writes one atomic latest.json file. It does not drive the rover, cancel
other clients' measurements, replace the world map, or reset the camera.

The current Biome vision provider exposes robot.tof.status:1 and
robot.tof.localize:1 directly, with collector freshness checked on each query.
The older wrapper is a legacy adapter for deployments predating retirement
of RSSI/BLE robot.radio queries; do not restore retired radio capabilities.

Anchor pair ranges construct a relative frame: A at the origin, B on +X, C
on the +Y side. This does not establish world-map translation or heading.
If anchor heights are configured, the rover's known camera height (0.0635 m)
allows a 2D floor-plane solution. Otherwise three ranges determine two 3D
positions reflected through the anchor plane; both hypotheses are reported.
A unique world position is not claimed from ambiguous geometry.

FTM systematic bias and multipath need real calibration. Residual fit error is
not an absolute accuracy bound. Inconsistent, stale or singular geometry is
reported invalid and grants no navigation authority.

## Build and verification

PlatformIO espressif32 6.13.0, Arduino ESP32 2.0.17, ArduinoJson 6.21.5.
pio run
python3 -m unittest discover -s tools -p 'test_*.py'

Physical qualification must separately check flash verification, normal boot,
LAN association, responder discovery, rover FTM success, peer ranges, and
three-anchor geometry after deployment. Compilation alone establishes none
of those hardware results.

## Authenticated wireless updating (1.0.2)

A unique per-board ota_password (16–64 characters), provisioned over USB or
the direct anchor AP, enables /update with HTTP Basic authentication as admin.
Keys are generated and stored outside the repository in a mode-0600 file;
no credentials are embedded in this source or image. Anonymous requests
are rejected. Verify the new boot ID, version, network and retained settings
after upload; never blindly retry an uncertain update.

The exact-MAC tools/provision_anchor.py reads a private Wi-Fi profile and a
per-MAC key file, uploads the firmware, and confirms association, OTA enablement
and the selected low-power profile. Firmware 1.0.0 remains a functioning FTM
responder but has no wireless update endpoint.

## Measurement retention

The collector keeps one last successful anchor observation per directed edge
until its original 30-second expiry, including during a pending or failed new
attempt. The timestamp is never refreshed by a retry or repeated packet;
reboots reset that node's observations. Latest attempt status stays visible.
Rover range observation span is reported separately. Moving-rover localization
requires motion compensation; asynchronous ranges are not asserted to be a
simultaneous navigation pose.

## Camera reboot recovery

The coordinator reconciles pending requests against camera boot ID, uptime,
and authoritative idle never_requested status. A reboot clears the old pending
request and pre-reboot rover ranges. It preserves busy/retiring sessions from
other consumers. Recovered request progress does not qualify position accuracy.

## Transmit power trials (1.0.3)

USB or authenticated LAN configuration accepts tx_power_dbm (2–19.5 dBm).
Set tx_trial_ms to try a temporary setting; the saved setting is restored when
the trial expires. commit_tx_power=true explicitly persists an active trial.
Status reports the SDK-applied tx_power_dbm and tx_trial_active. Observe boot
identity, association and distinct successful ranges before committing power.
Changing power alone does not establish ranging accuracy or anchor geometry.

## Reciprocal rover ranging

The S3 camera can also act as an FTM responder. Existing 1.0.3 anchors discover
its C3-TOF-ROVER- SSID and measure it without associating or reflashing. See
[RECIPROCAL.md](RECIPROCAL.md) for collector quality rules and the complete-status
transport fallback needed when larger reports exceed the legacy UDP writer.


### Joint self-localization

All anchor coordinates are unknown estimates. Adding an anchor fits the entire measured graph jointly, and subsequent validated refinement updates every included anchor. Rigid alignment retains coordinate-frame continuity without fixing anchor-to-anchor distances. No manual placement-prior file is consumed and no established anchor is treated as physical ground truth.

Sparse six-node graphs retain their measured-link observability and residual checks. Optimizer starts are ranked by the same soft-L1 objective they minimize. Inconsistent reciprocal measurements are rejected as measurements; coordinate estimates are not used to declare an endpoint correct.

Build the S3 with `pio run -e esp32s3`; the C3 environment remains `esp32c3`. Validated LAN peer identity/channel discovery supplements Wi-Fi scanning so the S3 can initiate ranges.
