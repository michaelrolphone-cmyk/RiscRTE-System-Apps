# Native Settings software qualification

Settings explicitly selects `PORTABLE_SETTINGS_NATIVE_UI` together with
`PORTABLE_NATIVE_CANVAS` and `PORTABLE_NOVA_UI`. The ordinary builder defaults
and paper/X4 path are preserved. Native layout uses capability dimensions,
whole visible rows, clipped labels, direct native glyph/icon rasterization and
shared render/hit rectangles. No completed 240-pixel viewport is scaled.

The measured matrix is 240x240, 410x502, 128x160, 160x128 and 502x240, with
touch rotations 0 and 180. Small layouts retain all pages, numeric ranges and
sliders, choice paging, both low-battery timers, timezone navigation and
Save/Cancel. At narrow widths the timezone Back control has its own footer row.
Content-fit checks require actual row/control/label space; these checks are an
application usability boundary, not a new display ABI minimum.

## Frame and input ownership

Logical selection reveal updates before frame readiness. Completion ownership
follows the actual submitted native app token, including sealed raster replay;
unowned Quick Controls/editor overlay frames cannot reset Settings input.

The first/cross-page frame records the raw provider sequence watermark using
the existing reset contract. This rejects contacts captured before visibility,
even if a synchronous display wait queues them for later dispatch. An already
accepted gesture may continue only with compatible page, control and contact
identity. Same-page slider/scroll and identical redraws preserve its ordered
MOVE/UP; each completion records the authoritative sequence and rejects distinct
new starts at or before that watermark. No timestamp epoch or minimum timestamp
is assumed. Physical Back, runtime exit and cancellation remain available.

The preserved 1.4.0 development release is known-limited: its dispatch-readiness
check allowed queued previsible touch after delayed synchronous presentation.
`capture-boundary-baseline.json` retains that exact-source witness and the
subsequent over-cancellation counterexamples. The successor fixture covers
stale/fresh/held contacts, explicit async drain, overlay token ownership,
snapshot predecessor/successor order, contact replacement, cross-page/reentry,
slider/scroll/unchanged-frame continuity, zero raw timestamps and millis32 wrap.
Tests also retain immutable pending-frame byte checks and zero unintended writes.

## Explicit current complete recipe

`scripts/build_native_settings_qualification.py` selects the native Settings
1.4.1 profile and emits a `native-settings-1.4.1-dev1` software-qualification
bundle. Compiled version, manifest and build receipt use the same version.
Version availability was checked in `version-availability-1.4.1.json` before
selection. The previously published `native-settings-1.4.0-dev1` and its assets
remain at their original source; the successor receives its own qualification.
It retains time/alarm synchronization, Quick Controls, radios, Contexts,
broadcast, motion, sleep, low-battery timers, atomic preferences, catalog and
foreground ownership. Source selectors are validated against all 16 declared
capabilities; a subset cannot silently receive the complete manifest.
The genuine full source manifest is
`lib/PortableApps/profiles/native-settings-manifest.json`; the builder verifies
its equality with the selected version and capability recipe before compiling.
It does not replace the legacy `Apps/settings.json` source identity.

External source pins are part of the builder and resulting receipt:

- Runtime: `48229bd1593a2e24e95bf4537b495ee615990fd3`
- Generic Watch bridges: `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`
- Profile/recipe source: `b9862e1379813a0914d4926ef59a3feb6fcf656a`
- Alarm utilities: `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d`

The generic bridge pin provides capability-backed power navigation and complete
native-sized RGB565 frame retention. Later product-specific Home helpers use
fixed-240/private-PMU behavior; they are not interchangeable bridge inputs.
The build verifies every external compiler dependency against its pinned Git
object, excludes host fixtures, validates the Xtensa ELF, bounds imports and
exports, records the compiler and full input hashes, and preserves available
license notices. Missing repository-root license notices are recorded rather
than inferred from bundled third-party licenses.

The manifest requests generic capability names and API versions. Installation
still needs a compatible loader, the listed providers and deployment-owned
instance bindings. Shared preferences use namespace 1 and the selected RTC
policy is fixed UTC+08-to-America/Denver. This artifact is neither a firmware
image nor proof of physical compatibility.

## Retained delivered baseline

