# Native Quick controls

Shared Home calls `pqa_set_dimensions(&session.ui, width, height)` after session
initialization and before input. Dimensions are capability values, each 1..2048;
input coordinates remain native. A visible panel or active contact prevents
changing dimensions. Preference/session/provider behavior is unchanged.

The existing `PORTABLE_WATCH_NATIVE_HEIGHT` build flag includes native Quick
rasterization and its font masks. Other applications omit these assets and
retain the 240x240 renderer. `pqa_set_height()` remains the legacy 240-wide
native-height adapter API. Native 240x240 uses the original raster path exactly.

`PortableQuickLayout.h` shares control rectangles between hit testing and
drawing. The shorter side sizes controls and typography; additional space
changes placement. Portrait and landscape layouts preserve all six actions,
brightness/volume sliders, optional Context control, and bottom dismissal.
Small dimensions are memory-safe; readable controls are tested from 120x150.

Native output is rasterized directly into one width-sized RGB565 scanline,
including rounded controls, panel animation, fade and torch output. The final
image is never scaled. The scratch scanline is 820 bytes at 410 pixels and
bounded at 4096 bytes. Existing licensed fonts are independently rasterized at
2x size and stored with 4-bit alpha (10,928 bytes). Individual glyphs/icons are
antialiased at their destination size. Regenerate masks with
`python scripts/generate_quick_native_fonts.py` (offline Pillow dependency).

## Verification

- `python scripts/test_quick_native.py`: plain legacy and native-height builds,
  normal plus ASan/UBSan. Native matrix: 120x150, 160x160, 240x240, 410x502,
  502x410, 2048x240; tiny/extreme guard cases 1x1, 1x2048, 2048x1. Covers all
  actions, sliders, cancellation/replay, dimensions, padded/unaligned surfaces,
  invalid strides/sizes, full/sliced row equality and exact native/legacy 240.
- `python scripts/test_quick_actions.py`: existing controller/session tests
  and 104 real-adapter normal/sanitizer/rotation executions.
- `python scripts/test_paper_quick_actions.py`: existing 124 paper/Home cases.
- `python scripts/test_quick_raster_rows.py --baseline BASELINE --output OUT
  --sanitize`: 271 complete-byte comparisons against the renderer from parent
  commit `99c62a065b8e3c5c14000bbaf2fb44e9222f1efb`.

Pinned Xtensa GCC 8.4.0 `-Os` object text/data comparison before LTO: Quick input
2,632 to 4,071 bytes; native-enabled Quick raster 14,903 to 33,159 bytes. These
are component object sizes, not a final Home ELF/store fit assertion. Hardware
behavior and final product packing need separate integration verification.
