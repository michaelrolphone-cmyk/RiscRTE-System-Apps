# Wi-Fi ordered-provider fixture failure classification

## Finding

Neither remaining fixture failure demonstrates a production input or identity defect.

- `queued-up`: invalid provider event stream. The diagnostic injector delivers a duplicate, newer UP before the already queued UP. The reducer correctly cancels the gesture and resynchronizes, suppressing inertia.
- `confirm-hidden`: obsolete completed-image/reveal-then-confirm expectation. Current confirm activates the current logical selection immediately. The fixture still expects the first confirm merely to reveal it and waits for a second confirm.

No production files were edited. Original evidence and failure counts remain unchanged: snapshot 72/80 and disabled 72/80. These failures are not reclassified as passing tests.

## Source binding

Source: `/workspace/shared/system-raster-union-qualification-20261010`, clean commit `c5661fb987e583443baf652d5acabc2d49250c98`.

Production, app, test, and script sources were frozen under `frozen/`. `source-sha256.json` records per-file hashes, original fixture/run hashes, staged SDK hashes, and the clean source commit. Production hashes were checked against the source root after copying; no difference was found. Compilation commands and binary hashes are in the two runs JSON files. Instrumentation is fixture-only; its diff is `instrumentation.patch`.

Selected hashes:

- `lib/PortableApps/include/PortableTouch.h`: `817d5bde4d3a3c4634512503d283d0c7e731744a65e743305038294b58205004`
- `lib/PortableApps/include/PortableTouchScroll.h`: `d19aba992818d60477ef0fea7882b73dd6c9c1bd1db76d4d0faf9acc2e64aebd`
- `lib/PortableApps/src/wifi_scroll.inc`: `afd681056c5aa0c8020117f96ce28a7630d61d7fad9463d77932ccac50fe7159`
- `lib/PortableApps/src/adapter.c`: `8515a1ae7d5c6c19ea42b0e2950a91f37a02c6ee3fa83cbab52cbb7e4b106579`

## queued-up trace

`instrumented-snapshot-paper-queued-up.log`, same result normal and ASan/UBSan:

1. Tick 954, elapsed 279: MOVE seq 200, timestamp 954, contact 1 at (240,177). The previous reducer watermark is seq 199. Gesture subsequently reaches offset 199 with Q8 velocity 255.
2. Tick 955, elapsed 280: the strict poll has already enqueued UP seq 201, timestamp 955, at (240,177), with provider snapshot contact_count=0. Queue head/tail is 200/201. The injector bypasses that queue and returns another UP at (240,166), assigning seq 202 and timestamp 955. Reducer is still at seq 200 / timestamp 954.
3. Tick 956, elapsed 281: the queue now delivers the older UP seq 201, timestamp 955. Reducer resync=1 and velocity=0; offset remains 199.
4. Tick 957: queue empty; reducer remains in resync until snapshot adoption. Tick 958: reducer adopts snapshot seq 202 / timestamp 955 / contact_count=0. Gesture remains cancelled, velocity=0.
5. Final assertion `sent_up && maximum_offset > 200` fails with max_offset=199 and release_offset=198.

Snapshot short has the identical ordering at ticks 949–953. Disabled paper and short have it at ticks 900–904. All four also reproduce under ASan/UBSan.

Cause is in the preserved fixture `ordered-wifi.c:67–71,115–118`: `scroll_next` injects before `logical_raw_next`, and the wrapper increments snapshot sequence independently. The normal queue independently generates UP on the 280 ms snapshot release (`logical_touch_queue_fixture.h:17`). Thus it emits two UP events for one release, out of order.

`PortableTouch.h` checks exact successor sequence and enters resync on sequence failure. `PortableTouchScroll.h` cancels on invalid/cancelled samples. This is correct fault containment, not dropped valid input.

## confirm-hidden trace

`instrumented-snapshot-paper-confirm-hidden.log`, same normal/sanitized:

- Tick 1576, elapsed 901, before first confirm: page 3 (scan), selected=0, logical identity=3, completed identity=3, logical/completed offset=326. Row 0 is offscreen.
- Next observed state at elapsed 902: page 0 (root), identity=4, offset=0.
- Tick 1776, elapsed 1101: fixture's second-confirm precondition sees page 0 and SSID `Network 00`; it aborts because `ordered-wifi.c:52` expects page 3.

Snapshot short first/second checks occur at ticks 1571/1771. Disabled paper and short occur at ticks 1522/1722; disabled completed offset is 324 versus logical offset 326, and the same logical row 0 activates correctly.

The production path is explicit:

- `wifi_scroll.inc:46–51`: validate selected index, reveal it, mark scroll dirty, return true. Comment specifies confirmation belongs to current selection even if unpainted.
- `Apps/wifi_settings_portable.inc:997–1001`: after that succeeds, call `wifi_activate(choice)` immediately.
- `Apps/wifi_settings_portable.inc:726–733`: selected open scan entry copies its SSID and returns to root.

The legacy fixture only sets expected_row after its second confirm and asserts the obsolete intermediate page beforehand. It fails before checking the actual successful first-confirm selection.

## Isolated controls

`control-wifi.c` changes fixture inputs/oracles only; production is byte-identical to the frozen build. `control-only.patch` contains the exact control diff.

- queued-up control modifies the coordinates of the single UP already appended by strict poll. No additional sequence is injected. Snapshot paper delivers UP seq 201, timestamp 955, at (240,166) after MOVE seq 200. It reaches max_offset=884 (paper) or 1007 (short); release_offset stays 198. All current queued-up assertions pass.
- confirm-hidden control sets expected_row=0 with the first confirm and verifies root page plus `Network 00` at the later checkpoint without sending a second confirm. Existing final identity, selection, cancellation, no-radio-connect, no-storage-write, cleanup, and ownership assertions still run and pass.
- `drag` remains a healthy control.

Coverage: snapshot/disabled × paper/short × normal/ASan+UBSan × queued-up/confirm-hidden/drag.

- Original fixture semantics with instrumentation: 24 processes, 8 drag passes, 16 reproduced failures, zero compilation failures.
- Isolated controls: 24/24 processes pass, zero compilation failures.
- No ASan or UBSan reports. Original-oracle sanitizer exits are their expected assertion aborts.

These 24 control passes are separate diagnostics and do not convert the legacy 72/80 matrices into 80/80.

## Artifacts

- `summary.json`: machine-readable classifications/counts.
- `instrumented-runs.json`: normal builds/runs and binary hashes.
- `control-and-sanitized-runs.json`: sanitized original-oracle and normal/sanitized controls.
- `source-sha256.json`: source/SDK/evidence hashes.
- `instrumented-*.log`, `control-*.log`: raw event and logical state traces.
- `legacy-*.log`: SCROLL_TRACE reproductions with the preserved original binaries.