Wave 0.1.59 retains Settings 1.3.25, 157124 bytes, SHA-256
`a7558b3bd2cd0c2a0edee95e4a8a69477a63772e8300e33e8e7332a2558f8d41`.
Its public mapping supplies the authoritative 16-capability feature boundary.
The exact original Wave build command has not been recovered. This migration
constructs and qualifies a current explicit recipe; it does not claim to
reproduce that historical ELF. Wave firmware and its retained app are unchanged.

## Reproduce

The complete command list and exact dependency checkouts are in
`.github/workflows/native-settings.yml`. Core host suites are:

```sh
python scripts/test_native_settings.py --output build/native-settings/layout
python scripts/test_native_settings_ownership.py --runtime ../runtime --output build/native-settings/ownership
python scripts/test_native_settings_capture.py --runtime ../runtime --output build/native-settings/capture
python scripts/test_native_settings_capture.py --runtime ../runtime --output build/native-settings/capture-snapshot --snapshot
python scripts/test_native_settings_timers.py --output build/native-settings/timers
python scripts/test_native_settings_timers.py --output build/native-settings/timers-snapshot --snapshot --case 7
python scripts/test_native_settings_features.py --system . --output build/native-settings/features
python scripts/test_native_settings_bridges.py --system . --bridges ../bridges --runtime ../runtime --profile-source ../profile-source --output build/native-settings/bridges
python -m unittest discover -s tests -p test_native_settings_build.py -v
```

The workflow additionally compares six native-UI-off target ELF profiles
byte-for-byte against the qualified `fd9a028c5d38eb517c1328b869684c4330a5d4d9`
baseline, then builds the complete native target. Existing Nova, paper/X4,
timezone, RTC/alarm and shared typography/geometry workflows remain independent
regression gates. Receipts contain actual case results and source hashes;
software success makes no hardware qualification claim.

## Allocation and persistent publication

The preserved 1.4.0 native ELF is 325,660 bytes, compared with the retained
157,124-byte file. These historical measurements do not describe the 1.4.1
successor; its exact ELF receives a fresh `memory-admission.json` in CI.
File size is not resident memory: the selected section-loader requests 97,790
executable bytes and 148,321 data bytes including alignment allowance. The
246,111-byte section total is 43,536 bytes above the retained 202,575-byte
request. Export tables, allocator overhead, cache, stacks, runtime/providers
and dynamic app/frame/Quick Controls/sleep buffers are additional. A sleep
snapshot alone can require `2 * native_width * native_height` bytes.

`report_native_settings_memory.py` derives exact ELF section/program headers
and verifies data allocation/placement against the pinned production helper.
The pinned ordinary-app admission check accepts both exact native and retained
ELFs and rejects missing imports, unsafe lifecycle state and the wrong role,
normally and under ASan/UBSan. This is bounded structural admission, not
relocation, execution, device grants or a linked-device heap-fit test. The
retained comparison is in `retained-memory-comparison.json`; CI includes it as
`historical-1.4.0-retained-memory-comparison.json` and regenerates the native
report from its exact target ELF.

After a successful main-push qualification, the main-only publication job
publishes the write-once `native-settings-1.4.1-dev1` prerelease. Direct assets
include ELF, manifest, recipe, build/allocation receipts and SHA256SUMS; its
compressed archive includes the exact qualification evidence and notices.
Later main runs verify and skip an existing completed tag at its original
source. They never overwrite that release or represent new source as its
older artifact. A partial release requires explicit review before recovery.
This no-overwrite policy is enforced by the publisher; it does not assert that
GitHub's platform-level immutable-release setting is enabled.

## Native idle and ordered cancellation

The expanded successor check found a separate idle boundary at source
`53fef0d0`: an initial completion cancellation could precede a newly captured
DOWN at the idle deadline. Automatic sleep ran before the next edge reduction.
`idle-cancellation-baseline.json` retains the exact trace and CI witness.

Native Settings/Wi-Fi automatic idle now waits for ordered input progress and
resynchronization to finish and accounts for physical held contacts even when
a page boundary has made their gestures ineligible. The feature fixture keeps
the neutral cancellation pass explicit; it does not turn it into a DOWN.
Its 62 runs include held reset, no-contact cancellation, persistent failed-poll
and invalid-snapshot recovery, while ordinary idle/wrap/refusal cases remain.
The unselected-profile code remains preprocessor-excluded.
