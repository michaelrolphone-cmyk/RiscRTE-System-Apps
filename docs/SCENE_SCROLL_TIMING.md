# Shared color-list rendering checkpoint

On the existing production scene host, a 240-row color frame required thirty
8-row paint calls. Contexts yields 10 ms on each loop, so that alone schedules
300 ms of work per frame before accounting for device CPU and display latency.
The test uses a 60-row checklist, the actual renderer and an explicit 10-ms
caller cadence. The successor uses bounded 32-row color slices: eight calls,
80 ms at that simulated cadence. Paper/gray formats retain eight-row slices.

Glyph and rounded-outline raster loops now iterate only rows intersecting the
current slice. This avoids scanning rows that the pixel routine would discard.
The logical document, current hit map, touch event queue, semantic dispatch,
mailbox selection and immutable in-flight frame remain independent of display
completion. No paging, event merging, dropped motion or input-rate limit was
introduced. Each color paint call performs more bounded work; physical input is
captured immediately after it. Device worst-case latency remains to be measured.

Run test_scene_scroll_timing.py against System crown checkpoint 2b9bac3 as the
baseline. One 100-frame Mac sample measured 632.40 vs 539.19 process CPU us/frame;
maximum single-call CPU was 84 vs 271 us. These host timings are descriptive,
not portable performance guarantees. The scheduled poll count is deterministic.
Final RGB565 output matched byte-for-byte. test_scene_decoupling.py passed 75
raster/cadence cases (formats, rotations, keyboard layers, ordered contacts,
60–200-ms typing cadence and simulated display delays through 2300 ms).
Shared component tests preserve direct finger-follow logical hit targets while
a display frame is pending. These tests do not measure physical touch latency,
actual LCD transport, radio coexistence, or light-sleep behavior.
