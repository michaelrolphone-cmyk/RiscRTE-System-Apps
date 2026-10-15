# Native Springboard input, scheduling, and catalog evidence

## Changes

- Poll wait accounting includes elapsed time inside Runtime's cooperative provider
  visit, not only the requested sleep. Requested-slice accounting remains as a
  second bound if the runtime clock is coarse or stalls. An expired poll still
  yields once. The change retains the existing one ordered touch edge per pass.
- Native Springboard consumes the exact oriented physical x/y of that same
  reduced edge. Legacy UI/Controls still receive their existing logical mapping.
  Controls reservation, replay, cancellation and release flags are unchanged.
  There is no additional queue or independent touch reducer. The static
  Springboard fallback retains the same physical input contract as its physical
  layout. Quick Controls replay origins are preserved in physical coordinates
  before final generic gesture reduction, while Controls itself stays logical.
- `PORTABLE_STAGE_PROFILE` adds opt-in bounded observation through the existing
  Runtime trace suffix. No per-pixel/event logging or synchronous USB transport
  is introduced. See `../STAGE_PROFILE.md`.

## Demonstrated wait defect

The focused async fixture assigns each owner-task visit 2 ms of provider work
followed by the requested 1 ms tick. A requested 20 ms poll previously took 60 ms
(20 visits). The corrected loop takes 21 ms (7 visits), including a uint32 clock
wrap test. Replacing only the wait accounting with its prior implementation
fails the new assertion. No display or optional service is disabled.

## Full native pipeline model

`profile_springboard_cadence.py` retains every deployment feature flag from the
healthy-controller fixture, its actual 24-entry catalog, Contexts/alarms/Controls,
atomic preferences, and the real local navigation bridge. The controller uses
410 × 502 physical coordinates. Display leases, submissions, status and bounded
poll work run the actual `panel_vendor/driver.c` provider. Backend transport is
a calibrated fake clock, not physical SPI. All other healthy providers remain
ABI fixtures. This does not measure active audio/RF DSP or on-device CPU speed.

The model charges only odd paired-row backend calls: 84, 250, or 1000 µs per
410 × 2 transfer. 84 µs is an illustrative nominal-bandwidth case near the
40 MHz quad-lane payload lower bound; it is not a measured hardware latency.
The other costs and 1 µs capture / 25 or 100 µs raster-checkpoint costs are
explicit sensitivity cases. Host CPU nanoseconds are reported separately and
must not be read as device FPS or added to model time.

The 84 µs case completes the logical input pass in average 18.849 ms, with
2.023 ms of cooperative provider work in rendering. The maximum poll is
21.096 ms. The prior wait accounting, on the same replacement native raster,
averages 40.908 ms polling. The newest-complete-frame mailbox remains bounded:
68 submissions for 120 logical renders in the corrected model, rather than
blocking each input pass to submit every image. Hardware transfer rate is still
bounded by physical panel/provider latency.

All six cases validate 60 native-coordinate contact samples, clean grant and
allocation teardown, and no catalog provider work during the 120-frame gesture.
Catalog provider counts remain the startup's 2 snapshots and 24 row reads;
ordinary app-name lookups copy the cached manifest. These figures do not imply
that publication changes or explicit refresh/launch paths are free.

## Regression coverage

- `test_watch_async_canvas.py`: async ownership, immutable leases, mailbox,
  cancellation, ordered input, optional service retention, clock wrap, elapsed
  wait accounting, and bounded/short-session telemetry; normal and ASan/UBSan.
- `test_watch_native_input.py`: 8 cases across 240 × 240 / 410 × 502,
  rotations 0 / 180, normal and ASan/UBSan. Checks top-edge horizontal replay,
  reserved-tap release, reset cancellation, actual Controls open/dismiss, and
  the next native drag, and generic fallback replay origin. A short native
  horizontal replay must not be misclassified as a Back gesture by mixing a
  logical start with a physical end.
- `test_watch_native_fallback.py`: 24 size/provider/metadata/sanitizer cases;
  each executes actual `app_main` right-column launch, edit → pin → done, and
  page-dot → right-column launch. Covers 240 × 240 and 410 × 502, sync/async,
  slow refresh, slow latency and retained-image metadata, normal/ASan/UBSan.
  This reproduced the prior 410 × 502 mismatch: physical (300,100) was reported
  as (175,58), selecting column 0 instead of 1. Both touch and static fallback
  primitive addressing now remain physical; 240 × 240 behavior is unchanged.
- Context adapter, optional-service adapter, and navigation-retention adapter
  scripts pass. LeakSanitizer is disabled only after observing its documented
  ptrace restriction in this environment; ASan and UBSan remain active.
- `test_watch_canvas_crown_exit.py`: all 44 real-adapter cases now pass normal
  and ASan/UBSan after the parent repaired stale `SOURCES.json` provenance.
  The header bytes are unchanged and were verified byte-identical to published
  Runtime `a2c2c9fa` `sdk/app/RiscKeyValueV1.h`. The original production
  `326396614def41f4a3b11c8c193c3ac1bbb75ab0` failed this same pre-build integrity
  condition. Only its stale receipt pin/hash was corrected, preserving the
  strict check. The verified actual SHA-256 remains
  `0e73bf29c402be030baf52f5b2b324f5b872549cdf58aa31f45b72bd70977179`.
  Evidence: `key-value-provenance-correction.json` and
  `crown-current-provenance.log` in the parent task's evidence directory.

Existing full-adapter admission bounds remain unchanged: RGB565, each physical
side 160–1024, valid legacy Watch layout, and logical height at least 240.
The legacy Quick Controls renderer/routing still requires that geometry.
Standalone native controller/raster tests at other sizes are not evidence of
full-adapter admission or feature coverage. A feature-preserving Controls
migration is required before lifting this boundary; no dimensions are invented
and no controls are silently clipped or removed.

## Reproduce

Use the real bridge commit `6e82fdcaec0b73ed700d2104cfb4d94f011c19d4` and the
matching .43 Wave `panel_vendor` source. The model source is now relocatable:

```
python scripts/profile_springboard_cadence.py \
  --bridges /path/to/RiscRTE-Watch --wave /path/to/RiscRTE-Waveshare \
  --output /tmp/native-cadence
```

Add `--old-wait` to replace only wait accounting on the same native raster;
add `--sanitize` for ASan/UBSan. The script preserves exact compile commands
and emits JSON plus logs. The archived JSON is one host/model observation,
not physical qualification. Production code and tests continue to fail closed
on custody uncertainty, and this evidence does not authorize publishing firmware.
