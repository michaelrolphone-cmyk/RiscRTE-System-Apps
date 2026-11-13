# ESP32-C3 FTM anchors for the rover

One identical image runs on each 4 MB ESP32-C3. It advertises an 802.11mc FTM
responder AP, discovers other C3-TOF anchors, measures their distances as an FTM
initiator, and sends latest-state reports to the LAN on UDP 7446. The rover's
existing ESP32-S3 camera is the ranging initiator; it needs no new radio.

## Setup

Flash c3-tof-anchor-v1.0.0-factory.bin at **0x0**. This factory image includes
bootloader, partitions and application. It replaces the selected board's
firmware and NVS. Alternatively use PlatformIO upload, which preserves NVS.

After flashing, press RESET once if native USB leaves the board in download
mode. Do not hold BOOT. Each device generates a unique C3-TOF-<AP MAC> SSID.

Send a single newline-terminated JSON object over USB at 115200:
{"ssid":"YOUR_WIFI","password":"YOUR_PASSWORD","target_host":"RECEIVER_LAN_ADDRESS"}

Then send status followed by a newline. Wait for connected=true and a LAN IP
before disconnecting USB. Settings persist. The public image contains no
Wi-Fi or SSH credentials.

Without USB, join the device's open C3-TOF access point and use its gateway
address to open the setup page. The gateway is reported by the Wi-Fi client.
Configuration is accepted only over the anchor AP or USB; LAN status is read-only.
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

The deployed provider wrapper appends a tof object to the existing
robot.radio.status:1 and robot.radio.localize:1 results. Original fields and
other capabilities remain available. Its collector freshness is checked on
every query.

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

## Authenticated wireless updates (1.0.2)

After one USB provisioning, an optional per-board ota_password (16–64 characters)
enables /update with HTTP Basic authentication as admin. Use unique random
keys saved in a private local file, never committed or embedded in the image.
The configuration endpoint remains restricted to USB or a direct anchor-AP
connection. Anonymous update access is rejected. Verify a new boot ID,
firmware version and retained configuration after upload; never blindly retry
an uncertain update.

tools/provision_anchor.py selects an exact USB MAC, uses a private Wi-Fi
profile and per-MAC key file, flashes the project, and verifies network,
OTA availability and configured low TX power before reporting ready.
The private profile/key files live outside this repository with mode 0600.
A4:30 may remain on 1.0.0 as an existing responder; wireless updating is not
available on that version. RF coverage and peer ranging must be qualified
at the actual installed positions. An update alone is not proof of coverage.
