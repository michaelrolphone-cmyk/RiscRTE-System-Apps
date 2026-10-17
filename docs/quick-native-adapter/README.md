# Independent native Quick adapter checkpoint

This checkpoint connects the existing native RGB565 Quick controller and rasterizer
through the generic foreground adapter. It is based on qualified System foundation
`fd9a028c5d38eb517c1328b869684c4330a5d4d9`. It is not an app release or completion of
the wider native Quick/shared-overlay migration.

## Explicit selection and ownership

Select `--quick-actions --alarm-client --quick-native` through the existing shared
builder helper. The new switch adds only `PORTABLE_QUICK_NATIVE`; it does not select
a foreground app geometry/profile, add capabilities, or alter an app default.
The foreground app must independently admit the provider geometry. Selector-off
paths retain their existing behavior. The native Quick core accepts dimensions
1 through 2048; a larger admitted display fails initialization with cleanup rather
than silently rendering or hit-testing in a mismatched coordinate space.

Generic startup initializes Quick with capability-reported width and height.
Touch dispatch passes provider pixel coordinates to Quick; cancelled top-edge
gestures replay their original coordinates to the foreground. The existing
`PortableQuickLayout.h` rectangles drive both rendering and hit testing. Controls
are individually laid out/rasterized at the target size; there is no completed
240-wide framebuffer enlargement. The original 240x240 raster remains exact.

Deferred RGB rendering records an immutable controller copy and bounded text.
Replay accounts for native-width pixels in its existing input-work budget. OOM
continues through the existing synchronous fallback. The new selector is rejected
with sparse-clock startup because that path has separate Quick initialization.

Shared Home is independently owned by Watch `apps/clock/crown.c` and
`apps/clock/launcher_touch.h`; delivered source
`416fa9f144fa5bd5471e6f4c9cefff21455c8361` already initializes native Quick and
routes native coordinates. No Watch source is changed here. Generic bridge pin
`6e82fdcaec0b73ed700d2104cfb4d94f011c19d4` must not replace that delivered Home.

## Qualification

Run:

```
python scripts/test_quick_native_selection.py -v
python scripts/test_quick_native_adapter.py --output build/native-quick-adapter
python scripts/test_quick_native.py
python scripts/test_quick_actions.py
python scripts/test_paper_quick_actions.py
```

The new adapter fixture includes the real production adapter with scripted display,
touch, preference and alarm providers. It does not include an app controller or an
idle-policy correction. It covers native-canvas and native-height app profiles,
direct and deferred rasterization, normal and ASan/UBSan runs, plus selector-off
240x240 compatibility. Geometries include 120x150, 160x160, 240x240, 410x502,
502x410, 640x240 and 2048x240; dimensions above Quick's bound are rejected cleanly.
Tests feed real adapter input polling/dispatch, exercise every tile action,
brightness and volume saves, torch closure/reopening, edge replay and dismiss.
They compare every rendered byte against the existing native Quick rasterizer,
including odd-address/padded surfaces and guards. Deferred state immutability,
allocation fallback, delayed presentation, actual modal close/underlay restoration
and resource cleanup are covered. The result manifest records compiler `-MM`
non-system dependency closure for every configuration, including adapter includes
and fonts; the generated fixture is recorded separately. System headers/toolchain
are outside that source closure.
Separate core tests retain tiny/extreme geometry and exact legacy-raster coverage.
The existing real-adapter regressions cover radio actions, modal lifecycle, paper
Quick and Home paths. Host mocks do not establish physical behavior or latency.

## Remaining integration boundaries

- Generic `alarm.inc` still contains fixed-240 RGB controls and touch conversion;
  that shared-overlay ownership needs a separate reviewed implementation.
- Paper Quick keeps its existing independent geometry model; this switch does not
  migrate the paper sheet, sparse-clock/resident Home or shared Home renderer.
- The stopped shared automatic-idle correction is excluded. Settings PR 122 at
  `53fef0d0` and Wi-Fi-only PR 123 at `08d52eda` are separate source candidates.
- Before any merge, explicitly review the union with current main and those
  candidates. Overlap is in `adapter.c` input, `foreground_adapter_open.inc`
  initialization, `quick_adapter.inc` snapshot admission, and
  `raster_snapshot_replay.inc` replay accounting. Preserve owner/token/input-boundary
  hooks; do not overwrite them with the fd9 foundation.
- Final app selection, combined-source qualification, release publication and
  hardware validation remain outstanding. No current release candidate changes.

## Published-main follow-on

The separate `union/` receipts document the subsequent clean merge with already-
published main `fd6bc6065f6e409ed2d2ea1858bd56ab6d9e2fb3`. The original receipts
above remain historical evidence for independent checkpoint `f3bb53c9`. Both
published parents are preserved; stopped unpublished corrections remain excluded.
