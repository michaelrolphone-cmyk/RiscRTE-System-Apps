# Opt-in raster replay: row-bounded primitive loops

Isolated candidate based on `61ad11348f7278578c555d7c8c6af3d80c5fb064`.
`PORTABLE_RASTER_SNAPSHOT` remains opt-in. No builder, product selection, public
API, command ordering, allocation/custody policy, or publication is changed.

## Change

The command scheduler already skips wholly empty rows. Its primitive callbacks
still scanned entire circles, glyphs, rounded rectangles, masks, caption
backings, buttons and fades on each remaining row, rejecting most pixels only
inside `fill`/pixel functions. The new internal `raster_rows` helper bounds those
loops before coverage, alpha and glyph calculations. Original coordinates,
pixel guards, blending order, logical/native rotation and clipping remain in
place. Zoomed paper text selects every source row intersecting the band.

The helper is compiled only with the existing snapshot define and is inactive
outside replay. It also accepts the full-height band used by synchronous
materialization. Its widened arithmetic contains no 64-bit division and adds
no target runtime import. Quick-reference and legacy Wi-Fi-specific primitive
loops are outside this focused increment.

## Host CPU-clock measurements

The unchanged latency fixture was compiled with `-O1`. Five executions of each
before/after binary used `RASTER_REAL_CPU_CLOCK=1 RASTER_ONLY_ZERO=1`.
These are process CPU measurements on this shared host, not hardware latency.
The Settings scenario selects both `--all-graphics --settings-graphics`; the
all-graphics scenario omits Settings. Both select `--snapshot --native-paper`.

| Scenario | Before total replay median (range), ms | After total replay median (range), ms | Before maximum poll range, ms | After maximum poll range, ms |
| --- | --- | --- | --- | --- |
| Settings, X4 MONO1 | 263.254 (255.355–402.616) | 4.412 (4.344–9.181) | 2.262–8.680 | 0.501–2.956 |
| Settings, Watch RGB565 | 85.497 (81.214–120.186) | 1.822 (1.708–1.944) | 3.957–4.056 | 0.565–0.668 |
| All graphics, X4 MONO1 | 159.092 (120.678–221.475) | 7.025 (6.355–8.242) | 2.062–3.414 | 0.426–0.628 |
| All graphics, Watch RGB565 | 71.711 (62.679–75.495) | 2.210 (2.100–2.621) | 3.998–4.747 | 0.505–0.766 |

The X4 median replay reductions are 59.7x for Settings and 22.6x for all graphics.
The 2.956 ms optimized Settings outlier is retained. This does **not** establish
a hard 2 ms bound for a whole poll. The existing scalar scheduler budget,
allocation, final copy, submit and synchronous fallback behavior are unchanged.
A target timing qualification remains required.

## Verification

- Complete emitted bytes match the existing immediate Settings/native and
  all-graphics/portrait baselines, for RGB565 and MONO1, costs 0/1/2, with
  ASan+UBSan. Recompiled non-snapshot sanitizer output also matches both
  existing baseline pairs.
- New `--row-bounds` regression compares immediate primitives to one-row and
  seven-row replay bands. It covers 20 primitive families, 13 vertical
  positions including negative/offscreen edges, all text faces, scaled paper
  glyphs, alpha overlaps, source/background bytes and untouched padding.
  Native and portrait sanitizer runs pass 2,028 complete-buffer comparisons.
- Snapshot custody: 18 ordinary + 2 text scene + 7 terminal + 1 clipped-begin
  cases pass ASan+UBSan. These retain failure/cleanup/orphan/OOM assertions.
- Frame-delivery sanitizer fixture passes both formats and all four present
  latencies (60 touch and navigation cycles each, maximum synthetic lag 4 ms).
- Allocation-fault offsets 1/2/3/4/5/6/7/8/12/16/24/32 in the dense Settings
  sanitizer binary preserve complete baseline bytes in both formats.
- Unchanged Quick row renderer: 271 full-byte baseline/sliced cases pass
  ASan+UBSan.
- Opt-in Xtensa builds for Settings, Springboard, Files and resident Clock pass
  target ELF structure and import/export validation. No new runtime helper
  import is required.
- `git diff --check` passes.

The unrelated aggregate `scripts/test_portable_apps.py` stops at its existing
`RiscRuntimeV1.h` provenance-hash assertion before compilation. This increment
does not alter that header, its API, or the provenance gate; no aggregate pass
is claimed. No hardware, flash, live product cutover or publication was run.

## Reproduction

```sh
RASTER_REAL_CPU_CLOCK=1 RASTER_ONLY_ZERO=1 \
  python scripts/test_portable_raster_latency.py --snapshot --all-graphics \
  --settings-graphics --native-paper --output /tmp/raster-settings-cpu
RASTER_REAL_CPU_CLOCK=1 RASTER_ONLY_ZERO=1 \
  python scripts/test_portable_raster_latency.py --snapshot --all-graphics \
  --native-paper --output /tmp/raster-all-cpu
python scripts/test_portable_raster_latency.py --row-bounds --native-paper \
  --sanitize --output /tmp/raster-rows-native
python scripts/test_portable_raster_latency.py --row-bounds \
  --sanitize --output /tmp/raster-rows-portrait
python scripts/test_portable_raster_latency.py --snapshot --all-graphics \
  --settings-graphics --native-paper --sanitize --output /tmp/raster-settings-san
python scripts/test_portable_raster_latency.py --snapshot --all-graphics \
  --sanitize --output /tmp/raster-portrait-san
python scripts/test_portable_frame_delivery.py --snapshot --sanitize \
  --output /tmp/raster-delivery-san
for variant in '' '--text-scene' '--native-terminal' '--clipped-begin'; do
  python scripts/test_portable_raster_custody.py --sanitize $variant \
    --output "/tmp/raster-custody${variant}"
done
```
