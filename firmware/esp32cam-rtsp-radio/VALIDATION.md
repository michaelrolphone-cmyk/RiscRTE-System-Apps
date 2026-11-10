# Validation record: ESP32-S3 CAM RF, AHRS and light controls

## Current scope and source

Version: `1.0-radio.4`. The only firmware target is
`esp32cam_s3_wroom_n16r8`. This package extends pinned upstream
`14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7`.
This update adds a Stratux AHRS sensor worker, cached API/UI readings, gyro
calibration, and separate UDP motion packets. The camera/light implementation
and RF acquisition are preserved; radio publication gains an independent AHRS
cadence using the existing destination.

The native project is an upstream checkout with the intentional integration
patch and overlay. The GitHub delivery repository contains that reproducible
package, rather than pretending its PR commit is the native upstream commit.

## Board and light evidence

The exact ASIN B0GD2BZ9B1 identifies a FORIOT N16R8/OV3660, dual-Type-C S3
camera board. Its seller photos were inspected. The corresponding FORIOT
pinout depicts an OV2640 camera variant on the matching board layout and labels
GPIO48 WS2812, GPIO4 CAM_SIOD, and GPIO5 CAM_SIOC. This supports the selected
one-pixel GPIO48/GRB profile; it does not substitute for physical verification
of the user's particular PCB revision.

The GPIO4 white-flash tutorial cited during the request explicitly uses an
AI Thinker ESP32-CAM. The S3 camera's global board flags use GPIO4 for SCCB SDA,
so the light module explicitly forbids GPIO4 and asserts against every defined
camera signal pin. RMT drives the pixel; the camera's LEDC timer/channel are
not reassigned.

References:

- https://www.amazon.com/dp/B0GD2BZ9B1
- https://m.media-amazon.com/images/I/7102nyCrLYL._AC_SL1500_.jpg
- https://m.media-amazon.com/images/I/71v0J4UcR1L._AC_SL1500_.jpg
- https://m.media-amazon.com/images/I/81e8TEDdnsL._AC_SL1500_.jpg
- https://docs.keyestudio.com/projects/MB0184/en/latest/docs/MB0184%20ESP32-S3%20CAM%20Development%20Board.html
- https://www.makerguides.com/control-esp32-cam-flash-led/

## Current AHRS qualification scope

The supported v1.1 sensor profile is inferred from the vendor guide's
Stratux v1.3b2 compatibility and that firmware's explicit MPU9250/BMP280
drivers. Actual chip identities must pass runtime checks. The guide does not
provide a PCB schematic or readable chip markings. Both the vendor PDF and
FORIOT header image were visually inspected.

The selected GPIO21/47 pins are exposed and avoid camera, USB, LED, SD,
and octal-PSRAM connections. The actual pinned SDK uses camera I²C controller1,
so the sensor module selects the opposite controller0. The wiring guide
reproduces the Pi header's intended3V3/5V/GND rails and documents3V3 I²C
pull-ups; these are wiring-source findings, not an electrical bench test.

New meaningful checks cover the sensor conversion/filter/calibration model,
AHRS packet parsing and receiver ordering/freshness, UDP-to-JSONL reception,
and firmware/embedded-script compilation. The scoped CI runs the actual
package preparation and single S3 build. Use the CI run associated with the
published head for its final result; this document does not predeclare a run.

The first integrated CI run passed the host/model checks but caught an
Arduino `PI` macro collision when compiling the firmware. The pure model
constant was renamed to `PI_VALUE` and its test references updated; no driver
behavior or existing feature was removed. The subsequent current-head run is
the qualification result for delivery.

No new native or browser interaction execution, connected-sensor read,
physical calibration, illumination, flash, or streaming coexistence test is
claimed. Those tests require the user's connected board.

## Prior UI verification (version `1.0-radio.3`)

The portal adds Turn on, Turn off, brightness slider/numeric entry, RGB color,
White preset, Apply color & brightness, and Reload light state. It uses the
existing authenticated `/api/light` endpoint. Page load is read-only; requests
are serialized and radio polling does not modify light form values. Turn off
is independent of form validation and sends only `{"on":false}`. API driver
errors are rendered with unknown output rather than a successful on/off state.

The native terminal capability was not advertised during this UI update, so
new native execution is not claimed. The scoped GitHub workflow verifies all
package hashes, reconstructs the pinned upstream source, checks the embedded
portal JavaScript syntax, builds the sole S3 target, and runs the existing
receiver/radio/light model checks. Consult the run tied to the delivery commit
for its actual results; this record does not predeclare CI success.

