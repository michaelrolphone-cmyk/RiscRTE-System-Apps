# T030 Waveshare Springboard drag

Baseline: System f74db7dda4d4a6d5fffa048a1e9384addab1540c, the shared
Springboard 1.7.11 retained in Waveshare T052 0.1.31 (product d9b00f4).
This branch changes the selected launcher to 1.7.12. It does not replace the
controller, honeycomb, icons, focus, geometry, motion, paging, launch, Home
editing, retained transitions, optional services, or ordered touch reducer.

Two independently reproduced costs are reduced:

* The edge mask skips its fully opaque center and already-black pixels.
  Fractional native-coordinate boundaries remain exactly the same.
* During the app's existing bounded input wait, an active async display token
  gets one-millisecond owner-task visits. The cooperative provider can transmit
  rows instead of waiting eight milliseconds between visits. Inactive/sync
  paths retain their prior wait cadence. Input still reduces one ordered edge
  per app pass; painting never submits a mutable image.

The native panel has 205,820 pixels versus Watch's 57,600 (3.57x). Its provider
sends at most 32 rows per visit, requiring at least 16 visits versus Watch's 8.
The deployed vendor backend sends pairs of rows as 410x2 windows. No native
transport, provider ABI, panel metadata, buffer geometry or driver is changed.

## Measurements and limits

`profile_springboard_drag.py` measures the actual controller and adapter with
ordered synthetic touch, native RGB565 buffers and 240 rendered drag frames.
Host CPU mean at 410x502: total 837.625 -> 494.775 microseconds (40.9% less);
mask/captions 599.158 -> 255.521 microseconds (57.4% less). Clear, circle/icon,
input/UI and copy/submission costs are recorded separately. The complete
rendered pixel hashes match before/after at both 240x240 and 410x502. At 240x240
render time was 413.108 -> 306.488 microseconds. Timing is informational and
machine-dependent; exact pixel equality is the correctness gate.

`profile_springboard_pipeline.py` links production launcher, adapter and frozen
cooperative display state machine. The product's Contexts-enabled 8ms baseline
wait slices are modeled without unrelated service mocks. Provider state-machine
bytes match Waveshare's current common/display.h except its nominal-refresh
metadata and trailing newline; the fixture explicitly reports unknown timing.
Ordered touch events, 100us row transport, CPU work per 512 raster operations,
and every-first/every-fourth-yield provider visits are explicit model inputs.
With zero modeled raster cost, completed frames over four seconds are:

| Provider visit | Watch reference 240x240 | Waveshare before | Waveshare after |
|---|---:|---:|---:|
| Every yield | 51 | 26 | 52 |
| Every fourth yield | 18 | 9 | 34 |

Maximum submission gaps change 250 -> 230ms and 448 -> 250ms respectively.
Full model results include expensive-raster cases, immutable-frame checks,
provider drainage, and normal/ASan/UBSan execution. This establishes a scheduling
bottleneck and improvement, not device frame rates, touch latency or final
hardware qualification. No serial or physical device was available.

## Regression commands

```
python3 scripts/test_springboard_raster_cost.py
python3 scripts/test_watch_native_springboard.py
python3 scripts/test_watch_native_springboard_features.py
python3 scripts/test_watch_native_springboard_transition.py
python3 scripts/test_watch_async_canvas.py
python3 scripts/profile_springboard_drag.py --output build/t030-host
python3 scripts/profile_springboard_pipeline.py
SANITIZE=1 python3 scripts/profile_springboard_pipeline.py
```

The older native/cooperative fixtures changed snapshots without emitting events.
They now publish matching ordered DOWN/MOVE/UP events and sequence watermarks;
the native fixture drains multi-contact cancellations one event per app poll.
The original 240 renderer remains the pixel reference. Full launcher feature
checks model optional-service absence, not real radio/alarm hardware.

Final firmware assembly belongs to the waterfall diagnostics thread
01a137dd-18c5-778b-92f7-1a848ba9e00b, preserving T052 .31 and clock checkpoint
3d82cee. Supply only the rebuilt Springboard component and versioned manifest;
this branch is based on the actual deployed launcher lineage, not the divergent
current generic System branch. Do not replace the full product with this tree.
