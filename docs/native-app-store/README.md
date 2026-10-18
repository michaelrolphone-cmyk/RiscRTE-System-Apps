# Native App Store 1.4.0 development qualification

This additive profile selects the production native list renderer, native Quick Controls,
and native alarms inside the existing App Store controller. It does not refactor the
application architecture or modify product firmware. Existing application profiles,
ordinary builders, and releases remain independent.

## Immutable recipe and identity

- New application version: `app_store` **1.4.0**.
- Reserved additive development release: `native-app-store-1.4.0-dev1`.
- New profile: `lib/PortableApps/profiles/native-app-store.json`.
- Complete manifest: `lib/PortableApps/profiles/native-app-store-manifest.json`.
- Retained command: `app_store.command` in `docs/shared-app-crashes/replay/wave.json`.
- Retained recipe SHA-256: `5dd7876cd3a869e0326979b599bf9d2c1201b656606028b4a83a31992553f715`.
- Explicit selectors: `PORTABLE_UPDATE_NATIVE_UI`, `PORTABLE_NATIVE_CANVAS`,
  `PORTABLE_QUICK_NATIVE`, and `PORTABLE_ALARM_NATIVE`.
- The existing `PORTABLE_NATIVE_CUSTODY_FENCE` selector is preserved. The complete
  retained flags, order, translation units, local navigation/sleep bridges, RTC
  policy, atomic Quick preferences, automatic Home preferences, native-height
  display handling, and apps-only update authority are preserved.
- `profiles/native-app-store/empty_catalog.c` contains the exact retained zero-entry
  catalog. This production profile source replaces only the historical generated
  input path. It does not enable `app.catalog` or import a host fixture.
- `PORTABLE_UPDATE_VERSION="1.4.0"` binds the compiled native version to the new manifest.
- Existing `Apps/native/app_store.json` remains version 1.1.5 and byte-identical.
  Existing `Apps/app_store.json` also remains byte-identical.

The public System-Apps releases endpoint returned eight releases on 2026-10-18;
none used the reserved native App Store tag. The matching tag-prefix endpoint
returned an empty array. The full connector responses were retained separately as
build audit evidence, before parsing. Availability is checked again at publication;
this observation is not permission to reuse an existing tag later.

## Full provider contract

The manifest declares 16 capabilities, including all inherited overlay features:

`display.output@1`, `input.touch.raw@1`, `rtc.clock@2`, `alarm.service@1`,
`telemetry.broadcast@1`, `contexts.service@1`, `input.power-key@1`, `power.sleep@1`,
`display.power@1`, `board.battery@1`, `storage.key-value@1`, `net.wifi@1`,
`bluetooth.hci@1`, `motion.accel@1`, `software.update.apps@1`, and
`storage.key-value.atomic@1`.

The App Store acquires the apps-only update service at instance 0 and Wi-Fi at
instance 15. Saved Wi-Fi uses key-value namespace 6; Quick/automatic preferences
use namespace 1, including atomic namespace 1. Provider composition, exact grant
bindings, working update transport, and installation paths are deployment-owned.
The recipe does not request firmware-update authority. It does not add text input
or storage-availability features absent from the retained recipe.

External Git input pins are checked and dependency bytes are compared with their
immutable objects before and after compilation:

- Runtime: `48229bd1593a2e24e95bf4537b495ee615990fd3`
- Generic Watch bridges: `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`
- Shared profile source: `b9862e1379813a0914d4926ef59a3feb6fcf656a`
- Utilities: `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d`

The bridge checkout must select its exact pin. Other repositories may contain newer
checkouts, but their consumed headers are read only from the specified Git objects.

## Qualification and publication gates

`.github/workflows/native-app-store.yml` runs the following on the exact source:

1. Four App Store controller modes: normal, ASan/UBSan, snapshot, and snapshot with
   ASan/UBSan. Each requires 24 cases at seven dimensions and both rotations,
   totaling 336 cases per mode.
2. Actual App Store plus native Quick/alarm/atomic preference composition: five
   cases at seven dimensions and both snapshot modes, totaling 70 cases in each
   normal and ASan/UBSan mode.
3. The unchanged 312-case shared updater geometry suite and complete existing
   paired-cohort, service, and controller fault suites. A wrapper retains commands,
   raw logs and exact source hashes.
4. Twelve explicit-native-selector-off target witnesses: App Store and firmware
   updater at default, Nova, Nova180, native-height, paper-X4, and custody profiles.
5. Both complete retained updater recipes with native App Store UI deselected.
   Both candidates must match the immutable pre-change source byte-for-byte.
