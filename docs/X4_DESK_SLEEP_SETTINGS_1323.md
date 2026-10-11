# X4 resident Settings 1.3.23

This isolated System update starts at d37ca43e5c5bef76c000559bd97b390f8dc6e856. It does not alter the delivered .46 image, Springboard, the idle owner, capture inhibition, or desk presentation/settling.

The resident Settings build now selects the same seven face IDs and Points in Time default as Home. Its default/invalid labels come from the loaded face instead of hard-coded Segments text. Native resident Settings exposes both existing landscape direction records even without a local Quick Controls renderer. Existing namespace-1 records are read in place, with no migration writes or automatic reset; invalid/unavailable records are reported and only an explicit Save writes.

Time to sleep edits the existing sleep_idle record (5–3600 seconds, five-second adjustments). Existing arbitrary whole-second values remain readable; decrement clamps at five seconds. sleep_mode and sleep_deep remain untouched. The resident host already reloads these preferences at its policy checkpoint and remains the sole sleep owner. No local sleep helper, timer, power grant, or new capability is added.

The selected root list uses a 656-pixel viewport at logical y=112 on the 480×800 display, ending at the 32-pixel lower inset. Physical Home/Back remain active. The root on-screen Back footer is removed; explicit editor Save/Cancel remains. Completed-frame hit identity and bounded momentum are unchanged. All selected interaction submissions are checked for the production LOW_LATENCY intent. Seven face choices use measured existing Orbitron/Rajdhani controls in a two-column grid.

## Qualification

- Selected Xtensa ELF passes structural/import/export validation; 212048 bytes, SHA-256 e40c93e319f897df43bde6aefdaf346cd256d7125136bb3befeaccaafe075bcc.
- Actual selected Settings controller tests cover erased defaults, all seven existing/saved/reopened faces, both landscape choices, no-op saves, cancellation/Home, corrupt/unavailable records, failed write/readback, legacy/custom timer values and bounds, preserved unrelated records, and the bottom-row touch target.
- Actual asynchronous list tests cover drag/momentum, bounds, interrupted and replaced contacts, completed-image hit identity, nested return, Home/Back, and top/middle/bottom native rasters at both geometries and UI flips, normal plus ASan/UBSan.
- Legacy desk Settings: 254 executions passed. Existing low-battery policy/Settings regressions and twelve actual System/Runtime/Graph resident-policy modes passed, including capture inhibition, activity, dirty edits, refusal and retention. Thirteen build-contract unit tests passed.
- Existing actual Home catalog rendering/codec suite passed normal plus ASan/UBSan, including both landscape directions as exact pixel inverses.
- Fresh version reservation checked 105 live branch/tag refs (90 unique trees) and an exact-version PR search; no 1.3.23 claim was found. Coordinated with the integration owner.

Artifacts are separate development outputs under /workspace/shared/x4-desk-sleep-settings-target, /workspace/shared/settings-1323-tests, /workspace/shared/settings-1323-scroll-tests and /workspace/shared/settings-1323-visuals. Capability providers in the Settings UI tests are simulated; the resident-policy suite uses production Runtime/Graph. No hardware, product image, publication, or physical display-timing qualification was performed.
