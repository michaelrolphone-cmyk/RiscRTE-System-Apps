# Fused four-neighbor icon coverage

The bilinear icon sampler resolves two source rows and four exact packed 2-bit
samples in one forced-inline helper. Out-of-range samples remain zero. Unsigned
coordinate stepping intentionally maps the `-1` edge's next neighbor to zero;
no invalid row/column address is dereferenced. Interpolation, opacity rounding,
`np_pixel`, checkpoint placement, layer order and output geometry are unchanged.
No cache, allocation, retained state or runtime/profile flag is added.

The selected Xtensa GCC `-Os` link removes four `call8 np_coverage` instructions
from each bilinear output-pixel iteration. The remaining `np_pixel` call and
its existing surface/geometry/blend/checkpoint paths remain. The selected full
Springboard ELF grows 92 text bytes with unchanged data/BSS/imports/exports.
This is static code-generation evidence, not a target-cycle or FPS measurement.

`python scripts/test_icon_bilinear.py --output <directory>` runs the actual
helper against an independent four-sample oracle for all delivered native glyphs
at all sizes1–256 and synthetic odd/even packed widths1–17. It includes all
fractional weights at edge/corner/last-row/last-column positions, zero and full
opacity, normal and ASan/UBSan lanes. GCC host and selected Xtensa builds are
qualified; no Clang executable was available for a separate compile.

Whole-controller panning tests preserve exact frames, delivered input and the
complete sequence of25 Runtime/provider callbacks. Eight interleaved measured
host `-Os` pairs after two warmups gave CPU-ratio medians0.981 horizontal,
0.951 vertical and0.894 diagonal; ranges were broad and included regressions.
Do not extrapolate these host results into measured device performance.
