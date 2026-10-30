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
