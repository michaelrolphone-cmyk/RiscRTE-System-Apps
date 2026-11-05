# Standalone native Firmware Update qualification

This additive software-qualification profile selects the production native
updater presentation, native Quick Controls, and native alarms for `ota_update`.
The shared controller retains firmware-only service authority. It does not build
a firmware image, change a product pin or release, deploy an application, or
establish physical-device behavior. Existing App Store profiles and builders
remain separate and byte-identical to the source baseline.

## Explicit target identity and complete recipe

- Source baseline: `c94f367f6ca3677954b580833f222ae16775c964`.
- Profile: `lib/PortableApps/profiles/native-firmware-update.json`.
- Application: `ota_update`, version `1.4.0`, output `ota_update.elf`.
- Manifest: `lib/PortableApps/profiles/native-firmware-update-manifest.json`.
- Retained recipe: `ota_update.command` in
  `docs/shared-app-crashes/replay/wave.json`.
- Whole retained recipe SHA-256:
  `5dd7876cd3a869e0326979b599bf9d2c1201b656606028b4a83a31992553f715`.
- Retained recipe source commit:
  `297c38619a867f6200cc2f605b6193e4394bdfa2`.

The target builder preserves every retained compiler/linker argument and its
order, including `PORTABLE_UPDATE_FIRMWARE=1`, Wi-Fi instance 15, fixed UTC+08
RTC conversion, the return to `springboard.elf`, both local navigation/sleep
bridges, Contexts, broadcast, low battery, motion wake, atomic Quick preferences,
automatic Home preferences, async RGB565 delivery, and native-height handling.
It adds only the explicit native selectors and version/include inputs needed by
this standalone target: `PORTABLE_QUICK_NATIVE`, `PORTABLE_ALARM_NATIVE`,
`PORTABLE_UPDATE_NATIVE_UI`, `PORTABLE_NATIVE_CANVAS`,
`PORTABLE_UPDATE_VERSION="1.4.0"`, and staged profile/Runtime include directories.
The retained `PORTABLE_NATIVE_CUSTODY_FENCE` occurs exactly once.

The exact translation units remain `ota_update.c`, `adapter.c`,
`portable_navigation.c`, `portable_sleep.c`, `quick_actions.c`, `quick_render.c`,
`quick_session.c`, `quick_radios.c`, and the production zero-entry
`profiles/native-firmware-update/empty_catalog.c`. The last file preserves the
retained catalog bytes without compiling a historical host-fixture path.
No update-provider source is linked into this application.

## Required capabilities and authority boundary

The manifest declares the complete 16-capability feature closure:

`display.output@1`, `input.touch.raw@1`, `rtc.clock@2`, `alarm.service@1`,
`telemetry.broadcast@1`, `contexts.service@1`, `input.power-key@1`, `power.sleep@1`,
`display.power@1`, `board.battery@1`, `storage.key-value@1`, `net.wifi@1`,
`bluetooth.hci@1`, `motion.accel@1`, `software.update.firmware@1`, and
`storage.key-value.atomic@1`.

The application acquires `software.update.firmware@1` at instance 0. It never
requests `software.update.apps`, direct bank access, or `app.catalog`. Wi-Fi uses
instance 15 and saved credentials use key-value namespace 6. Shared/atomic
preferences use namespace 1. Deployments must supply matching provider grants;
this profile does not supply or change those bindings.

The unchanged firmware provider requires `platform.http-client@1`,
`platform.bank-store@1`, and `platform.clock@1`. Its catalog classification,
paired-cohort admission, streaming, verify-before-activate, transport cleanup,
retained failure, uncertain activation, and restart paths remain provider/Runtime
responsibilities. Paired banks, cohort metadata, durable activation state and
transaction receipts remain owned by the deployed Runtime and provider. This
target neither creates transaction receipts nor substitutes a reduced provider.

The builder separately compares 15 provider/API/parser/manifest contract files
with their immutable bytes at the source baseline, both before and after the
build. These hashes appear as `service_contract_sha256`; they are deliberately
separate from the application's actual compiler-discovered `source_sha256`.
This preservation check does not claim that a provider or native bank transaction
was executed. The existing shared updater regressions exercise those service
contracts independently.

## Immutable external inputs

- Generic Watch navigation/sleep bridges:
  `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`.
- Runtime SDK headers: `48229bd1593a2e24e95bf4537b495ee615990fd3`.
- Shared profile source: `b9862e1379813a0914d4926ef59a3feb6fcf656a`.
- Utilities alarm headers: `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d`.

The bridge checkout must have its pinned HEAD because its sources compile in
place. Other inputs are read from exact Git objects, so their checked-out HEAD
may differ. Every compiled dependency is attributed to its source repository and
hashed before and after compilation. Unowned sources and target inputs under
test/fixture directories are refused. The command, compiler wrapper, actual
compiler target, cc1, libgcc, source hashes, metadata hashes, linker section
layout, manifest, and license notices are retained with the target ELF.

## Running locally

Use an installed ESP32-S3 compiler and local repositories containing the exact
objects above. This workflow installs or downloads no tools. Output directories
must be new or empty.

```sh
export NATIVE_APP_CC=/absolute/path/to/xtensa-esp32s3-elf-gcc
python scripts/test_native_firmware_update_target.py
python scripts/build_native_firmware_update_qualification.py \
  --runtime ../runtime --bridges ../bridges --utilities ../utilities \
  --profile-source ../profile_source \
  --output-dir build/native-firmware-update/target
python scripts/report_native_firmware_update_memory.py \
  --elf build/native-firmware-update/target/ota_update.elf \
  --runtime ../runtime --output build/native-firmware-update/admission
```

The negative contract suite tests every omitted/reordered recipe argument,
duplicate/zeroed selectors, changed fixed values, extra flags or translation
units, substituted source/header/include/map paths, stale output directories,
host compilers, changed service contracts, changed recipe bytes, version mismatch,
and interchange of the firmware and App Store recipes. It also checks existing
manifests/builders against the immutable source baseline. Admission-binding
negative controls refuse changed ELF/manifest/layout/command bytes, stale or
incomplete target receipts, missing required source identities, changed source
hashes, invalid source paths, and non-Xtensa input.

The target compiler must produce ELF32 little-endian Xtensa. The final image must
be ET_DYN, contain its explicit application version, export exactly `app_main`,
`app_module_init`, and `app_module_fini`, and import only the bounded application
allowlist. A native structural validator checks the actual output bytes.
The separate admission reporter uses immutable production Runtime helpers in
normal and ASan/UBSan modes. It checks ordinary application admission,
missing-import rejection, unsafe-lifecycle rejection, driver-role rejection,
and section alignment placement. Its required target build receipt must match
the exact ELF, manifest, layout, command, current source hashes, and preserved
provider contracts before and after admission. `elf-memory-admission.json`
retains the production helper hashes and the complete target source binding.

## Evidence and limits

`firmware-update-build-record.json` identifies the exact compiled source,
immutable external inputs, service contract preservation, compiler implementation,
manifest capabilities, and resulting ELF. A dirty working tree is explicitly
recorded; a local development qualification is not a released-source identity.

Section-layout figures are allocation requests for the selected loader model.
They exclude dynamic frame and Quick/sleep snapshots, stacks, allocator overhead
and fragmentation, shared Runtime/provider/cached-image memory, and other
concurrent work. Native instructions are not executed by host validation.
Physical memory fit, grants, live network/provider transport, installation,
activation and restart on hardware remain unqualified. No publication or
deployment command is part of this target builder.
