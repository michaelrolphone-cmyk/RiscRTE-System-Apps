# Shared native typography

This is an app-independent source prerequisite. It changes no app layout,
product version, firmware image, store payload or release asset. In particular,
the delivered Waveshare .59 bytes are unchanged. Hardware has not been tested.

## Selection and coordinates

Select `PORTABLE_NOVA_UI` and `PORTABLE_NATIVE_CANVAS` to compile the shared
native text, fill, rounded rectangle and icon entry points. Existing
`PORTABLE_RF_NATIVE_CANVAS` callers retain the same native selection through an
additive alias. No app, device, watch-height, font subset or color-only profile
is implicitly selected. Legacy 240-wide NOVA APIs retain their contracts.

`portable_nova_native_icon(x, y, size, name, color)` takes a native top-left
square with size 1..65535, matching the display ABI's extent. The selected
profile's icon keeps its aspect ratio and is centered in the square. Unknown
names and invalid sizes return false. A known completely clipped icon returns
true without pixel work. Pixel-center bilinear sampling uses the original
licensed `rpi_glyph` bytes directly into the capability-sized destination.
The uint8 source dimensions and uint16 destination size give a compile-time
uint32 sampling-product bound. No new 64-bit division helper is needed.

Native text retains the existing `rps_glyph` bitmaps, bearings and Q4 advances.
Faces 0..5 use their original sizes; face 6 retains the existing Orbitron 10
scale. Faces 4 and 5 keep their existing sparse time/digit and plus/minus
coverage; no new font coverage is invented. Unsupported ASCII bytes use the retained question-mark glyph. Text is
bounded to 96 bytes, and only complete advances within the requested width are
emitted. Compact fallback text keeps its original 5x7 glyphs, uppercase mapping,
6-pixel advance and 128-byte limit. Its offscreen coordinate arithmetic is now
wide and clipped before narrowing.

These are per-glyph native-pixel operations. They do not render a 240-wide view
and enlarge it. Glyph metrics and icon/license/source files are unchanged.

## Bounds and custody

Native glyph loops clip against capability dimensions and the current replay
band before sampling. Native pixels also respect an active scroll viewport.
Coordinates and endpoint arithmetic use wide intermediates; only proven visible
coordinates are narrowed. Font and icon rendering allocates no glyph or frame
buffer and uses the existing surface ownership/custody check. The existing
bounded command recorder can capture native icons without retaining a caller's
name pointer. Existing display submission and optional replay storage are not
replaced by this change.

## Reproduce

Run `python scripts/test_native_typography.py --output build/native-typography`.
The production adapter is compiled with generic native selection, the RF alias,
the reduced faces profile, watch-native glyph selection and active scroll
clipping, normally and under ASan/UBSan. The test supplies synthetic display and
input peripherals and checks:

- 128x64, 160x96, 240x240, 410x502, 502x410 and 640x240 destinations;
- all retained ASCII text glyphs/faces, fallback mappings, icon names, width
  clipping, tiny/large icon sizes and INT_MIN/INT_MAX coordinates;
- maximum-edge 65535-sized icon sampling, independent pixel-oracle equality,
  direct/band-replay equality, immutable icon command capture;
- odd destination alignment, guard bytes and stride padding, no glyph allocation,
  rejected names/faces/sizes, retained-surface failure and cleanup;
- synthetic RGB565, MONO1, rotated MONO1 and active viewport clipping.

The MONO1/rotation matrix exercises renderer compatibility on synthetic surfaces;
it does not claim that every dimension is a supported paper-app profile.
Rendered specimens are primitive samples, not app-layout acceptance. PPM sample
hashes and deterministic pixel receipts are retained in `evidence/`.

Existing geometry, native-raster, original NOVA, reduced-font, paper, transition
and input suites remain separate regression evidence. Host checks and target
compilation do not establish target performance or hardware acceptance.
