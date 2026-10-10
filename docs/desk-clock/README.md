# Portable six-face desk-clock renderer

The current renderer preserves the reconstructed Home unpadded-hour feature.
The original Reader oracle and `evidence/` captures below are historical and
remain unchanged. Current validation uses the separately identified
[unpadded layout evidence](evidence-unpadded/validation.json); see
[current layout qualification](#current-unpadded-layout-qualification).

Renderer/assets/tests only, based on System Apps
`269c27a71e0ad606061f7a653a96b24c0ae4bc35` (PR #68). This does not wire sleep,
change a shipped app, change settings or manifests, or alter the frozen X4
0.1.6 BIN or Watch composition. It adds no Runtime capability or native export.

## Public integration contract

Compile `lib/PortableApps/src/desk_clock_faces.c` into the future clock app and
include `PortableDeskClockFaces.h`. `portable_desk_draw_face` accepts a caller-owned
canvas with `context`, logical `width`/`height`, and one synchronous `fill_rect`
callback. The only face IDs remain those in `PortableDeskClock.h`:
0 Segments, 1 Sans, 2 Serif, 3 Minimal, 4 Railway, 5 Deco. The retained 80-byte
schema and type are unchanged. No renderer pointers/state are retained.

The renderer clears its logical surface to white, then draws the requested face.
It uses no heap, filesystem, timezone conversion, RTC, panel, sleep API or mutable
global state. A callback can forward to the existing app `fill_rect` after choosing
its actual logical orientation. Every delivered rectangle has positive area and
is clipped wholly inside the surface. A callback returning false stops subsequent
callback delivery; the function returns false and that partial frame must not be
presented. Null callbacks/canvases and dimensions outside `[240,2048]` return false
without drawing. Rectangles, source spans, scanline loops and line strokes are
bounded by those dimensions; tests impose a conservative 65,536-call ceiling.

Use validated local civil time from the caller's qualified clock/timezone policy.
`hour24` is 0..23, `minute` is 0..59, `use12_hour` is a Boolean (persisted format
0 means 12h; 1 means 24h). In 12h mode midnight/noon both display 12; a single-digit
hour has no leading zero in either format. Segments recenters and scales the
three-digit layout. Invalid or out-of-range time produces the same `--:--`
Segments placeholder for every face. Unknown face IDs select Segments, as Reader
does. Analog hands include the minute's half-degree hour-hand motion.

The caller then adds date, AM/PM and translated wake/set-time text. Those are
`DeskClockSleep.cpp` frame chrome, not part of Reader's face renderer, and are
intentionally not replaced with Nova7 labels here. Canvas orientation, background
chrome, local date/time conversion, language and font selection must also be
identical when reconstructing an old frame. The component only proves its face
pixels, not a whole future sleep-frame implementation.

For a future differential refresh, decode the validated record, use
`portable_desk_plan_frame`'s `previous_minute` with its recorded configuration and
qualified time conversion, rasterize the old frame on a fresh white surface,
then rasterize the new one on another fresh surface. Do not overlay hands on the
retained pixels. Only mark a frame presented after confirmed provider completion.
A rendering failure must discard the partial frame and must not advance the
physical-image checkpoint. Full-frame refresh policy, typed lifecycle sequencing,
retained-clock restoration and physical testing remain separate integration work.

## Exact source and assets

The frozen oracle is Reader
`34d8e694d89a1e72d8854403d8592c289fae3ddc`:
`src/util/DeskClockFaces.h`, `src/util/DeskClockDigits.h`, and
`src/DeskClockSleep.cpp`. Frozen C++ face/digit headers and the original font
rasterizer are in `test/fixtures/desk_clock_reader/`. The test builds this actual
reference rather than a second rewrite or a textual approximation.

[`SOURCES.json`](../../lib/PortableApps/desk_clock/SOURCES.json) records Git blobs,
SHA-256 and sizes for all original inputs, both exact original Noto Regular TTFs,
both complete OFL notices, the original generator, renderer and sleep-frame
source. Only the eleven raster glyphs `0123456789:` per family are imported;
there are no TTF files or on-device font rasterizers. The C conversion preserves
every original span and glyph metric, and is reproducible with:

```
python scripts/generate_desk_clock_assets.py --check
```

The source is MIT. The derived Noto numeral assets retain SIL OFL 1.1 and original
2022 Noto Project Authors notices in `LICENSE-NotoSans.txt`/`LICENSE-NotoSerif.txt`.
These notices should ship with any app package that incorporates these assets.

## Historical pixel-level adaptation and evidence

At the original Reader adaptation checkpoint, digital geometry/scaling/spacing
and all numeral spans were unchanged. For analog
faces, on-device `sin`, `cos`, `sqrt`, `lround` and floats are replaced by an exact
floor integer square root and a 181-entry Q20 quarter-sine table at half-degree
intervals, using signed 32-bit products and round-to-nearest, ties away from zero.
The maximum radius is 934, keeping the largest product plus bias below INT32_MAX.
No 64-bit arithmetic, compiler division helper or math library is required by the
renderer. Near half-pixel boundaries, Reader's float trigonometry sometimes
rounds an endpoint differently. These are explicit one-pixel adaptations, not
pixel-identical analog claims.

396 paired native 480x800/800x480 captures cover all faces at 16 representative
times, both formats, and invalid time. Segments, Sans, Serif and all invalid-time
rasters are byte-identical to Reader. Maximum changed pixels per full 384,000-pixel
frame: Minimal 70, Railway 257 (0.067%), Deco 184. Every black pixel in either
analog raster has a counterpart within one pixel in the other (bidirectional
Chebyshev bound). Golden SHA-256s additionally lock the precise portable output of every
case. Reader analog hashes record this host run; the oracle comparison allows
host-libm rounding variation within the same one-pixel/512-changed-pixel bound. This is software raster evidence, not physical display/current qualification.

- [Portrait: Reader / portable](evidence/comparison-480x800.png)
- [Landscape: Reader / portable](evidence/comparison-800x480.png)
- [23:59 in 12-hour format, portrait](evidence/comparison-late-480x800.png)
- [23:59 in 12-hour format, landscape](evidence/comparison-late-800x480.png)
- [Actual existing Nova7 assets alongside portable faces](evidence/nova7-context.png)
- [All 396 full-raster hashes and difference counts](evidence/rasters.json)
- [Xtensa ELF/import and raster validation](evidence/validation.json)

Contact sheets are nearest-neighbor half-size previews. Adjacent individual PNGs
are native-sized, lossless, 1-bit captures for each face/orientation at 10:08/24h,
23:59/12h and invalid time. Their PBM SHA-256s can be reproduced by converting back
to `P4\nWIDTH HEIGHT\n` plus MSB-first row bytes. The existing Nova7 paper clock was
inspected as a visual context comparison: its Orbitron/Rajdhani typography,
heavy tick ring, battery, grid and swipe chrome remain unchanged. It is not the
six-face oracle and its font assets are not substituted for Reader's Noto spans.

## Validation

### Current unpadded layout qualification

The current production renderer already implements the numeric-hour behavior
documented in [the Home reconstruction](../x4/home-0.3.17-reconstruction.md).
Comparing its one-digit layout directly to the older Reader formatter is an
invalid expectation: Reader pads 24-hour hours, and its one-digit 12-hour Segments
face retains four-digit spacing. The current implementation omits the zero in
both formats and centers/scales Segments to 22 columns instead of 29.
The qualified System source `8a75862929e4e83c8f66b3d58cc1c36d44830c84`
(local equivalent `bd98b2e14c8d4ac20e4075c03396b12d8480e8f4`) already contains
the same renderer bytes, SHA-256
`f76843e4e7d5607205328e491fdb5f0d21121728607b894d2a1c266630e19fc7`.
The failing `480x800-segments-0000-24h` comparison differed from historical
Reader by 20,992 pixels because it compared centered `0:00` to padded `00:00`;
it is byte-identical to the explicit current-layout reference.

`test/fixtures/desk_clock_reader/UnpaddedLayout.h` is an explicitly test-only
reference that reuses the frozen Reader segment and numeral primitives with
the documented single-digit layout. It is not represented as original Reader
source. The frozen Reader headers, input provenance, original 396 raster
records and original PNGs remain byte-identical. Production source and assets
are unchanged by this fixture correction.

Every normal and ASan/UBSan run now checks all 396 existing cases against both
references. The 44 intentional layout cases must differ from historical Reader
and match the unpadded reference byte-for-byte. All remaining 352 portable cases
must still match their original historical golden; the new reference must also
equal Reader there. Frozen Reader digital hashes remain checked for all cases.
Segments, Sans, Serif and invalid-time output still require exact pixel identity;
analog output retains the same bidirectional one-pixel/512-changed-pixel bound.
All 396 current portable hashes are separately locked, including the 44 changed
layouts. No face, orientation, format, time or invalid-time case is skipped.

- [Current 396 raster hashes and historical differences](evidence-unpadded/rasters.json)
- [Midnight / 24h, portrait](evidence-unpadded/comparison-midnight-480x800.png)
- [One-digit / 24h, portrait](evidence-unpadded/comparison-one-digit-480x800.png)
- [One-digit / 12h, landscape](evidence-unpadded/comparison-one-digit-12h-800x480.png)
- [Source-bound normal/sanitized and target validation](evidence-unpadded/validation.json)
- [Rejected historical-format mutation controls](evidence-unpadded/negative-controls.json)

New contact sheets label the test-only reference separately. Native-size PNGs
include portable, unpadded-reference and original Reader captures at midnight,
9:05 in both formats, the original 10:08/23:59 examples, and invalid time.
`--write-evidence` writes only `evidence-unpadded/`; it does not regenerate the
historical `evidence/` directory.

```
python scripts/test_desk_clock_faces.py
python scripts/test_desk_clock_faces.py --target-cc /path/to/xtensa-esp32s3-elf-gcc
python scripts/test_desk_clock_layout_controls.py
```

The mutation controls compile temporary source copies with padded Segments,
four-digit Segments spacing, and padded 24-hour Noto hours. The real raster gate
must reject each at its exact first differing case; production source is never
edited. These controls distinguish the correction from skipping historical
differences or simply relaxing pixel tolerances.

Normal and ASan/UBSan C11 builds run 17,280 full-day face/format rasters each, edge
and invalid dimensions, INT_MIN/INT_MAX time, unknown IDs, missing callbacks,
callback failure, repeated rendering and cold old-frame reconstruction through
the real 80-byte encode/decode/policy contract. The oracle harness alone is C++.
The production renderer builds as strict freestanding C11. Target witness GCC
8.4.0 compilation links with `--no-undefined`, no `-lgcc`/`-lm`, zero undefined
symbols and only the test app's `app_main` export; the real ELF structural
validator passes. The original historical witness was 27,152 bytes; the current
witness size and hash are recorded separately in `evidence-unpadded/validation.json`.
Neither witness is a distributable app package.

Regenerate reviewed PNGs/checksums only intentionally (requires Pillow):

```
python scripts/test_desk_clock_faces.py --write-evidence \
  --target-cc /path/to/xtensa-esp32s3-elf-gcc
```

The unchanged paper Clock, Watch-facing Nova UI/Springboard, portable apps and
policy suites passed separately, along with all 52 repository Python unit tests.
The paper Clock suite passed 98 normal/sanitized cases; Nova primitives, keyboard,
22 retained-modal cases and 19,000 Springboard trajectories passed. Compressed
complete logs are alongside the raster evidence.
No hardware was accessed.
