# Shared adapter capability-sized geometry

This source change qualifies the admission and native raster foundation. Settings
layout and typography remain separate work. No profile is selected implicitly,
no app version is reserved, and no delivered product/release is changed.

## Source boundary

Baseline: System Apps `297c38619a867f6200cc2f605b6193e4394bdfa2`.

* `foreground_adapter_open.inc` rejected dimensions outside 160..1024 and selected
  native-height views with fewer than 240 logical rows, including landscape.
* `WatchLayout.h` also imposed a 2048-pixel physical/logical ceiling.
* Native round/fill primitives imposed a separate 2048-pixel extent ceiling;
  negative/extreme clipping and radius squares used overflowing signed arithmetic.
* The actual native raster already writes into the provider surface. This change
  retains that path, without a completed-view bitmap or pixel expansion stage.

The old T030 inventory URL is absent from this baseline. Qualification follows the
actual adapter, primitives, headers and tests listed here instead.

## Bounds and preservation

`PortableDisplayGeometry.h` checks nonzero dimensions against the existing
`uint16_t` touch snapshot dimensions, and checks row/frame/stride sizes against
both `uint32_t` display-surface capacity and `size_t`. This is an ABI bound, not a
list of known devices or an aspect-ratio policy. RGB565 and packed MONO1 use their
actual row sizes. Pitched surfaces still require complete rows, including final
row padding, as before. Unsupported formats and malformed surfaces are rejected.

Selected native-height views retain the existing width-scaled layout and matching
input transform. The 240-row minimum is removed. Unrepresentable logical input
heights are rejected, for example 1x274 has 65760 logical rows. The explicitly
legacy color/Nova 240x240 profile remains 240x240-only. Admission does not promise
that every legacy app's controls fit a newly admitted small viewport.

Native fill/round/text endpoint arithmetic and replay bounds are widened before
clipping. Native text uses exactly the same glyphs, metrics, faces and palette;
this is an arithmetic correction, not a typography change. Optional transition
and damage-cache size policies, grant ownership, frame custody, input providers,
app feature selections and release manifests are unchanged.

## Reproduction

Run from this repository:

```
python scripts/test_native_geometry.py --output /tmp/native-geometry/bounds
python scripts/test_watch_color_profile.py --evidence /tmp/native-geometry/admission.json
python scripts/test_rf_native_raster.py --output /tmp/native-geometry/raster
python scripts/test_wifi_watch_touch.py --output /tmp/native-geometry/wifi-normal
python scripts/test_wifi_watch_touch.py --sanitize --output /tmp/native-geometry/wifi-sanitized
python scripts/test_quick_native.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_portable_transition.py
python scripts/test_reader_paper_flip.py
python -m unittest discover -s tests -v
```

Target import comparison (GCC8.4 was used for the saved evidence):

```
mkdir -p /tmp/native-geometry-baseline
git archive 297c38619a867f6200cc2f605b6193e4394bdfa2 lib/PortableApps lib/NativeApps Apps | tar -x -C /tmp/native-geometry-baseline
python scripts/test_native_geometry_target.py --baseline-source /tmp/native-geometry-baseline --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc --output /tmp/native-geometry/target
```

The bounds suite exercises actual adapter lifecycle and native raster, with fake
peripherals. It covers 1x1, 128x64, 160x96, 240x240, 410x502, 502x410, 800x480,
1025x256, 256x2049, 4096x16 and 65535x1; byte/stride overflow; undersized and odd
unaligned padded buffers; INT_MIN/INT_MAX coordinates; clipped huge rectangles;
large radii; direct/replay identity; and explicit allocation/frame/grant cleanup.
Separate full adapter init/fini cases exercise generic, legacy 240-only and
native-height profiles, including extreme-aspect rejection and renderer/input
edge agreement. All are run normally and with ASan/UBSan.

The Wi-Fi footer case is retained because a previous native-height render/input
transform mismatch ignored visible buttons at 410x502. Existing Quick, handoff
and paper tests independently cover unchanged profiles and ownership behavior.

Host testing does not establish physical display/input behavior, device memory
availability for huge valid surfaces, or end-to-end app layout acceptance.
LeakSanitizer is disabled under the local ptrace executor, where it is unsupported;
ASan/UBSan and explicit owned-resource cleanup assertions are enabled. Target
object compilation is a separate check, not a linked firmware or hardware result.
