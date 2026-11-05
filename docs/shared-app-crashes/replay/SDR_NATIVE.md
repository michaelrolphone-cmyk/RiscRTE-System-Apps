# Native SDR successor replay

The complete Wave and T-Watch recipe sets preserve the qualified crash repair.
Only `waterfall.elf` gains `PORTABLE_RF_NATIVE_CANVAS`; it contains both the RF
Spectrum and FALL views. The separate microphone `audio_spectrum.elf` stays
byte-identical. Each product retains its prior compiler flags and grants,
including Wave's atomic-preferences selectors.

The source commits are recorded in `inputs.json`: `r0` owns the shared native
raster/adapter and `r5` owns the RF controller and renderer. Every other recipe
retains its expected output hash. SDK aliases remain canonical symlinks so
GCC 8.4.0 sees one identity for `#pragma once` declarations.

The native path supports capability-reported dimensions from 160 through 1024
pixels on each side. Layout, glyphs, plots and touch coordinates are evaluated
at those dimensions. Bounded history remains 228 bursts and at most 438 rows;
there is no new full-frame allocation. Room and event recognition status owns
a dedicated strip below the plotted data and frequency axes.

## Repeatable qualification

From the pinned Utilities checkout, run:

```
python scripts/test_rf_native_canvas.py --system-apps "$SYSTEM" --output build/native-canvas
python "$SYSTEM/scripts/test_rf_native_raster.py" --output build/native-raster
python scripts/test_rf_models.py
```

`test_rf_native_canvas.py` covers 160x160, 200x200, 240x240, 410x502, 320x240,
640x240 and 240x320 with normal and address/undefined sanitizers. It exercises
pending-frame input, spectrum/FALL cursor endpoints, scrollable Controls,
last-row/editor reachability and plot-byte invariance under recognition labels.
The native raster test compares immediate drawing with snapshot replay and
bounds text replay to its glyph rows and at most one immediate glyph pass.

The producer's qualification report records the broader controller and product
profile results, including paired inherited fixture failures. Those failures
are retained explicitly rather than described as a clean full-scene pass.

Use `scripts/replay_shared_crash_apps.py` with the normal `--product wave` or
`--product twatch` arguments for complete 17- or 18-app replay. Final product
composition verifies only Waterfall, its version field and the cohort binding
change against official Wave 0.1.45 and T-Watch 1.1.36. Native Runtime binaries
and every unaffected store file retain their exact official predecessor bytes.
