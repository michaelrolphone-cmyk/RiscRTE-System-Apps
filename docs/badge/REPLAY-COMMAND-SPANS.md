# Replay command span audit and repair

Base: System `c8933f4c0cedc9ec3bc95e65a8d108631c56f8a6` (shared clock reader,
round Settings and previously qualified cooperative raster retained).

## Changes

The immutable raster already traverses commands in layer order, one bounded row
at a time. `RS_NP_CAPTION` had no span case, so each caption repeated its fitting,
glyph-layout and backing setup on every logical display row. The primitive did
clip actual glyph/backing pixels to that row; this was repeated setup, **not**
466 copies of the full caption's pixel work.

`np_caption_measure` now supplies the same fitting and inclusive glyph bounds to
both drawing and replay metadata. Replay includes the backing's two-pixel
expansion, six-pixel feather and exclusive upper endpoint. Empty/space-only or
fully truncated captions have no pixel work. A separately recorded `RS_NP_MASK`
still runs over the entire surface in its original layer order.

The icon span is the conservative union for any scaled glyph height no greater
than requested size: `[y-floor(size/2), y+ceil(size/2))`. This does not assume a
square glyph, change bilinear sampling, or clip a glyph to an optimistic estimate.

The generalized audit also found an existing correctness mismatch: native Watch
`RS_NU_PIXEL/RECT/ROUND` used centered-240 metadata despite `nu_pixel` using
`WatchLayout`. Those command bands now use the actual `watch_layout_native_rect`
transform, including its clipping/partial final row and selected safe origin.
Legacy centered rendering is unchanged. This is not claimed as the physical
Springboard slowdown's cause (Springboard itself uses NP commands).

No scheduler, frame receipt, geometry/contact model, display lease, Runtime
callback, input sampling, allocation fallback or terminal-retention rule changed.
No additional heap allocation or per-command storage was added.

## All command kinds reviewed

`scripts/test_raster_command_coverage.py` checks the entire declared enum against
explicit dispositions (38 kinds including optional native RF commands):

| Commands | Span authority |
|---|---|
| FILL | Actual rectangle; native columns for rotated fills |
| ROUNDED, TEXT, ICON | Existing conservative primitive rectangle/font height |
| PP_TEXT | Whole selected font metrics and selected zoom |
| PP_CIRCLE, NP_CIRCLE | Center/radius inclusive extent |
| NP_ICON | Union over both centered size parities |
| NP_CAPTION | Shared actual fitted glyph bounds plus full backing feather |
| NP_PIXEL | One logical row |
| NP_MASK, QA_RENDER | Deliberate full-surface rendering |
| NU_PIXEL, NU_RECT, NU_ROUND | Actual native WatchLayout, or legacy center offset |
| NU_TEXT, NU_BUTTON | Deliberate full span: composite fitting/scaling |
| SV_PIXEL, SV_RECT, SV_FADE | Settings native-coordinate helpers |
| SV_TEXT | Actual glyphs plus union of round fitted font banks |
| SV_BUTTON | Actual button rectangle and glyph overflow bounds |
| WV_PIXEL, WV_RECT | Legacy centered coordinates |
| WV_TEXT, WV_BUTTON | Deliberate full span: composite fitting |
| SBH_TEXT | Actual selected font/string glyph bounds |
| QR_TEXT | Resident font/word union; full fallback when unselected |
| QR_ROUND, QR_ICON, QP_ICON | Existing conservative paper-coordinate bounds |
| BITMAP, LAYER_BLIT | Physical image height |
| LAYER_BEGIN, LAYER_END | Exactly one metadata operation |
| CUSTOM | Caller-declared row contract |
| NU_NATIVE_TEXT, NU_NATIVE_ROUND | Existing native RF helpers |

Unknown/future kinds retain full-height fallback, not an empty or guessed band.
Adding a known command requires updating the coverage audit. Original pixel
clipping remains authoritative inside every primitive.

## Evidence

`test_portable_raster_latency.py --command-spans` records actual production
commands then compares full-height execution with the span-limited dispatcher,
including patterned underlay, alpha, padding, offscreen/truncated/empty/NULL
captions and both fonts. Native 466 has 504 complete-buffer comparisons; compact
240 has 504 and MONO1 480×800 has 490. Native ASan/UBSan also passes 504.
The unchanged base reproduces a native NU rectangle mismatch at y=-9; the
new transform passes. This expected baseline failure is preserved separately.

Representative workload: 19 overlapping circles and size-40 icons, the complete
mask, `10:40 AM` and `STOPWATCH` captions, RGB565 466×466. Both exact base and
candidate produce identical complete-buffer FNV1a `af678402` (772,800 fixture
bytes, including padding/unused backing storage). The span repair changes
caption executions per frame **932 → 51** and icon executions **1159 → 760**.

Eight interleaved exact-base/candidate host runs, each with eight untimed warmup
frames then 128 identical measured frames, use `clock()` process CPU time.
Medians were **267.8865 ms → 235.311 ms** (about 12.2% lower). Baseline distribution:
min 240.047, max 283.375, mean 265.5221, sample standard deviation 15.2948 ms.
Candidate: min 221.072, max 256.226, mean 236.6815, standard deviation 11.9081 ms.
Earlier non-warmup samples are preserved too (about 7.6% median reduction).
Individual samples overlap and host arithmetic/cache costs are not Xtensa costs.
This is useful bounded-work reduction, **not** an 18× frame improvement,
measured device FPS, or proof that physical panning is now usable.
The row-count ratios must not be represented as frame-rate ratios.

The existing real-controller sliced suite passes 25 timing/fault/OOM scenarios,
21 ordinary interactions and two completed-frame-map checks, with exact
first-frame pixels. Its synthetic timing excludes some setup arithmetic and
therefore does not replace this host timing or device profiling. Terminal
custody tests include a new caption-row interruption and assert no subsequent
provider calls, submission, release or free after retention.

Required final product/target admission and physical timing remain integration
work. This source-only cohort does not relabel any published firmware.
