# Biome Monitor firmware 1.1.1

This is a native ESP-IDF firmware for the **Seengreat ESP32-C6 E-Paper Driver Board v1.4 (ESP32-C6FH4)** and its **800×480, 7.5-inch black/white UC8179-class panel**. The recovered Vector Arcade `c6_0.3.2` archive is the hardware source of truth. It is an **ESP32-C6 image**, not an ESP32-C3 image.

Biome Monitor is an outbound Capabilities Service participant. It enrolls with the same credential mechanism as infrastructure providers, opens a session, checks in, advertises an exact pinned `provider-activation/1` profile and publishes an offer after activation and a successful controller BUSY probe. It supports one display profile and one in-flight frame. It does not implement the S3 runtime's Relay/Explorer/Sentinel/Reconciler/Continuity executors.

## Default display

The supplied borderless Biome emblem and `Biome Monitor` wordmark are built into the firmware as a centered black-on-white, 800×480 native 1-bit frame. The wordmark uses Archivo Bold (weight 700), matching the RiscRTE logo source. It appears immediately after a successful panel probe, **before provisioning, Wi-Fi or service connectivity**. A validated remote frame replaces it. The logo returns when that frame expires or the device receives withdrawal of that desired state. Unrelated/stale withdrawals and invalid replacement frames do not erase a still-valid displayed frame. Reboot always starts with the logo and requires fresh authority before accepting remote content.

Expiry uses a monotonic deadline derived from the service timestamp, including while offline or retrying requests. Refresh is serviced by the same task that owns the display: an in-progress transport operation or physical refresh may delay fallback. The idle logo is refreshed once per transition, not at every poll. A controller fault makes the panel state unknown and disables further refreshes until reboot. A valid image can remain after activation expires; desired-frame expiry is what returns it to idle. Power loss retains the e-paper's last physical image.

The canonical preview is `assets/idle-logo.png`; `main/assets/idle_logo.bin` is its exact packed representation. Historical Vector Arcade mentions below identify the recovered hardware source only. Existing immutable capability/provider IDs and digests are retained for compatibility; the participant name and firmware identity are Biome Monitor.

## Hardware and display contract

| Signal | GPIO |
|---|---:|
| BUSY (active low) | 2 |
| RESET | 6 |
| DC | 3 |
| CS | 7 |
| MOSI | 4 |
| SCK | 5 |

SPI mode 3, 4 MHz. The full-refresh register sequence and pixel polarity come from the recovered driver. The firmware requires BUSY to assert and then release; a disconnected BUSY line cannot report success. Hardware/optical qualification is still required. No device has been flashed by creating this build.

Capability `display.epaper.frame@1`, operation `render`, execution mode `desired_state`, effect semantics `convergent_state`. `catalog.json` contains the complete input schema, frame descriptor and immutable contract/provider pins. The ready offer repeats `frame_spec`, so a consumer can discover the format without this README.

| Property | Required value |
|---|---|
| Dimensions | 800×480 |
| Native tone depth | 1 bit/pixel; two tones |
| Bit value | 0 white; 1 black |
| Byte order | Rows from top to bottom; columns left to right |
| Within each byte | Most significant bit is the leftmost pixel |
| Stride | 100 bytes, no padding |
| Total | Exactly 48,000 bytes |
| Origin/rotation | Top left; 0 degrees |
| Compression | None |
| Refresh | Full frame, full refresh |
| Integrity | Lowercase SHA-256 of all 48,000 bytes |
| Transfer | Same-service artifact, scoped to recipient participant |

Pixel `(x,y)` is byte `y*100+x/8`, mask `0x80>>(x%8)`. An all-white frame is 48,000 zero bytes; all-black is 48,000 `0xff` bytes. Arbitrary bitmaps are accepted. Higher-depth source images can be thresholded or dithered by the sender; native gray levels and experimental partial-refresh LUTs are not advertised.

The desired input is:

```json
{"format":"mono1-msb-black1","width":800,"height":480,"stride_bytes":100,"artifact":{"artifact_id":"artifact_…","sha256":"64 lowercase hex characters","size_bytes":48000}}
```

## Connection and provisioning

The service endpoint is compiled to `https://capabilities-service.cantikash.chatgpt.site`. HTTPS certificate verification uses the ESP-IDF certificate bundle; redirects and alternative artifact origins are refused. The device uses Wi-Fi station mode, SNTP and outbound HTTPS. A private Wi-Fi-only configuration is also supported: it joins the network and retains the logo, but performs no service calls until service credentials are provisioned. No inbound rendering port is opened. This first native build uses a private NVS provisioning image; the S3 phone setup portal is not included.

1. With operator credentials, run `python tools/client.py --credentials operator.json publish`. This publishes the contract/provider definitions. `operator.json` contains `site_access` and, if required for the deployment, `credential`. Do not distribute this file.
2. Run `python tools/client.py --credentials operator.json enroll --out device-private.json`. The issuer saves a random bootstrap and separate participant credential before issuing a single-use enrollment valid for 24 hours. Edit `ssid` and `password` in that private file. Use a device-appropriate Site gate credential rather than distributing a broad operator credential.
3. In the ESP-IDF 5.4.2 shell, run `python tools/provision.py device-private.json device-nvs.bin`. This creates a private 128 KiB NVS image. For a Wi-Fi-only installation, the JSON may contain just `ssid` and `password`; service fields are left empty. An explicit `--nvs-generator PATH` can select an installed ESP-IDF-compatible NVS generator when `IDF_PATH` is unavailable. It contains secrets and must not be uploaded to the public catalog or source control.
4. After confirming the connected board is the recovered C6 board, flash the release files and private NVS image:

