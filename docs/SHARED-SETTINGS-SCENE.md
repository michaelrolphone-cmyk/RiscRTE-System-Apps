# Shared Settings semantic application

This implementation separates Settings state, semantic presentation, Runtime
capabilities and deployment policy. `Apps/settings_scene.c` does not link the
legacy display/touch adapter or select device names, dimensions or rendering
backends. The same `settings.elf` consumes `ui.scene@1` and an explicit
`settings.policy@1` provider. No existing product's catalog or firmware is
silently replaced by this source addition.

## Ownership and preserved state

- `PortableSettingsModel` owns preference codecs, confirmed state, drafts,
  explicit Save/Cancel/Discard/Reload, uncertainty and retained operations.
- `PortableSettingsScene` owns semantic routes, choices and intent validation.
  The full 419-entry timezone catalog is grouped by stable region and initial;
  providers paginate it. The largest document has 64 nodes, below the 96-node
  contract. Timers display seconds while the stored record remains milliseconds.
- `PortableSettingsRuntime` owns checked namespace 1 grants, availability,
  legacy/atomic/preferences contracts, clock reads and alarm-aware native/RTC
  commits. Reconciliation explicitly releases/reacquires namespace grants.
- `RiscComponentSceneClient` owns checked scene custody and exact revision
  visibility. A copied update is not evidence of pixels: pending Save waits
  until its matching document is settled and semantic input has drained.
- The deployment-selected policy supplies supported features, defaults and
  clock/storage authority. It is not inferred from screen dimensions.

Existing stored representations are reused for format, timezone, sleep, alert,
face, flip, language, desk direction and idle/deep timers. All seven desk face
IDs are compiled into the shared artifact. Civil editing preserves DST gap/fold
decisions and distinguishes clock, metadata and alarm outcomes. Missing,
unsupported, unavailable, invalid, stale and unconfirmed values remain distinct.
Choosing a default or navigating does not write a preference.

## Source and reproduction

The source baseline is System
`77c427a75e5651f03119c059fcdfd36d23df8708`; the canonical published Runtime SDK
is `13eca114cb58469cf6004fafc3da84fbd9d29beb`. Supply a flat directory containing
that Runtime's `sdk/app/*.h` to `--runtime-sdk`. The verified Utilities commit is
`e817612532beb613376d69a502cd1e11b94ac8c4`; supply that checkout to `--utilities`.
Its tagged Alarm API and production alarm
service are explicit dependencies, not copied or fabricated SDK declarations.

`scripts/build_settings_scene.py --runtime-sdk SDK --utilities UTILITIES
--output OUTPUT` builds twice, compares SHA256, validates the actual target ELF
and records source/header provenance. `NATIVE_APP_CC` selects the compiler.
There are no device/profile CLI switches. The policy provider builder is
separate; see `Services/settings_policy/README.md` for mandatory JSON fields.

## Qualification

Run the scripts in `scripts/`:

- `test_component_scene_client.py`: endpoint custody and exact-visible-revision
  checks in normal and ASan/UBSan configurations.
- `test_settings_model.py --runtime-sdk SDK [--sanitize]`: codecs, all 419 zones,
  no implicit writes, atomic stale revisions, cancellation, retained custody and
  DST decisions.
- `test_portable_settings_scene.py --runtime-sdk SDK`: all semantic routes and
  the exact production document validator.
- `test_settings_scene_app.py --runtime-sdk SDK --alarm-include ALARM_INCLUDE`:
  real app/model/view with transport doubles, cancellation past a 32-event batch,
  presentation/input barriers, stale events, retry publication, controls,
  exit/reentry, retained custody and explicit namespace reload.
- `test_settings_scene_host.py --runtime-sdk SDK --output OUTPUT --round-side
  466 --round-inset 69 [--sanitize]`: actual published scene host, all 419 zones,
  native watch 240 square, paper 800x480 rotated 270, badge 466 square with 69px
  conservative insets, and generalized 300/330/400/480 round-safe viewports.
- `test_shared_settings_runtime.py`: actual Settings Runtime and production
  Alarm service, modeled clock/storage/broker ports; see its explicit CLI inputs.

The round metadata comes from Drivers commit
`c1221ed4a609adc610f77166b0e3bb2d38232922` on
`badge/bulk-continuation-2026-10-28`, board
`waveshare-esp32-s3-touch-amoled-1.75c/display-profile.h`. It is a distinct board
from the rectangular 2.06 product. The 466 raster and 69px insets are declared
source metadata. The insets are conservative mathematical bring-up bounds, not
measured visible bounds. The physical pixel rotation 180 is driver-owned and
is not substituted for scene logical rotation.

The existing scene host uses that conservative 328px square, rather than round
chords. Its compact typography and 40px rows/26px Back control do not preserve
the legacy round renderer's 44px minimum control criterion. Native framebuffer
rendering and absence of circle clipping therefore do not establish full round
modality presentation parity. This is provider-owned follow-up, not an app
geometry branch.

## Remaining acceptance gates

Host evidence is not physical acceptance. Product manifests must explicitly
register/grant the policy and compose the selected scene/display/input,
namespace and clock/alarm providers. Existing resident POLICY checkpoints can
reload persisted idle settings after settled child custody, but do not provide
an immediate save-time acknowledgement of physical effects. The supplied
read-only policy profiles do not fabricate live-apply success.

Exact deployment admission, selected device behavior, native input/visibility,
sleep timing, and end-to-end physical acceptance remain separate gates. Do not
infer all-app convergence or successful firmware installation from this single
application's repeatable artifact or host qualification.
