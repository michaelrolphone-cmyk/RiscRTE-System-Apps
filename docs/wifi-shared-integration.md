# Shared Wi-Fi integration status

## Qualified candidate

PR #108 commit `dae4973239c246539f3585431edd9c845e37ec85` extends the
existing actual-app fixture without changing production app/provider source.
[Exact-head CI](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/41999972495)
passed the six shared-provider semantic workflows in normal and ASan/UBSan
builds, the fourteen original Wi-Fi cases, scene/keyboard regressions, and
ESP32-S3 app/provider compilation.

One host executable is exercised at 240x240 RGB565, 410x502 RGB565,
800x480 monochrome rotated 270 degrees, 480x800 grayscale, 320x240 RGB565,
and 240x320 RGB565. Geometry is fixture/provider input, not an app branch.
The added flow covers persisted secure joins, confirmed forget, a second
saved network, shared-keyboard cancellation, static-screen redraw suppression,
and cleanup. It uses semantic intents; the original paper touch tests remain.
These are synthetic-provider checks, not pixel-layout acceptance, installed
product identity, physical RF, device-memory fit, or hardware qualification.

## Preserve the current source contract

The reconciliation baseline is System-Apps
`7270a10ed9cefba6ec1368060196a13bb3370985`. The older candidate must not
replace the current application merely because its historical CI passes.

### Credential and settings storage

Current sources:
- `Apps/wifi_atomic.inc`
- `lib/PortableApps/include/PortableWifiAtomic.h`
- `lib/PortableApps/include/PortableSettingsAvailability.h`
- `lib/PortableApps/include/PortableWifiProfiles.h`
- `lib/PortableApps/include/PortableWifiCredentials.h`

The candidate's direct KV save/forget calls do not preserve the current native
atomic and availability contract. Reuse the existing codec and atomic operation
helper rather than inventing another credential representation.

Preserve namespace-6 data ownership through editing, checked availability
generation, explicit keep/discard choices, clean reload boundaries, revision-
checked SSID/fingerprint operations, and collection of accepted submission
receipts before releasing ownership. Busy, conflict, stale, capacity, uncertain,
cancelled and retained outcomes must remain distinct. An unavailable provider
must not silently fall back to legacy KV writes. Preserve namespace-1 radio
policy behavior as well.

Bring across the existing atomic/availability behavioral fixtures, then exercise
these same boundaries through the actual shared-scene app, including keyboard
and radio cancellation during accepted storage work. The six-profile fixture
does not establish this parity.

### Complete application and shared-provider composition

Current baseline references:
- `Apps/native/wifi_settings.json`
- `lib/PortableApps/profiles/native-wifi.json`
- `scripts/build_native_wifi_qualification.py`
- `scripts/test_native_wifi_features.py`
- `scripts/test_native_wifi_boundary.py`
- `scripts/test_native_wifi_idle.py`
- `scripts/test_native_wifi_idle_target.py`
- `docs/native-wifi/README.md`

The current eighteen-capability native recipe preserves Contexts, telemetry,
power/sleep/motion, Quick Controls, catalog and return behavior in addition to
radio, storage and shared text entry. These responsibilities can move to shared
providers, but their behavior cannot be dropped when adopting the candidate's
smaller capability list. Reconcile the latest shared provider implementations
before taking the candidate's older provider files.

Preserve cancellation and input resynchronization at presentation boundaries:
new gestures must wait for the appropriate completed frame; established contacts
may survive compatible status-only repaint; changed controls and stale scan
results must not activate a different action. Preserve held-contact automatic-idle
inhibition and retained-custody behavior.

## Remaining delivery

Integrate without device-class, backend or fixed-resolution application branches.
Build one canonical ELF and verify its byte identity in the X4, T-Watch and
Waveshare product candidates, with their current compatible provider/grant cohorts.
Run the existing full source-selected recipe and new shared-scene behavioral
checks against the final integrated source. Keep current firmware/catalog
associations and hardware claims separate. This candidate remains a draft,
not a completed migration or deployment.
