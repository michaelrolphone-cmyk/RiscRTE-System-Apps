# Watch list cadence repair

This isolated checkpoint extends the compact Watch canvas adapter, based on System `6ea07eb`. It does not alter the frozen Watch 1.1.6 recovery composition, the modern shared adapter lineage, app grants, or any installed UI feature.

## Reproduction and result

`python scripts/probe_watch_list_cadence.py` drives the real Settings controller/view/adapter with a wall-clock drag and configurable synthetic display-completion delay. The control retains synchronous presentation. With 240 ms modeled display latency its logical poll stalls reach 260 ms. The opt-in asynchronous path stays at the requested 20 ms logical cadence across 0, 16, 40, 80 and 240 ms delays. A fast 80 ms drag is reduced while the earlier display token remains active, including the final coordinate of a real UP event.

Snapshot-only contact disappearance does not invent a final coordinate. The touch sample explicitly distinguishes a real UP coordinate from its legacy start-coordinate fallback, preserving the existing drag/back/tap behavior. Both touch rotations are tested.

These are deterministic software measurements, not physical Watch frame rates. Rasterization still costs CPU time; no on-device raster timing or visual smoothness claim is made. No animation duration, input threshold, visual effect or UI feature has been removed.

## Explicit opt-in and ownership

Compile the compact adapter with `PORTABLE_ASYNC_RGB565` (or use `build_portable_settings.py --async-rgb565`). Initialization selects it only for a 240×240 asynchronous, non-retaining RGB565 display and a Runtime exposing the checked terminal retention suffix. Other displays and historical Runtime prefixes retain their previous synchronous path. The compile-time opt-in is a deployment assertion that the provider has the reviewed serialized ownership behavior; ASYNC_PRESENT alone does not establish that guarantee.

The path allocates one 115,200-byte private canvas. There is one drawing/sealed latest image and at most one accepted provider token. A new complete paint replaces an obsolete unsubmitted image. Only complete images are copied through a separately validated real provider lease. The actual Watch provider accepts FIFO, so the app never asks it to support MAILBOX and never reacquires while its accepted token is active.

Logical app images remain separate from physical completion. Alarm and Quick Controls save the newest complete app underlay, not a stale screen or an overlay. Quick close copies its underlay into the private canvas before freeing its temporary allocation; a newer app paint may replace that pending restore. Input and animations continue during healthy transfer ownership. Launch, native exit and teardown drain accepted work at explicit ownership boundaries.

The selected Watch panel driver drains each SPI row and releases its lock between healthy provider polls, permitting serialized storage/alarm and brightness calls without a mutable lease. Every provider-progressing yield is followed by status validation before normal input, capture or service work. A failed/unknown/refused status, timeout or invalid zero token invokes checked Runtime retention and prevents later provider operations, frees or grant release. COMPLETE observed after a delay is accepted before timeout evaluation. Drain has an iteration bound as well as a wall-clock bound. A Runtime that violates its owner-retention contract is never allowed to resume ordinary provider-polling yields.

## Qualification

- `python scripts/test_watch_async_canvas.py`: real adapter latest-image ownership, padded surfaces/damage, immutable provider bytes, mid-paint Contexts capture, modal underlays, Controls/alarm/crown, handoff, failure-after-yield and terminal retention.
- `UTILITIES_SOURCE=<qualified-utilities> python scripts/test_watch_async_stopwatch.py`: eight real Stopwatch cases from exact Utilities `fc0bdbbbc600ce891e60c44d091fce0586ade7d8`, proving crown/paint retention skips private-grant cleanup and later Runtime callbacks.
- `python scripts/probe_watch_list_cadence.py`: synchronous control and asynchronous cadence in normal and ASan/UBSan builds.
- Existing Settings, Nova, alarm, Controls/radios, Contexts, low-battery, broadcast, radio/audio, transition and crown suites are retained. Three stale fixture contracts were corrected: existing Contexts preference reads, the canonical Runtime retention offset, and disabled/four-hour sleep timer bounds. The old Contexts and low-battery failures were reproduced on unmodified `9c5dceb` before those test-only corrections.
- All 19 compact full-feature Watch clients compile with the pinned Xtensa 8.4.0+2021r2-patch5 toolchain, preserve their exact export sets and introduce no new imports. Loader-preserving compaction remains enabled. The complete 111-file store still contains all 25 apps and 30 providers and independently round-trips in 5,308,416 bytes with 27 free SPIFFS blocks.
- Sanitizer results distinguish ASan/UBSan from leak scanning. The cloud runner can reject LeakSanitizer under ptrace; when that exact error occurs the focused runner repeats only with leak detection disabled. Legacy retained-invocation fixtures also run with leak detection disabled because they intentionally preserve allocations. These are not passed leak scans.

No new product version, hardware acceptance, source-less binary substitution or flash operation is implied by this source checkpoint.
