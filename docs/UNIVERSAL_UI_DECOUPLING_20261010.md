# Input, model and display decoupling: qualified increment

## Selected source and use

The qualified X4 `.57` renderer source is System
`8a75862929e4e83c8f66b3d58cc1c36d44830c84` (tree
`2a2de2b8b7537a3bfd322a6c4771f28e51e774df`). The corresponding
Utilities helper is `35c2141c68668971dc524d98702109b00c740815`
(tree `7183b3f6ef553e204c8a990fef079d814b8e60c7`).

Select `--raster-snapshot` explicitly on the portable System builders and
Utilities resident builder. Custom wrappers using `resident_client_build.prepare`
set `raster_snapshot=True` on their existing arguments. The helper records the
actual define and renderer source hashes and rejects an unsupported System
source. It adds no capabilities, grants, roles or application exports.
The default/legacy builder selections remain available.

The 17 affected app ELFs were rebuilt from clean source against final native
Runtime `0f17a435f99d02d60ca50df1d1a51fcef123db89`. All 48 SDK files are
byte-identical to the qualified Runtime `fc05d35827fc0d51be749c6797be4b3a4426ba23`.
Every ELF passes structural, import/export and version checks. Commands,
source mappings and complete artifact hashes are in
[the target closure receipt](qualification/raster-selected-057.json).

GameBoy 1.3.24 retains its independent presenter and cadence; it does not link
this adapter and receives no raster flag. The standalone text host 0.1.3 and
scene presenter are unchanged. The shared keyboard's scene ownership/handoff
paths remain covered. The selected X4 display remains fast UC8279 0.1.13.
No hardware flashing, GitHub merge or Watch product switch was performed.

## Behavior and ownership

The existing immediate-mode drawing API records an immutable app-owned command
snapshot. The ordinary foreground poll replays bounded row work into owned
pixels and continues input/model/service work between polls. Command order,
text/image lifetimes, fonts, clipping, rotations, alpha, glyph coverage and
original return values remain intact. Unsupported icons keep their existing
failure return. No command is silently dropped on capacity exhaustion.

A software snapshot is not a completed display image. Logical scrolling,
selection, updater release identity and explicit install confirmation do not
require completed pixels. Pending display tokens remain immutable. Normal
service work never receives a mutable provider lease from the raster snapshot.
A completed offscreen frame is copied under one actual provider lease and
submitted once; there is no fabricated frame identifier.

Two measured final-boundary defects were fixed during qualification:

- Launch settles its final software frame before checking the existing retained
  transition guard. A valid rapid tap no longer becomes a false launch refusal.
- RGB handoff time starts when logical drawing starts, as in the immediate
  renderer, rather than restarting when deferred rasterization finally acquires
  a surface. Delayed frames use the current elapsed animation phase.

Ordinary model processing does not wait for these ownership boundaries.
Sleep, scene/keyboard transfer, final launch and teardown still perform their
required checked settling. Interrupted Quick Actions, alarm restore, direct
framebuffer access and retained terminal custody keep their compatibility paths.

## Qualification

The following are host/model tests and target compilation, not physical timing:

- All 21 independent-build host commands pass with snapshot both enabled and
  disabled. Full NOVA qualification preserves 112 cases and 19,000 motion
  trajectories; logical frame cadence and launch intent are checked separately
  from provider completion timestamps.
- Selected Settings: 312 executions in each renderer mode, including sanitizers,
  two orientations and two display heights. Wi-Fi paper: 354 executions and
  portrait/native pixel identity. Current ordered logical scrolling covers
  Settings, Time Zone, Wi-Fi, Files and both Springboard modes.
- 2,028 full-buffer primitive comparisons cover 20 drawing families, clipped and
  offscreen rows, fonts, alpha, rotations and padding. Dense complete rasters
  match immediate output. Shared controls compare 68 common captured frames
  byte-for-byte; the real Runtime suite covers 64 control cases.
