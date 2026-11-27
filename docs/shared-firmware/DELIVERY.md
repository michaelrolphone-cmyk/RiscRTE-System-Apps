# Shared Firmware Update delivery

This package replaces Firmware Update with one canonical semantic application shared by T-Watch S3, Waveshare S3 Touch AMOLED 2.06 and Xteink X4 Pro. The app delegates layout, resolution, physical input and presentation lifecycle to each product's existing `ui.scene` provider. Product-specific firmware feeds and installation policy remain in independently built `software.update.firmware` providers.

## Package and identity

`ota_update.elf` is byte-identical in all three selections. Per-product manifests retain the existing capability API versions, including the product's alarm contract; those declarations do not select a different executable. `firmware-delivery.json` binds the exact source commit, dependency source commits, build receipts, product stores, SD archives, native firmware and asset hashes.

The baseline is the immutable cumulative Touchpad release `touchpad-shared-77c427a75e5651f03119c059fcdfd36d23df8708`. The assembler verifies the published baseline bytes before changing the Firmware Update app, its manifest, explicit app grants and its existing firmware-update provider. It preserves all other internal and SD bytes, store geometry, native firmware, cohort metadata, namespace assignments and unrelated boot authority. X4's firmware provider SD fallback is renamed to the new provider's content hash and its existing route is updated atomically. Other fallback routes are not rewritten.

Existing installed app identity, label and icon remain intact. The scene provider owns direct input; the app retains the non-input services needed for brightness observation, radio preferences, shared controls, alarms and background behavior. Explicit `ui.scene`, `time.civil` and native `app.catalog` grants support shared presentation, civil-time policy and the existing Context launch behavior.

## Preserved behavior

The shared controller supports optional service discovery, saved Wi-Fi credentials in the existing namespace, quick radio preferences, release selection, full release identity and reason text, explicit installation confirmation and revalidation immediately before beginning an update. A cancellation is scoped to the current operation and survives progress presentation without allowing stale install or selection actions. The firmware provider's append-only metadata suffix reports configuration and network requirements without changing the established update API prefix or initiating I/O merely to inspect readiness.

Context, broadcast/Bluetooth, telemetry, alarm, power and shared-control work uses the established reusable background controller. UTC and civil time come from capability contracts, not a compiled product timezone. Missing optional services disable or explain the corresponding feature; a retained or failed service is not silently treated as absent. Lifecycle fences preserve required pause and cleanup boundaries before an unsafe update operation.

## Installation boundaries

Use only the exact matched native firmware, store and SD asset set recorded in `firmware-delivery.json`. These cumulative assets are not a generic initial-flash image and are not interchangeable with earlier native firmware. Devices without the matching runtime and encoded-image decoder need the baseline's established firmware-first procedure. Preserve private data, bonds, Wi-Fi preferences, alarms and Context models through the normal transactional installation path; this software qualification does not authorize erasing a device or replacing its private data.

A known inherited X4 baseline limitation remains: content-addressed fallback names for the existing power, scene-host and civil-clock providers do not match their current internal image hashes. This delivery fixes the changed firmware-update provider's own fallback route but deliberately does not mutate those unrelated providers. The Runtime's preserving-update policy may reject a provisioning operation involving those inherited mismatches. Therefore the package is not evidence that every baseline preserving-upgrade path is installable. Resolve that cumulative packaging issue and qualify the actual installation path before relying on an X4 preserving firmware upgrade.

## Evidence and limits

The delivery workflow requires normal and ASan/UBSan tests of the actual shared controller and background boundaries, readiness-prefix compatibility and legacy portable update regressions. Leak detection is disabled in those sanitizer runs; address and undefined-behavior checks remain enabled. It independently repeats the target application build, checks canonical ELF identity across three selections, repeats each product's firmware provider build and requires equal provider ELF and manifest bytes.

Owning native-profile Runtime harnesses prepare each candidate graph and validate internal images in normal and sanitized modes. Fixed-size SPIFFS stores and SD archives are unpacked and roundtripped against exact candidate trees. External SD routes are bound to the expected bytes, but this does not execute the app on removable media or on a device.

No physical device execution, on-device update, real network transfer, bank activation/reboot, live heap fit, actual display responsiveness, or complete hardware background/persistence test is claimed. Host fixtures and static target admission do not replace those tests. Published source and software qualification are distinct from physical confirmation.

## Reproduction and provenance

`source.zip` retains exact source archives and commit IDs for System, Utilities, both Runtime revisions, the three product sources and the boot-filesystem tools. `qualification.zip` retains host and admission evidence, commands and logs. `application-build.json`, `assembly.json`, admission receipts and provider build receipts bind source inputs, selected policies, toolchains, artifacts and hashes. The repository's `docs/shared-firmware/source-lock.json` pins the dependencies, and `.github/workflows/firmware-scene-delivery.yml` specifies the complete reproduction.

Publication occurs only after qualification succeeds on the designated source branch. The immutable publisher rejects differing or unexpected assets, verifies authenticated upload reads and then checks anonymous public downloads. A published release still carries the hardware and installation limitations above.
