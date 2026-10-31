# ESP32-S3 infrastructure agent: phone setup

The v1.2 release is a complete 16 MiB first-install binary. Download the `.bin` directly from RiscRTE.com / the GitHub release and flash it at offset `0x0`. No archive extraction, Python, build tools, editor changes or USB credential injection are required for phone setup.

Target: ESP32-S3 with **16 MiB flash and octal PSRAM**, 80 MHz flash/PSRAM, 240 MHz CPU. This is a first-install image: it replaces existing flash contents and identity. Use the paired update tools for a data-preserving update.

## Configure from a phone

1. Download your private `riscrte-pairing.json` before connecting to the device. It contains the service credentials for one installation; it is never in the public binary.
2. Power the flashed device. Join Wi-Fi **RiscRTE-Setup**, password **RiscRTE-Setup**. Stay connected when the phone reports no internet.
3. Open the captive setup page. If the phone does not open it automatically, browse to **http://192.168.4.1/**.
4. Enter your 2.4 GHz Wi-Fi network name and password. Choose the private pairing file (or paste its JSON) and tap **Save and connect**.
5. Wait for **Settings saved**. The setup network closes, and the ESP32 joins the configured network. It restores these settings after power loss.

A failed first connection reopens setup so the Wi-Fi details can be corrected. A subsequent failed station connection also opens recovery setup; changing settings then requires the existing private pairing file as proof of authority. In recovery the device periodically retries its saved network. No private values are returned in the page or diagnostics.

The device saves policy and credentials together through the existing CRC-framed, bounded asynchronous journal. An uncertain commit must be resolved before reporting success. It closes DNS, HTTP clients and the listener before changing radio mode. Normal operation has no inbound server. Captive setup is the user's explicit addition to the original headless/outbound-only specification.

## Scope and verification

The image contains the native runtime, bootloader, partition table, paired-bank metadata, mutable-data seed, `infra_agent`, `infra.node` and its radio dependency. It supplies `scheduler.drive`, `service.reconcile`, `service.health.observe`, and `source.poll`; network OTA and `payload.transform` are not offered.

Automated coverage includes captive redirects, strict HTTP framing, session nonce/origin checks, bounded DNS including malformed packets, credential and implementation-pin validation, save-before-disconnect, failed Wi-Fi recovery, journal reload after reboot, both uncertain-commit outcomes, native socket ownership/retention, and the existing infrastructure service suite. The target image and ELFs are compiled for ESP32-S3. A physical board and mobile captive-browser behavior have not been tested in this environment.

The required base specification is `esp32s3-infrastructure-agent-spec-v1`, revision `f0e52f9fb704bd984a1b7ed8d0bdc9c74b2596a875d58ea74b7354f2c6667ac2`. See `CONFORMANCE.md` for the original contract coverage and `WORKSTATION-MAINTENANCE.md` for optional administrator tooling and paired updates. Secrets are not encrypted at rest in this profile.

## v1.4.2: recover a rejected participant request without replacing enrollment identity

An `INVALID_REQUEST` for a durable Capabilities request is retired only after the authenticated response identifies that exact operation and says `mutation_outcome=not_applied`. The journal atomically replaces that entry with a bounded, redacted rejection record; URLs, credentials, request bodies and free-form validation messages are excluded. `relay.health.last_error` reports `participant_INVALID_REQUEST`. Ambiguous outcomes keep their original custody.

Observation requests can continue with fresh measurements. Empty observation checkpoints start at verified UTC milliseconds instead of restarting at sequence 1 against an existing participant's source history. Execution/rotation and other participant failures retain job state and other pending requests for operator reconciliation; relay delivery, enrollment and required maintenance remain active. This is a visible participant quarantine, never a claim that execution succeeded.

For an existing v1.4.1 installation with a sealed enrollment already waiting, use the **repair bundle**, not the full initial image. With Python 3, `esptool` and a C compiler installed, close the browser USB connection, extract the bundle and run:

```sh
python3 scripts/repair_infrastructure.py --bundle . --port YOUR_SERIAL_PORT --backup /PRIVATE/NEW_BACKUP_DIRECTORY
```

The repair reads and verifies a complete private flash backup first. It prints only the pending action and operation ID, replaces the paired executable images, migrates the exact published v1.4.1 provider pins, verifies the preserved app-data volume, and activates the new pair last. It preserves credentials, relay private/public keys, subscriptions, enrollment requests, cursors and every pending request. It never publishes, acknowledges or replaces a queued bus enrollment. Keep the backup private and reset after the script reports success. If any step fails, retain the backup and the reported stage; do not substitute an initial-image flash.

To inspect a captured backup without writing to the device:

```sh
python3 scripts/inspect_infrastructure_state.py --bundle . --flash /PRIVATE/BACKUP/flash-before.bin
```

The catalog's current remote flash operation accepts only full images and erases saved data. It cannot perform this paired repair or read the running private journal. Hardware recovery is unverified until the repaired device's serial diagnostics, heartbeat and enrollment decision are observed.
