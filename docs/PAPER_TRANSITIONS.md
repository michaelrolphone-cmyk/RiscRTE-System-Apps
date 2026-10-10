# Paper screen motion

The opt-in paper profile adds a finger-following Quick Controls pull-down and a
completed-image crossfade when Clock or Springboard enters. It uses actual MONO1
pixels. The existing Watch RGB565 transition stays on its own unchanged path.

## Selection

- `--paper-transitions` selects the paper Quick Controls translation independently
  of app crossfade. It requires `--quick-actions`; shared builders expose it.
- Clock and Springboard additionally accept `--paper-crossfade
  --paper-display-sdk <canonical-sdk-directory>`.
- The crossfade SDK directory must contain all four canonical display headers:
  `RiscDisplayOutputV1.h`, `RiscDisplayOutputPowerV1.h`,
  `RiscDisplayOutputMetricsV1.h`, and `RiscDisplayOutputSnapshotV1.h`.
- Those headers are copied together into an isolated `paper-sdk/include` tree.
  Build receipts hash the exact compiled prefixes and implementation inputs.
  A directory name is never treated as proof of a provider version or commit.
- Selected Clock and Springboard motion profiles use versions 0.3.9 and 1.7.6.
  Unselected versions and capability grants are unchanged. Other apps' version
  allocation remains part of their explicit deployment composition.

The qualified snapshot provider is X4 commit
`f1b70b367820be6cb7d16137c56ddaf261b7a590` (fast panel provider 0.1.1).
Its base display and power headers come from Reader commit
`aac8c06d3221139084acd0cfc64f7b0ba194a97a`. The exact four header hashes are in
[evidence/paper-transitions/qualification.json](evidence/paper-transitions/qualification.json).
The provider's 10 Hz / 100 ms values are scheduling hints, not a hardware result.

## Behavior and ownership

Quick Controls follows the vertical drag over a copied, completed foreground
image. Release commits opening after 32 logical pixels; an upward drag of
64 pixels closes an open sheet. Controls cannot activate at intermediate sheet
positions. Alarm cancellation takes precedence and dismissal restores the
foreground image. The paper motion uses the shared elapsed-time animation clock.

The app requests a copy of the physically completed outgoing image before
acquiring a drawing surface. An acquired surface's initial contents are never
used for the fade. Copy refusal, an absent/malformed extension, incompatible
format or allocation failure produces the ordinary incoming scene.

At 800×480 the outgoing copy costs 48 KB. Ordered 4×4 coverage changes each
changed pixel once from outgoing to incoming. Unchanged pixels remain unchanged.
The first submission already includes incoming pixels; it never retransmits an
alpha-zero old frame. The transition lasts 400 ms of app-side elapsed time from
the first submission, and the endpoint is byte-exact incoming content. A slow
provider skips intermediate coverage levels. Completion may take longer than the
400 ms envelope because the physical display owns that timing.

There is at most one outstanding frame. Input continues while it is pending,
and controllers keep the latest state for the next available draw. A later
launch selection replaces earlier feedback; expensive app admission follows the
completed feedback frame. Home/Back, Quick Controls, alarms and repeated slider
gestures preserve their existing ownership and input gates. After a modal
returns, a pending fade resolves to the current incoming scene. Sleep cancels an
app fade before drawing its own scene.

Completed snapshots seed the existing damage comparison. The first mixed frame
and later frames therefore use the ordinary partial-damage calculation; large
scene changes can still require the provider's full image update. The fade does
not request a clean/OTP waveform. Safe finalization releases its private copy;
uncertain native ownership retains the invocation and its allocations.

Cold boot keeps the real boot logo and can fade from its completed pixels into
Home. Sparse timer wake does not start a foreground fade or acquire a snapshot.

## Qualification

- 25 production Springboard/adapter cases, normal and ASan/UBSan: ordinary,
  700 ms async, dropped intervals, padded strides, no extension, malformed
  suffix, copy refusal, allocation failure, repeated begin, Back/Home,
  replacement launch, Quick Controls repeat/Back/Home, two brightness commits
  including 700 ms frames, alarm interruption, and retained display failures.
- 168 padded and odd geometries across all 17 coverage levels, with invalid
  input preservation and exact endpoints, normal and ASan/UBSan.
- 12 real native Home/Points controller cases cover reverse crossfade, fallback,
  cold boot logo and sparse timer isolation. Its final raster equals the
  unanimated Home raster.
- Quick Controls captures contain 31 actual frames in each portrait/native,
  normal/sanitized run, with exact background restoration.
- Regression checks passed: 124 paper modal/Home cases, 120 sparse adapter
  lifecycle cases, the Watch RGB565 reference/retained/eager transition suite,
  and 20 builder tests.
- GCC 8.4.0 target Clock and Springboard ELFs pass the Runtime structural
  validator and import/export allowlists. Opt-out Watch and paper adapter
  preprocessed tokens and linked ELF bytes equal baseline `32dcd49`.

[Home → Springboard](evidence/paper-transitions/home-to-springboard.png),
[Springboard → Home](evidence/paper-transitions/springboard-to-home.png), and
[Quick Controls pull-down](evidence/paper-transitions/quick-pull-down.png) show
actual rendered pixels. GIFs beside them are illustrative playback, not measured
panel motion. Physical X4 timing, ghosting and waveform quality are unqualified.

Reproduce the focused checks with `scripts/test_paper_transition.py`,
`scripts/test_paper_home_transition.py`, and `scripts/test_paper_quick_motion.py`.
Use their explicit SDK and source arguments; the scripts never publish an image.

## Combined plain logging and native Home proof

The `--stage-logs` fixture option combines direct stage statements, display
metrics, paper pull-down and crossfade. All 50 transition cases pass, and 296
captured PBM frames are byte-identical to their unlogged counterparts. The native
Home suite additionally supports `--tagged-alarm` and passes 28 cases with the
deployed API2 descriptor, 140/350 ms asynchronous provider fixtures, Home-top
opening, swipe opening, sustained inspection, swipe dismissal and interruption
of an unfinished app fade. The original 12-case unselected suite still passes.

[Native Home pull-down](evidence/paper-native-home-stages/native-home-pull-down.png)
and its adjacent GIF show the actual current fonts and Points UI beneath partial
sheet positions. The modal preserves its completed background; on close, Home's
existing dirty state clears pressed feedback and an interrupted fade reaches the
exact current Home raster. Diagnostic reads do not change controller state.
The fixture rejects preference writes, freezes pending pixels, and verifies
matching submission/completion logs. No production source adjustment was needed.

[Combined evidence](evidence/paper-native-home-stages/qualification.json) records
the tests and target artifacts. GIF timing is illustrative. These host fixtures
do not establish panel timing or ghosting. Target receipts were generated before
the proof commit and need a clean rebuild before product composition.
