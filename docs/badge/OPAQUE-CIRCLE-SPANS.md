# Exact opaque-circle interior spans

Base: System `beaf89867f7e9385d4b78ad5df8213d9239f2fbe` (separately frozen
command-span and native-Nova metadata correction).

For RGB565 circles with opacity255, the integer-root extent of
`(radius-1)^2-y^2` identifies the completely opaque run on each row. That run
uses the existing exact RGB565 affine fill path. The original per-pixel
anti-aliased rim, its integer-root/alpha rounding, color conversion and drawing
order remain unchanged. No raster cache, extra allocation, changed image
identity or provider API is introduced.

Active clipping and Contexts compositions retain the original pixel path;
non-opaque and MONO1 rendering are also unchanged. Bounds and terminal/surface
checks surround every fill. The existing fill checkpoint-before-pixel behavior
is retained; no work resumes into the rim or next row after failed custody.
Offscreen rejected pixels need not generate the old incidental pixel-counter
increments; real input delivery, frame identity and failure boundaries are
qualified separately, rather than requiring historical polling counts.

## Qualification

- Actual prior `np_circle` is extracted from beaf898 as the pixel oracle.
- Native466:676 initial whole-buffer cases, then677 including active-clip
  fallback; final677 pass ASan+UBSan. All radii1..256, random RGB, alpha0/1/127/
  254/255, offscreen coordinates and immediate/seven-row replay are included.
- Compact240 and MONO1:676 initial cases each. Expanded selected rotation90
  fixture passes677 RGB plus677 rotated paper cases, including stride canaries.
- Terminal custody:9 ASan+UBSan cases, including the new circle-run failure
  exactly at its checkpoint before the first pixel. The complete offscreen
  buffer is unchanged and no later callback, release, submission or free occurs.
- Real-controller sliced suite:25 timing/fault/OOM cases,21 ordinary
  interactions and two completed-frame-map tests pass, exact first-frame pixels.

These are host/source gates. Selected target compilation, final product cohort
and physical performance remain integration evidence, not claimed here.

## Representative cost

Generated host instrumentation uses the real Nova controller and renderer with
an existing deterministic seven-second back-and-forth pan stream. Zero
synthetic pixel charge gives67 submitted frames,603 icon commands and121 input
events. Every submitted-frame hash, command key and input outcome is identical
between the prior pixel implementation and the circle-span variant.

Eight alternating host runs exclude the first eight frames from primitive cost
aggregation as warmup. Each primitive row has the same `clock()` instrumentation
in both versions; the cost includes that instrumentation, not just arithmetic.
Circle CPU median is **43.149 → 17.1785ms** (~60.2% lower). Ranges are42.271–45.696
and16.384–18.067ms. Summed measured primitive CPU median is
**115.2295 → 93.143ms** (~19.2% lower), with ranges106.168–126.230 and
86.376–102.945ms. Earlier all-frame measurements are preserved separately.

These timings are **not device FPS** or proof that physical Springboard panning
is acceptable. No serial/provider callback was added for profiling in product.

The separate2209-byte invariant-mask table experiment showed only a noisy3.1%
mask-only median change. It and the icon-cache key/capacity simulations remain
unselected evidence; neither is part of this source change.
