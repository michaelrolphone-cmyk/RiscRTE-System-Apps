# Native Springboard renderer qualification

The shared Springboard uses capability-reported physical width/height, native
contact coordinates, proportional layout and direct RGB565 rasterization. No
completed 240-wide view is expanded. The original Orbitron, Rajdhani and Font
Awesome glyph outlines are scan-converted at their requested pixel sizes.

## Scope and limits

- Native renderer/controller fixtures cover 1×1 through 2048×2048, including
  128×128, 240×240, 320×160, 320×320, 410×502 and 800×1000. Tiny frames prove
  bounded execution, not readability.
- Complete adapter qualification covers 160×160, 239×293, 240×240, 320×320,
  320×390, 410×502 and 800×1000 with the real 24-entry catalog and all deployment
  features. The unchanged adapter admission is 160..1024 per axis; legacy Quick
  Controls still needs its logical height to be at least 240. Standalone
  landscape/128/2048 fixtures do not expand complete application admission.
- Known-ineligible display metadata still selects the generic grid. Its physical
  contact and native vector icons are corrected; its compact bitmap text and
  fixed control dimensions remain. Other applications and Quick Controls are
  separate migration work.
- The controller's existing 40 ms frame interval remains: at most 25 requested
  frames/s. Host render times are not physical frame rate or transfer latency.
- Native icons cap at 96 physical pixels. Text design extent caps at 960, giving
  maximum 44 px clock and 60 px label em sizes. These are layout caps; outlines
  are rasterized directly, without enlarging a pre-rasterized glyph.

## Source and deterministic regeneration

`lib/PortableApps/fonts/native_sources/` contains the exact audited four inputs:
Font Awesome solid/regular WOFF2, Orbitron-500.ttf and Rajdhani-600.ttf. Existing
`LICENSE-FontAwesome.txt`, `LICENSE-Orbitron.txt`, `LICENSE-Rajdhani.txt` and
`SOURCES.json` retain provenance and SIL OFL terms. No font substitution is used.

```sh
python -m pip install fonttools==4.61.1 brotli==1.1.0
python scripts/generate_springboard_native_vectors.py
git diff --exit-code -- lib/PortableApps/fonts/native_vectors.inc
```

The qualified generator used Python 3.12.14, fontTools 4.61.1 and libbrotlidec
1.1.0. Python Brotli 1.1.0 produces the same decoded font and generated C bytes.
`native_vectors_sources.json` records all input/output SHA-256 values, exact
source revisions, glyph counts, encoding, and actual generator versions.
Environment identity may differ; generated `native_vectors.inc` must not.

There are 168 unique glyphs (30 icons and 69 uppercase/printable glyphs in each
text font). Lowercase maps to uppercase exactly as the presentation already did.
All supported printable punctuation and fallback remain. The command stream is
19,741 bytes, packed losslessly into 12,348 bytes. Decoding, 512 edges, 512 sorted
intersections and 128-pixel coverage rows use fixed bounded storage, no heap.
Quadratic/cubic subdivision is bounded to depth 8. Four vertical samples and
horizontal coverage use nonzero winding, preserving holes and overlaps.

The ELF retains `native-springboard-vector-v1`. A preliminary exact-flags target
ELF was 166,024 bytes, below the unchanged 180,720-byte store ceiling by 14,696
bytes. Final package admission/store and telemetry-enabled builds are qualified
separately; this is not a final published artifact hash.

## Correctness commands

Run from the System repository root. In ptrace-based environments only, set
`ASAN_OPTIONS=detect_leaks=0`; ASan and UBSan remain enabled. Fixtures also assert
allocation/capability custody explicitly.

```sh
python scripts/test_springboard_native_raster.py
python scripts/test_watch_native_springboard.py
python scripts/test_watch_native_springboard_features.py
python scripts/test_watch_native_springboard_transition.py
python scripts/test_watch_native_fallback.py
python scripts/test_runtime_app_catalog.py
python scripts/test_springboard_nova.py
python scripts/profile_springboard_full.py --bridges /path/to/pinned/bridges --output build/native-full --verify
python scripts/profile_springboard_full.py --bridges /path/to/pinned/bridges --output build/native-full-sanitized --verify --sanitize
```

