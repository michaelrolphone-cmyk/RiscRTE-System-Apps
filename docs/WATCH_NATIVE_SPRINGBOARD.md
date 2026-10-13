# Original Watch Springboard at native height

## Source and deployment

This is the original shared honeycomb controller and NOVA presenter, not the
non-Watch grid. `Apps/springboard_nova.inc`, its 19-entry axial lattice, glyph
rasters, motion model, focus sizing, launch pulse, page handling and catalog
order remain unchanged from original Springboard revision
`14762601b3fa7bbeeac5ed565a6825534bf74dca`.

Use the complete original Springboard build recipe and catalog, adding the
same `PORTABLE_WATCH_NATIVE_HEIGHT` flag for both Watch displays. No device
name, hardware identity or pin conditional selects the layout. The host
qualification invokes the exact same executable with several provider
geometries. The final device ELF/cohort build and hardware qualification are
separate deployment checks.

The committed `test/native_apps/fixtures/watch_springboard_catalog_23.c` is the
exact 23-entry catalog from the audited deployed Springboard recipe. This
change does not replace or reduce the catalog or any original feature flags.

## Coordinate contract

The provider retains physical screen and surface dimensions. The Springboard
controller alone receives a private copy of its app API with logical dimensions
`240 × ceil(physical_height × 240 / physical_width)`:

- 240 × 240 produces 240 × 240 logical coordinates
- 410 × 502 produces 240 × 294 logical coordinates
- 320 × 400 produces 240 × 300 logical coordinates

The original honeycomb remains centered, and its bottom name/page captions stay
anchored to the full-height bottom. Extra height is actual layout space. Both
axes use the width scale, including circles, glyphs, captions, edge fading and
hit coordinates. There is no vertically stretched square or letterbox.

The adapter's existing single physical-to-logical touch map is shared with the
native raster's inverse. Invalid physical contacts are rejected before they
can be turned into a live hit. The last partial logical row maps to the last
physical row. At scale one the original direct pixel path is retained.

## Retained transition and resource ceiling

The original retained RGB565 handoff is retained on 410 × 502. Under the shared
native-height flag only, `PORTABLE_TRANSITION_MAX_SIDE` increases from 320 to
512. The default profile retains the original 320 bound. The adapter already
uses this shared ceiling, so no lifecycle, submission or custody code changes.

The transition's outgoing and scratch allocations each remain explicitly
bounded at 512 KiB. On 410 × 502 each is 411,640 bytes, or 823,280 bytes combined.
This is additional to any separately selected canvas/alarm buffers. Existing
allocation-failure handling, size/stride validation, disjoint-range validation,
exact endpoints, duration and radius are unchanged. Larger panels retain the
existing bounded no-transition fallback rather than unbounded allocation.

## Verification

Run:

- `python3 scripts/test_watch_native_springboard.py`
- `python3 scripts/test_watch_native_springboard_transition.py`
- `ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_springboard_nova.py`
- `ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_portable_transition.py`

The first script compiles an isolated original 1476260 source archive and the
current native-height source. Normal and ASan/UBSan variants each run against
synchronous and asynchronous provider capabilities. Each native executable is
reused for all three runtime geometries. At 240 × 240, every captured frame is
pixel-identical to the original: initial view, both pages, pan result, page
change by touch, completed launch, and clean re-entry.

The production controller/presenter tests cover:

- All 23 entries on the original two honeycomb pages and every centered hit
- Original strict focus hit radius and original lower-ring focus behavior
- Real provider contacts through adapter mapping, drag without launch, and
  bottom-anchored page hits below the old 240-logical-pixel boundary
- Lower honeycomb content/hits in the added native vertical area
- Invalid/out-of-surface/multicontact cancellation and the last physical row
- Actual app loop launch, return, held-contact entry, drag, and re-entry
- Retained transition activation at both sizes, sync/async surfaces, padded
  stride sentinels and released display/touch/runtime resources

The second script extends only the existing test oracle's allocation/rejection
bounds in a build-only copy; its independent reference blur/mix is unchanged.
It checks 12,128 reference frames and 182 non-mutating rejections per normal or
sanitized run, including 410 × 502 and the generic 512 × 512 allocation ceiling.

On the managed ptrace executor, LeakSanitizer cannot inspect threads. These runs
explicitly disable leak scanning; ASan bounds/use-after-free and all UBSan checks
remain enabled, with explicit resource counters and pixel guards. This is host
qualification, not a measured hardware latency or on-device acceptance claim.

Generated reports and real-render PPMs are under `build/watch-native-springboard`.

For the full original feature profile, run:

`python3 scripts/test_watch_native_springboard_features.py /path/to/springboard-exact-build.json`

This extracts every feature switch and forced profile include from the audited
recipe, then runs the same native executable at both sizes with synchronous and
asynchronous provider capabilities, normally and under ASan/UBSan. All Alarm,
Broadcast/BLE, Contexts, Quick Controls, Low Battery, RTC, local navigation/sleep,
retained handoff and async feature switches remain enabled. The actual controller
and adapter initialize, render, accept touch, launch, return and re-enter while
Alarm/Broadcast/Contexts are explicitly absent and Runtime is healthy. Only
host navigation and sleep hook boundaries are test doubles; the test never
claims to exercise hardware sleep or substitute hardware services.

Completed capture sets may be stored losslessly in `verified-frames.zip`, with
`frame-archive-receipt.json` recording SHA-256 hashes verified after extracting
each member. The JSON render/parity results remain available beside the archive.
