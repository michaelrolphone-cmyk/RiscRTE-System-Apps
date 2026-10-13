# Original Watch Settings native-height checkpoint

Baseline: local `8f831566c51f724955cdc475b4fd3088a913bb1b`, exact tree
`eeca7e88156b04e732b455b99c85f2a2579d7ee7` of published Settings 1.3.24
System source `3508092f135d428b5bbccee0546616d046938527`, whose parent is
`14762601b3fa7bbeeac5ed565a6825534bf74dca`. This is the original Watch/NOVA
lineage, not the divergent generic/main Settings presentation.

Add `PORTABLE_WATCH_NATIVE_HEIGHT` to the original complete feature recipe.
Display/touch grants must expose their native dimensions. Both axes scale by
native width / 240. Logical height is ceil(native height * 240 / native width):
240×240 stays 240×240; 410×502 becomes 240×294. No square proxy or vertical
stretch is used. The self-contained WatchLayout header uses exactly the same
half-open rectangle and inverse-touch rules as the shared Home geometry.

Settings keeps the original fonts, palette and control sizes. Extra vertical
space expands lists and scroll limits; footer buttons/status are bottom
anchored. Tap calibration, RTC/alarm synchronization, sleep, alert, low-battery,
Contexts and Quick controls remain compiled. Quick owns native-size underlays,
uses the same transform, and extends its panel, torch and close handle to the
bottom. Async rendering remains sealed, cooperative and single-token.

## Qualification

`python scripts/test_watch_native_height.py`: 190 cases passed, normal and
ASan/UBSan, 240×240 and 410×502, touch rotations 0/180. Covers the actual Settings
controller, extra rows/footer hits, edit/cancel, padded surfaces; Quick/radio
save/cancel, torch/alarm interruption, exact full-native background restoration,
repeated modal return; async ownership, crown exit and failed-provider retention.
16 frozen 240 Settings frames (eight pages in both time formats) are byte-identical
to the delivered source. Pure native Quick comparison covers every native pixel
against its logical render, including unaligned/padded and truncated surfaces.

`python scripts/test_settings_clock_change.py`: normal and ASan/UBSan passed;
verified RTC edit invokes the original alarm clock-change control once, and
incompatible/unconfirmed service states still block unsafe writes.

The original full-feature Settings compiler command plus the new layout define
builds successfully: text 124025, data 7712, BSS 82504 bytes (initial geometry
checkpoint; +1424 text and +32 BSS relative to the exact old recipe). No flags
were stripped. LeakSanitizer is unavailable under host ptrace; ASan and UBSan ran.
Physical panel/input/RTC behavior and deployment are not claimed by these tests.

## Required service closure

Geometry does not silently remove provider requirements. The original full
profile aborts startup without contexts.service@1, telemetry.broadcast@1 or
alarm.service@1. Retained alarm additionally needs haptic.effect and audio.output.
Do not merely remove manifest grants. Supply the exact provider closure or use
a separately qualified explicit unavailable-service profile. Wi-Fi/BLE and
motion capability availability also need honest presentation. No X4 Settings
persistence or time-source policy was changed.