No browser interaction run or physical illumination test is claimed by this
record. Review and CI validate source integration, syntax, and firmware build;
actual illumination remains for the user's board test after flashing.

## Prior native verification (version `1.0-radio.2`)

Completed through the Mac terminal capability; no cloud development shell was used.

Operation: `esp32cam-light-module-validate-build-1`, exit code 0.

- Light model: six test functions passed, including 28 rejected request cases.
- The model compiled with `-std=c++11 -Wall -Wextra -pedantic`, without warnings.
- Tests cover default-off/color memory, integer boundaries and brightness
  rounding, atomic validation, trailing JSON content, embedded-NUL key aliasing,
  GRB/MSB wire order, 300us latch timing, and invalid clock resolution.
- The initial malformed-input test exposed escaped NUL aliasing in ArduinoJson
  object-key storage. Explicit raw/escaped NUL rejection fixed that issue before
  the successful test and firmware build.
- ESP32-S3 CAM firmware build succeeded in 20.415 seconds.
- Integration check confirmed `camera_light::begin` and version `1.0-radio.2`.
- The integration `git diff --check` passed.
- An independent source review found no blocking defects in pin ownership, RMT
  transmission/recovery, authenticated API handling, or state/error reporting.

| Native build measure | Value |
| --- | --- |
| Application binary size | 1,472,528 bytes |
| Application binary SHA-256 | `2a15b92acafa6ffbbd537f34dbb75b12a863addf7cff33557c40be9ee1b1aabc` |
| Reported RAM | 76,996 / 327,680 bytes (23.5%) |
| Reported flash | 1,472,161 / 6,553,600 bytes (22.5%) |

The checksum above belongs to the native application binary, not a GitHub
artifact ZIP. CI artifacts have their own run identity and archive digest.

New source SHA-256 values:

- `include/camera_light.h`: `c0c676b74ce90ed0134c4d82aea3591ed801c19cd0d14cae1fc519ef04c34b51`
- `include/camera_light_model.h`: `3a3d980a112c94ef9b45bbbb3be002a04da258438f4de9218c776419f3406fe6`
- `src/camera_light.cpp`: `48787da311e2260a957cd4b598f2561597eae75aa44fad7cc7469fd5d8696636`
- `test/camera_light_model/test_model.cpp`: `0e191a640d7eb980b3416aa607bd9b5a79a1eee0a17c9ba9542732a5e76af1ce`

The full native source/build receipt is content-addressed artifact
`6259f196cf9b3c0da2f741e8ec2dfbb7ca339078ea47aff9e8dd9945f39799f3`.
This binary is the prior API-enabled version and does not contain the new
portal controls. Use the artifact from the `1.0-radio.3` delivery commit's CI run
for the UI-enabled image.

## CI checks

The scoped GitHub workflow builds only the S3 target, runs the 16 receiver
tests and radio model tests, and now also runs the light model tests against
ArduinoJson 6.21.5, pinned to source commit
`40ee05c065ce248192c47eb37af7a70a6935dfa6`.
The workflow preserves S3 binaries as seven-day artifacts. Consult the workflow
run for the actual delivery commit's current result; this document does not
predeclare a future CI result.

## Physical qualification

The camera has not been flashed with this version. No live LED illumination,
color-order measurement, brightness/lux measurement, camera/lighting coexistence,
radio/video coexistence, physical OTA operation, or FTM ranging accuracy is
claimed. The API reports driver and command status, with
`hardware_verified=false` and `optical_readback=false`.

The light has no automatic duration limit. The RMT driver has a 100ms
transaction-completion fault deadline; that does not switch a successfully
latched pixel off. A failed command returns HTTP503 and marks command state
unknown rather than reporting physical success.

## Package reproduction

For `1.0-radio.4`, the manifest includes the AHRS module, model checks and
wiring/API documentation. CI verifies those hashes and reconstructs the
pinned upstream project before building; there is no claim of a new native
checkout comparison.


For version `1.0-radio.2`, the preparation script reproduced all 117 source
files byte-for-byte against the native project used for its successful S3 build.
Temporary reproduction files were removed after comparison. For version
`1.0-radio.3`, CI runs the same manifest-verified preparation script before its
S3 build; no new byte-for-byte comparison with a native checkout is claimed.
