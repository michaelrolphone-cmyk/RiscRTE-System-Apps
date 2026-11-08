# Biome Monitor native BiomeRTE provider

Version 2.0.0 replaces the old capabilities-service registration with a standalone, authenticated provider through the public HTTPS gateway. A Raspberry Pi is needed only for commissioning or flashing, not normal operation. Wi-Fi credentials can be reprovisioned for another network; no LAN controller address is compiled in.

## Hardware and build

Seengreat ESP32-C6 E-Paper Driver Board v1.4, UC8179 800x480 black/white panel, 4 MB flash. Pins: BUSY2, RST6, DC3, CS7, MOSI4, SCK5. Build with ESP-IDF 5.4.2. The native workflow runs host regression checks and builds the C6 image. Existing panel driver, idle logo, diagnostics, radio reset workaround and idle watchdog handling are retained.

## Provisioning

Use a verified device serial identity, make a private full-flash backup, and preserve its partition layout and Wi-Fi configuration. Never publish the backup, Wi-Fi password or private key. Do not flash other attached ESP devices.

Configuration is JSON in NVS namespace `biomerte`, key `config`:
- `origin`: public HTTPS gateway origin, no path
- `site`: controller site ID
- `provider`: SHA-256 fingerprint of RSA SPKI DER
- `resource`: dedicated display resource ID
- `private_key`: device RSA2048 PEM private key
- `ssid`, `password`: configured Wi-Fi network

The public gateway forwards existing native signed calls; device enrollment uses the controller’s existing peer trust and needs no per-device gateway deployment. The controller peer grant is limited to cap.offer, offer:display.epaper.frame:1, resource:<display resource>, cap.fenced, provider.poll, provider.claim, provider.result, cap.status and artifact.get. No terminal or admin permission is needed.

## Display contract

`display.epaper.frame:1` has capacity one. Inputs are sha256, format, width, height, stride_bytes, size_bytes and expires_at_ms. Format is mono1-msb-black1, dimensions 800x480, stride 100 and size 48000. The expiry must initially be 60 seconds to one day ahead. Upload the exact packed frame as a controller artifact before invoking. The provider verifies decoded length and SHA-256 before refresh.

Native lease/fence and claim checks govern effects. The local journal records uncertainty before panel refresh and terminal completion afterward. An unresolved refresh is never blindly repeated or replaced by fallback content. Terminal completion means the panel controller completed refresh, not optical readback.

## Public transport and recovery

POST /v1/call carries the existing RSA-signed native envelope unchanged. The controller checks enrollment, signature, scopes and revocation on every HTTP attempt. TLS uses ESP-IDF's certificate bundle; redirects are disabled. Every request ID is journaled, including reads. Transport failures and retryable errors retain the exact request and signature for recovery. Native controller receipts deduplicate mutations; no separate gateway acknowledgement protocol is required.

The monitor retries Wi-Fi and public gateway connections, synchronizes network time for TLS and frame expiry, and displays local diagnostics after sustained failures unless an effect is unresolved.

The bounded runtime journal retains the current operation atomically with its identity. Unresolved evidence is never replaced by another operation. Completed older operations cannot be claimed again because the controller retains native terminal receipts. Corrupt journals and authority mismatch fail closed.

The user LED (GPIO8, active-low) is held off from application startup. Battery-capacity LEDs are wired to the ETA9185 charging IC, not an ESP32 control pin, and remain hardware-controlled. See the manufacturer schematic and LED table: https://seengreat.com/wiki/198/esp32c6-e-paper-driver-board .
