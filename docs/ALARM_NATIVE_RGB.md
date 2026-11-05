# Opt-in native RGB alarm overlay

This source component continues the shared-overlay part of the native Quick
Controls task. Its base is merged System main
`fbf74c263721a5e48c3259e6551f1c195af58464` (Quick adapter PR124).

## Selection and ownership

`--alarm-native` in `portable_quick_build.py` adds only
`PORTABLE_ALARM_NATIVE` and an implementation-source receipt. It requires an
explicit `--alarm-client` and rejects sparse initialization. It adds no grants,
Quick selection, native-canvas selection, app defaults or version changes.
The C adapter rejects a selected native alarm unless `PORTABLE_ALARM_CLIENT`,
`PORTABLE_NOVA_UI` and `PORTABLE_NATIVE_CANVAS` are all defined.

The existing Settings `--native-ui --alarm-client --alarm-native` command
composition supplies these dependencies, with or without Quick. Tests capture
that real builder command and host-compile its adapter flags. This is not a
new target release or a claim that every app builder exposes native canvas.
Other builders need their own existing, explicit compatible composition.

`PortableAlarmLayout.h` owns capability-pixel rectangles. `alarm_native.inc`
draws those rectangles with existing native Nova glyph/pixel primitives.
`alarm.inc` uses the same rectangles for raw provider-coordinate hit tests.
There is no completed-view bitmap scaling and no new frame allocation or
service/provider call in the presentation helpers.

The service and modal state machine still owns exact-token acknowledgement,
Retry, loading/uncertain-output handling, input cancellation, Quick/torch
interruption and app-frame restoration. Existing controls are Dismiss/Back and
Retry; the service API here has no snooze operation. Their ordering is unchanged.
The paper draw/input branch returns through its existing path. Selector-off
preprocessed production tokens are compared against the base commit.

## Geometry and usability

Controls are anchored to the capability dimensions, stacked on ordinary
portrait/square screens and arranged side by side on sufficiently wide
landscape screens. Rectangles and hit boundaries are valid across the display
ABI (nonzero dimensions through UINT16_MAX); unsupported zero/overflow
dimensions fail the pure layout helper. ABI-valid tiny surfaces may omit text
that cannot fit a row. That arithmetic property does not establish usability.

Composed renderer/input tests use 120×150, 160×160, 240×240, 410×502,
502×410, 640×240 and 2048×240 RGB565 surfaces. Small-screen actual captures
show the complete uncertainty warning: `Stop unconfirmed` is used when
`Output stop unconfirmed` would be clipped. Long copied provider labels use a
smaller existing face and visible ellipsis; the provider's stored label is
never changed. Text rasterizes with the existing font pixels.

## Reproducible qualification

Run:

```
python scripts/test_alarm_native_selection.py -v
python scripts/test_quick_native_selection.py -v
python scripts/test_alarm_native_layout.py --output build/alarm-native-layout
python scripts/test_alarm_native_optout.py --output build/alarm-native-optout
python scripts/test_alarm_native_adapter.py --output build/alarm-native-adapter
```

The real-adapter fixture calls production `alarm_foreground`, with scripted
capability providers rather than a replacement controller. It covers native
canvas and native-height composition, Quick on/off, direct and immutable
snapshot rendering, 0°/180° touch mapping, and normal plus ASan/UBSan builds.
It checks pixel equality, odd-address/padded-stride guards, native hit centers
and boundaries, rejected gestures, exact/stale/simultaneous tokens, the loading
gap, blocked Retry/Back, uncertain output, delayed presentation, OOM fallback,
and Quick/torch interruption with app-underlay and brightness restoration.

The runner records every compiler `-MM` non-system dependency hash and rejects
source changes during its matrix. Actual provider pixels are captured as PPM
and lossless PNG. Pure layout stress results, composed renderer/input evidence,
and selector-off comparisons are separate receipts, not interchangeable claims.
The dedicated CI workflow also runs the existing alarm, raster-custody,
terminal-client, BUSY and frame-progress suites with pinned external SDKs.

This component does not qualify the separately stopped shared automatic-idle
correction, activate an app recipe, replace the shared Home implementation,
publish a release, or establish physical-device acceptance. Paper overlay
migration and app integration remain separate work under the full task.