The bridges checkout must be RiscRTE-Watch commit
`6e82fdcaec0b73ed700d2104cfb4d94f011c19d4`. Full-provider tests retain every feature
from `docs/t063/healthy-controller/command24.json`. Target-specific compilation
switches are adapted only to make the host fixture executable. No target feature
is removed. Source snapshots isolate profiling builds from subsequent edits.

Native raster tests include 28 normalized drag/friction/overscroll/settle paths,
exact per-pixel circle coverage, row guards, clipping, and all 168 glyphs at nine
sizes, 1..128 px. All 1,512 glyph-size cases repeat byte-identically; maximum
edge use is 432/512. All cases run normally and with ASan/UBSan.

The 108-case native suite retains metadata identity, both pages, exact physical
contacts, hit radii, motion, launch/return/re-entry, and ownership assertions.
Old 240-pixel-hash parity is deliberately removed because glyph sampling changes
image quality. Same-binary metadata variants must still have identical pixels.
The runtime-catalog test now compares current fixed-inventory and current
runtime-inventory renderers; the former partial-old-source/current-include
combination was not a valid historical build. All 184 inventory, stale-launch,
fault and custody cases remain, including exact current 23-entry frame parity.

## Native host profile

```sh
python scripts/profile_springboard_full.py --bridges /path/to/pinned/bridges --output build/native-profile --instrument --repeats 5
```

The five 240-frame runs at 410×502 measured median host CPU render 2,200.193 μs
(range 1,980.265..2,821.998), input-poll processing 6.395 μs and motion 0.940 μs.
The 240×240 render median was 1,029.432 μs. These are instrumented host
`CLOCK_THREAD_CPUTIME_ID` values, not hardware timings. Physical input-to-display
latency remains unmeasured here. Per-stage samples and spread are retained.

Completed pre-replacement measurements are archived, not rerun: the .43
410×502 model used 5,333,515 `native_watch_pixel` calls and 2,045,824 root calls
per 240 frames. Native replacement uses zero `native_watch_pixel` calls and
536,875 roots. The archived timing used a different host clock, so no timing
speedup ratio is asserted. Original-outline rasterization adds work relative to
low-resolution atlas sampling; no claim is made that quality improvement alone
solves physical slowness.

Within the same native replacement, batching explicit glyph checkpoints every
four rows (and the final partial group) reduces Contexts calls from 261,852 to
138,955 per 240 frames at 410×502, a 46.9% reduction. Pixels, motion hashes and
other call counts remain identical. Pixel loops retain their checkpoint every
512 operations. At most 8,192 edge visits occur between explicit row checks;
the actual corpus reaches only 12 simultaneous intersections. Native scheduling
and physical transfer evidence are recorded separately in the cadence work.

## Machine-readable reports and screenshots

- `qualification.json`: schema `native-springboard-render-qualification-v1`,
  admitted versus isolated bounds, test counts, caps and reviewed source hashes.
- `build/t064-render/native-raster/results.json`: `scope`, two normal/sanitized
  `checks`, and SHA-256 `frames` for direct PPM captures.
- `build/watch-native-springboard/results.json`: controller `checks`, frame
  hashes, metadata and intentional pixel-change declaration.
- `feature-results.json`: exact feature switches and 72 optional-absence checks.
- Full-provider `results.json`: array of width, height, repetition and
  `NATIVE_PASS` result, one for each admitted geometry.
- Profile `results.json`: one row per size/repetition; `*_us_per_frame` is the
  sum for that stage divided by 240, `*_calls` is the total call count,
  `pixels_hash`/`state_hash` are deterministic checksums.
- Profile `summary.json`: per-size repetitions, frame count, timer basis,
  per-stage median/min/max and deterministic hashes.
- `instrument_calls` indices: 0 Contexts capture, 1 token status, 2 monotonic,
  3 yield, 4 touch poll, 5 native pixel, 6 legacy scaled pixel, 7 square root;
  remaining slots reserved.

This directory retains final samples, reports and direct RGB565 screenshot
conversions. `before43-410x502.png` is the archived .43 profile frame;
`after-native-410x502.png` is the native replacement's corresponding frame.
The size matrix uses direct native controller frames. No screenshots are mockups
or post-upscaled views. Final product release qualification owns the published
source pin, complete store reserve, admitted ELF budgets and hardware results.