6. The full native App Store target, compiled-version check, strict imports/exports,
   ELF32 little-endian Xtensa structural validation, exact dependency closure and
   license notices.
7. Normal and ASan/UBSan checks through pinned production Runtime structural,
   import, role, lifecycle and allocation-placement helpers.
8. Offline source/receipt cross-checks and negative tests for omitted or extra flags,
   changed include roots, incomplete evidence, tampered assets and non-additive
   catalog changes.

The seven native dimensions are 128×128, 160×128, 128×160, 240×240, 410×502,
320×180 and 502×240. Rotations are 0 and 180 degrees. Leak detection is disabled
for the host sanitizer fixtures; address and undefined-behavior checking remain
selected.

Opt-out baseline: public System commit `02f65827d524ec7b1bd6a5d0576a9d731cc589d5`,
complete tree `e32ad8d0feeec9280569b7dccb5997c3738e2669`. An optional local sparse
replica is permitted only when its complete Git tree matches that identity. Its
materialized local commit is recorded separately. The two sides always use the
same compiler; these controls do not claim reproduction of a historical release's
binary bytes.

The builder probes the selected compiler for ELF32 little-endian Xtensa output and
records the executable, version, compiler wrapper hash, cc1 hash and libgcc hash.
The supplied local ESP32-S3 wrapper identified itself as ESP GCC **15.2.0**, not
GCC 14. The workflow explicitly selects the existing pinned ESP32-S3 GCC 8.4.0
package; its actual compiler is likewise recorded. Reports from different
compilers are separate qualifications.

## Running locally

Use new/empty output directories. From the System checkout, with the four pinned
repositories available beside it:

```sh
export NATIVE_APP_CC=/absolute/path/to/xtensa-esp32s3-elf-gcc
python scripts/test_native_app_store_release.py
python scripts/build_native_app_store_qualification.py \
  --runtime ../runtime --bridges ../bridges --utilities ../utilities \
  --profile-source ../profile_source --output-dir build/native-app-store/app
python scripts/report_native_app_store_memory.py \
  --elf build/native-app-store/app/app_store.elf --runtime ../runtime \
  --output build/native-app-store/admission
cp build/native-app-store/admission/elf-memory-admission.json build/native-app-store/app/
python scripts/run_native_app_store_regressions.py --output build/native-app-store/regressions
```

The workflow contains the complete controller, overlay and byte-control invocations.
Retain its directory layout when assembling local evidence. Run the final offline
gate with `scripts/native_app_store_evidence.py --artifact build/native-app-store
--source-sha <exact-commit>`. `--allow-dirty` is available only for local
integration diagnosis. The publisher has no dirty-source bypass.

## Immutable release pipeline

Ordinary pushes and PRs cannot publish. The dedicated workflow must be manually
started on `main` with `publish_development=true`, after qualification succeeds.
The publish job names the `native-app-store-release` environment. Repository
administrators should configure that environment's desired approval protection;
this file alone does not create or guarantee an approval policy.

`publish_native_app_store.py` defaults to local preparation. `--publish` is an
explicit opt-in. It requires a clean exact-source checkout and the complete source-
bound evidence set, creates a new tag and draft prerelease, downloads and checks
all draft assets before publication, then downloads every asset through public
release URLs and verifies SHA-256, sizes, archive/direct agreement and exact tag
source. Raw command responses, downloaded bytes and HTTP metadata are retained
before parsing. Earlier release metadata and asset identities/digests must match
before and after. No asset clobbering, deletion, tag movement or old-release edit
is supported. An existing complete release for the exact same source is audited
read-only. A partial draft or conflicting tag is left untouched for review.

Public assets include the ELF, manifest, target build record, command, section
layout, production admission report, aggregate qualification, catalog binding,
publication metadata, deterministic qualification archive, and SHA256SUMS.
A failed or unavailable public audit is a blocker to catalog binding even if the
release was created successfully.

`catalog-binding.json` provides exact image/manifest URLs and hashes, version,
source and required grants. `verify_native_app_store_catalog.py` verifies a proposed
catalog export adds exactly that audited binding while preserving every previous
entry. It does not mutate a catalog or create a firmware payload. The owner of the
catalog integration must adapt its external schema to this explicit contract,
apply the authorized additive binding, read it back and retain the audit.

## Qualification limits

These receipts establish software behavior under bounded providers, target
structure and production helper admission. They do not establish physical-device
memory fit, provider transport or actual install/launch behavior. Section allocation
requests exclude dynamic buffers, sleep snapshots, stacks, allocator fragmentation,
Runtime/provider concurrency and caches. Nothing here changes Runtime/Wave .60
work, firmware payloads or retained releases. Physical testing and deployment
qualification remain separate release gates.