```sh
python -m esptool --chip esp32c6 --port PORT write_flash \
  0x0 bootloader.bin 0x8000 partition-table.bin \
  0x9000 device-nvs.bin 0x30000 firmware.bin
```

For manual installation, the release also contains `biome-monitor-esp32c6-v1.1.1-0x0.bin`: flash that single file at **0x0** on the recovered C6 board. It contains bootloader, partition table and application, with **blank NVS**. It shows the logo without provisioning, but Wi-Fi and service access still need the separate private NVS image at `0x9000`. The merged image overwrites existing NVS; for an already provisioned device use the separate application image at `0x30000` to preserve configuration.

This is a new partition layout. Replacing NVS removes enrollment and outstanding local journal state; do not write the provisioning partition as a routine firmware update. The factory image requires 4 MiB flash. No OTA path is provided in this release.

5. Once enrolled, identify the participant in Capabilities Service and activate supply with `python tools/client.py --credentials operator.json demand --participant PARTICIPANT --expires-at FUTURE_UNIX_MS`. This creates one demand for one matching panel. The recovered project has one panel; a multi-panel deployment should set the required count and/or introduce distinct local profile labels explicitly. Wait for its offer to become `ready`.

The profile is dormant without demand. A failed probe prevents readiness. On reconnect, a fresh session/lease is required; credentials, pending mutations and control records persist in NVS. An exact persisted request is retried after a lost response. No restored grant permits refresh after reboot. Unknown controls remain recorded and block admission for diagnosis rather than being skipped.

## Send a frame

For a raw frame, no image dependency is needed. For PNG/JPEG/etc., install Pillow on the consumer and run:

```sh
python tools/pack.py image.png frame.bin --dither
```

The input must be exactly 800×480; crop/resize explicitly. Transparent pixels are composited onto white. Omit `--dither` for thresholding.

Discover the ready offer from the catalog. Its standard ID is `PARTICIPANT:riscrte.esp32c6.epaper@1:panel`.

```sh
python tools/client.py --credentials operator.json render frame.bin \
  --participant PARTICIPANT --offer OFFER_ID \
  --request-id UNIQUE_STABLE_REQUEST_ID --expected-revision 0 \
  --expires-at FUTURE_UNIX_MS
python tools/client.py --credentials operator.json status PARTICIPANT:epaper-frame
```

Use the current desired entity's `revision` for subsequent updates. Keep the request ID, parameters and client SQLite journal unchanged on a retry. A new image needs a new request ID. `expires-at` is an absolute Unix millisecond timestamp; allow several minutes for transfer, lease renewal and full refresh.

The client uploads and finalizes the immutable artifact with `scope.participant_ids`, then calls `desired.put`, binding the offer and `slot: frame`. The firmware checks metadata, reads exactly 48,000 bytes, verifies SHA-256, renews authority and re-reads the desired revision before touching the panel. A fresh update may supersede an older frame; this interface converges to the latest state and is not a queue of animations.

Acceptance is not completion. Poll until the expected `desired_revision` is observed and `actual.status` is `rendered`. `actual.controller_refresh_completed` means BUSY asserted and then cleared. It does not prove optical pixels through a camera. Timeout/fault results are `unknown`, never `rendered`; the provider becomes unhealthy until reboot/reprobe. A crash after refresh but before acknowledgement can cause a safe full repaint of the same desired bitmap. An ongoing physical refresh drains to completion; lease expiry prevents new refreshes. The local logo replaces withdrawn/expired content as described above; power loss retains the last physical image.

## Build and verification

```sh
# ESP-IDF v5.4.2 (same SDK version as recovered Vector Arcade release)
idf.py set-target esp32c6
idf.py build
python tools/package.py
python -m unittest discover -s tests -v
gcc -std=c11 -Wall -Wextra -Werror -I main main/frame.c tests/frame_test.c -o /tmp/frame-test
/tmp/frame-test
```

CI builds the C6 image and packages bootloader, partition table, firmware, the merged 0x0 image, checksums, this document and source. Host checks cover packing, exact geometry, bounded writes, path confinement, lease admission, mutation replay, contract pins, logo byte identity, idle expiry, stale withdrawal rejection and current-content deduplication. `tests/service_integration.mjs` runs against the real Capabilities Service source when `CAPABILITIES_SOURCE` is supplied. Software checks do not constitute hardware qualification.

Source provenance: recovered archive SHA-256 `237000cff740f7abe1fae52f5ecd5a99dce3331f921b7c69d3d885677fb9528b`, project `c6_0.3.2`, display driver `components/epd/epd.c`. This firmware retains only its vendor-derived full-refresh path. The source archive records its own release as untested on hardware.
