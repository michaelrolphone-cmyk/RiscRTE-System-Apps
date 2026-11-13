# Format-selected RGB565 primitive rows

The shared Nova renderer now uses row-local RGB565 address/clip/color setup for
icon coverage, translucent circle/rim pixels, caption backing and glyph rows.
The adapter's black-icon rows use the same helper. This is not a Badge-specific
profile or new provider capability. `PORTABLE_RGB565_ROW_WRITER` defaults to1;
explicit `=0` selects the independent original scalar path for qualification.
Only zero/one values are accepted. No builder flag is required.

The scalar `np_pixel` API and body remain authoritative and unchanged. MONO1,
recording and unavailable/unsupported destinations fall back to it. Standalone
single-pixel calls remain scalar. Other UI pipelines (including Settings'
separate native-pixel function) are not claimed to inherit this optimization.

## Boundaries and custody

The cache is row-local stack state, not persistent provider state. It hoists only
pure invariants: RGB packing, logical row clip, affine native address origin and
step. Empty/clipped rows return before affine arithmetic (including INT_MIN/MAX
y) but still preserve every original pixel count and checkpoint attempt.

Every512th pixel delegates to the **original np_pixel**, preserving count,
checkpoint, clipping, current format and post-callback write ordering exactly.
Only after it returns is row state refreshed. Thus a callback may change the
pixel pointer, frame, stride, dimensions, clip, rotation/flip or format without
using the old row address. Known failure/retention makes refresh inert. A changed
pointer/frame/format is also detected before any fast write. Other operations
that may call services must explicitly invalidate the row; the circle's solid
fill path does so. The remainder of that row uses the scalar path. Do not reuse
this helper across arbitrary callbacks without that invalidation contract.

The helper neither changes sampling intervals nor groups/skips budget-clock
checks. Layer command ordering, raster band filtering, alpha-zero counting and
alpha>=255 packing are preserved. It does not allocate, retain a frame, alter an
ABI, acquire authority or change surface validation contracts.

## Owning tests

```
python scripts/test_rgb565_row.py --output /workspace/.../row-evidence
```

Four lanes use the actual adapter and default selection: normal/sanitized,
with paper flip disabled/enabled. Each compares9,516 cases against scalar
np_pixel, all512 checkpoint prefixes and exact whole-buffer hashes at each
checkpoint. Tests cover alpha0/255/>255, clipping, rotation/flip, odd strides,
INT_MIN/MAX rows, layer/recording/MONO fallback and callback-induced pointer,
frame, same-pointer stride/dimension, clip, orientation, format and custody
changes. Callback providers are modeled, not hardware. Existing complete-image,
layer, handoff and custody suites supplement this scalar differential test.

Selected Xtensa module link comparison has no new imports/exports or BSS;
explicit disable reproduces the original module bytes. The default helper is
still an out-of-line per-sample routine: it removes repeated geometry/record
setup, not every function call. Whole-controller host CPU observations and
strict ELF/PSRAM checks are evidence only; no device timing/FPS claim follows.