- 28 snapshot custody cases cover direct/partial restore, interrupted modal
  state, scene handoff, allocation failures and terminal callback fencing.
  Sixteen handoff-origin cases compare complete pixels at 0/17/120/700 ms,
  including clock wrap and offscreen allocation fallback.
- Rapid input: 75 synthetic/actual GT911 runs preserve valid equal-timestamp
  reports, same/different contact identities and full 32-edge bursts. The
  production ordered-touch reducer is unchanged. Timestamp equality is not
  treated as report identity or as grounds to discard a valid tap.
- Actual Runtime/panel integration: 112 scenarios / 224 frames, normal and
  ASan/UBSan, covering SSD 0/17/2300-ms refresh, ordinary UC, and the selected
  fast.13 provider with retained settling and quiet maintenance. All frames have
  exactly one provider acquisition/submission; no writable lease crosses the
  measured Runtime yields or owner-loop returns. Same-source immediate SSD
  control matches all 24 frames' wire hash, damage, bytes, GPIO and transfer time.
  [Provider receipt](qualification/raster-provider-progress.json).

Provider-model maximum touch gaps are 9 ms for SSD/ordinary UC, 8 ms during
fast.13 frames and 11 ms during its settling stage. A requested 50-ms app wait
can finish several raster/provider stages before returning; maximum measured
owner-loop gaps were 56/54 ms. These counters do not claim every application
updates its model between every raster row or that model time was hardware time.

## Cost and memory

Five-repeat host CPU medians after row-loop bounding were 4.412 ms for the dense
X4 Settings raster (previously 263.254 ms), and 7.025 ms for all-graphics X4
(previously 159.092 ms). Watch-format medians were 1.822 and 2.210 ms. The
2.956-ms maximum-poll outlier is retained; this is not a hard 2-ms whole-poll
bound. Detailed unchanged-source comparisons are in the
[row-bounds report](PORTABLE_RASTER_ROW_BOUNDS_20261010.md).

Owned framebuffer payloads are 48,000 bytes for selected X4 MONO1 and 115,200
bytes for 240×240 Watch RGB565. Target GCC measures each command node as 184
bytes, or 200 bytes with clipping state. Image/Quick-state payloads are copied
separately. The dense host fixture's tracked incremental peaks are 49,800 and
117,000 bytes; these are not complete on-device app heap measurements.

Final frame copy remains finite synchronous work. Host samples were sub-ms;
modeled cost injection of 1 ms per 4096 bytes produces 11-ms X4 and 28-ms Watch
edge-to-model delays during that copy. Those injected costs are not physical
measurements. Direct-frame APIs and command/offscreen allocation failure use
complete synchronous compatibility rendering rather than truncating features.

## Explicit remaining scope

- The unselected legacy Reader panel 0.1.14 has a synchronous operation-deadline
  contract. Its full-timeout compatibility wait remains available. No selected
  X4/Watch non-async RGB provider was found; arbitrary future synchronous RGB
  providers are not claimed to become nonblocking without a polling contract.
- Direct framebuffer callers, allocation fallback and final frame copy retain
  the finite work described above. Physical target latency/heap measurement
  remains outstanding; no hardware timing claim is made.
- Shared RGB behavior and compact target compilation are qualified. The exact
  selected Watch Files recipe also uses its own RGB presentation unit and a
  partly recovered legacy preparation bundle. No complete selected Watch Files
  rebuild or recipe adoption is claimed here.
- Historical Files/Wi-Fi diagnostic suites still have identical failing
  expectations in both modes. They are retained as explicit coverage gaps,
  not counted as passes. The [consolidated app receipt](qualification/raster-app-matrix.json) separates those attempts
  from current passing app, logical-input, custody and pixel gates.
- Disabled target ELFs pass all build gates but are not claimed byte-identical
  to the pre-refactor binaries; internal helper factoring changes their layout.
  Versions and preserved behavior are qualified rather than hidden by a stale
  historical whole-binary assertion.

Historical CI failures and fixture repairs remain documented. Exact-head CI
must be read independently of this qualified target receipt; neither unfinished
CI nor old fixture expectations silently remove an API or feature.
