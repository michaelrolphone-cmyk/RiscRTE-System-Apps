# Shared edge-mask identity work reuse

Base: published System `fb868fe3bd474b9d0761251a115e5843be08b3fd`.

The multiplicative RGB565 edge mask maps black to black for every coverage.
The matched67-frame controller pan processed5,127,577 edge pixels, including
5,066,177 already-zero pixels (98.80%). Those pixels previously repeated
coverage computation, channel arithmetic and zero writes.

The shared mask now skips that identity operation after reading the pixel.
It also derives each physical row's affine origin/step once instead of
transforming each pixel independently. Existing alpha/corner/channel rounding,
row traversal, opaque-interior skip, image/layer order and every eighth-row
checkpoint remain. Surface/custody is checked after the checkpoint before
forming pointers. This is not a zero-filled-background assumption: arbitrary
nonzero pixels still receive the exact old operation.

No coverage table, layer cache, new allocation or per-app renderer is introduced.
Existing generic raster layers own complete RGB surfaces and immutable
refcounted generations; they do not provide transparent coverage caching.
Reusing a full RGB layer under moving objects would not preserve their blending.
The current facade still receives complete frames; this change does not claim
partial-display transport or change the completed-image input map.

## Qualification

- Original fb868fe mask and actual `native_point` are the pixel oracle.
-460 full-buffer comparisons, normal and ASan/UBSan:160–1024 dimensions,
  square/rectangular layouts, 90° rotation, flip, safe origin, padded strides,
  black/white/random/sparse backgrounds, partial replay bands and fault rows.
  Checkpoint sequence and interrupted partial buffers match exactly.
-10 retained-custody cases pass ASan/UBSan, including the new mask-row failure
  before its first write; full offscreen buffer unchanged and no later callbacks.
- Actual-controller sliced suite passes25 timing/fault/OOM cases,21 ordinary
  interactions and two completed-frame-map checks with exact first-frame pixels.

## Matched-workload host measurement

Eight alternating baseline/identity/affine-identity runs of the actual controller
and renderer use the same deterministic seven-second pan input. Each submits67
frames with603 icon commands and121 input events. All submitted-frame hashes,
command keys and input outcomes match. The first eight frames are warmup.
Per-row `clock()` overhead is present equally; these are host process CPU costs,
not Xtensa timing or measured device FPS.

Final exact-source mask CPU median28.6265→9.421ms (~67.1% lower), ranges
27.015–31.912 versus9.192–11.154ms. Summed measured primitive CPU median
62.9745→42.307ms (~32.8% lower), ranges58.672–74.371 versus41.157–50.946ms.
The earlier prototype run had a smaller24.8% combined median gain; its full
results are retained rather than selecting only a best sample.

Target compilation, integrated product tests and physical responsiveness remain
separate qualification. This does not establish40–60FPS usability.

## Remaining architecture findings (not implemented here)

- `NOVA_FRAME_MS=40` limits requests to25Hz even if rendering becomes fast. The
  nominal outer poll is20ms. A display-aware schedule is necessary for a higher
  target, but changing it alone cannot fix a raster/transfer-bound frame.
- Exact final-alpha icon cache keys must include glyph identity, dimensions and
  opacity; unrounded bilinear coverage needs18bits, not8. On the same published
  workload,16KiB LRU payload yields318/603hits,32KiB333/603; metadata costs extra.
  Per-command CPU attribution gives an **optimistic upper bound** of11.986ms and
  12.644ms saved, respectively (16.4%/17.3% of that measured primitive run), only
  if all work on hits vanished. A real cache must still blend/write/checkpoint,
  perform lookup, construct misses and honor OOM/retention lifetime, so actual
  benefit must be lower. No icon cache is selected without a net-cost proof.
- The previously examined2209-byte coverage table gave small/noisy mask benefit
  and remains unselected. It did not address the dominant identity-pixel work.

## Supported format entry proof

This mask has an RGB565 contract, not a MONO1 pixel implementation. Its static
begin/caption callbacks are exposed only through `np_view`;
`springboard_presentation_get` rejects non-RGB565 before returning that view.
Only `np_begin` originates pending mask work; replay can only replay the same
internally recorded command. The adapter selects its format at open and checks
that every acquired surface's format/stride/size match before exposing pixels.
The MONO1 Springboard uses the distinct paper presentation instead.

`test_nova_mask_format.py` runs the actual Springboard and adapter through36
ASan/UBSan scenarios: portrait480×800/60-byte stride and native800×480/100-byte
stride, each immediate and snapshot replay. Each provider buffer is exactly
48000bytes, not an oversized RGB565 backing. The test asserts the RGB getter is
NULL and counts zero mask entries while preserving the existing paper-frame,
launch and teardown assertions. All36 pass. No format guard that silently
removes paper graphics was added; product source remains byte-identical2349a6b.
