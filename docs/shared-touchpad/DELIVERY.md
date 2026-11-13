# Shared BLE Touchpad delivery

One device-neutral application and the reusable ui.scene pointer extension are integrated into published shared Home cohort 83afd19e5184eb511cc205299c83eb9cb5c05430.

## Packages

The canonical ble_touchpad.elf is byte-identical in X4, T-Watch S3 and Waveshare selections. Independently compiled ui.scene profiles own geometry, physical input, gestures, frame custody and lifecycle. Matching store.bin and sd-apps.zip assets preserve original partition sizes, app paths, namespaces, all unrelated canonical and stored bytes, and native firmware. Only Touchpad, the existing scene provider and explicitly recorded application grant delegation/facade bindings change.

Native firmware and cohort.json are unchanged from the hash-verified Home release. Use only the exact firmware/store/SD combination in touchpad-delivery.json. Devices not already running that matched firmware must use the Home delivery's firmware-first upgrade procedure, including its encoded-image decoder requirements. This package is not a generic initial firmware flash. Preserve private data, bonds, radio preferences, alarms and Context models through the normal transactional installation path.

## Behavior

Preserved interactions include left-click tap, tap-drag, pointer motion, two-finger right click and two-axis scroll. The shared controller retains secure-ready checks, neutral release, held-report refresh, reconnect, pairing confirmation/rejection, explicit bond forgetting, battery reporting, alarm reservation/dismissal, shared controls, idle/power policy and shared Context/broadcast background behavior.

Settings and lifecycle interruptions close HID; return does not reconnect automatically. Confirmation requires a fresh press/release on the exact challenge after its frame completed presentation. Stale, ambiguous, interrupted and previsibility contacts cannot confirm. A zero-flag checkpoint resume preserves its settled visual instead of repainting unnecessarily.

UI, HID, pointer/hold, alarm, power, storage and background contracts are optional. Missing features are disabled or disclosed. Missing UI performs no domain/storage acquisition. Legacy HID lacking two-axis scroll gets its supported fallback with the limitation disclosed.

## Evidence and limits

Normal and ASan/UBSan suites cover pointer/held input, legacy scene prefixes, controller and background behavior, actual Runtime/app/provider lifecycle across three display profiles, alarms, missing services, and twice unload/reopen. The target app is independently rebuilt and copied unchanged to each selection. Provider profiles are separately repeated and validated.

Owning native-profile Runtime harnesses prepare all three product graphs and validate internal images. Fixed-size stores and SD archives are roundtripped and compared. Wave/X4 external Touchpad routes are bound by exact bytes to the validated canonical ELF; this is not physical removable-media loading evidence.

Physical pairing/bond persistence, host interoperability, display latency, live heap fit and device execution remain untested. Software qualification and publication do not claim hardware confirmation.

## Provenance

source.zip, source-lock.json, qualification.zip, application-build.json, assembly.json, admission receipts and provider build receipts retain exact sources, compiler, artifacts and hashes. Font licenses accompany provider assets and source archives. The immutable publisher checks uploaded bytes with authenticated reads, then verifies public anonymous downloads. See touchpad-delivery.json for the exact dependency closure and scope.
