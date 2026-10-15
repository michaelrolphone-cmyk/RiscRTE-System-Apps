# Home wake and dark Springboard handoff qualification

## Findings

Two separate defects are established by exact shipped source and modeled production execution. Neither proves that a particular hardware blackout was a crash.

1. Waveshare 0.1.43 omits `runtime.retained-wake@1` from both Home's manifest and its boot grants. The unchanged shared Home requests this API, then intentionally falls back to `RISC_BOOT_RESET` when it is unavailable. The later cold-intro branch consequently treats initial Home entry after a genuine deep wake as cold. The actual Home test reproduces intro replay for deep timer, GPIO/button and other causes. The product-only fix declares the existing API and grants instance 0. Cold power/reset animation remains; deep wake remains fast even with an absent or mismatched checkpoint because the native API reports the actual cause independently of payload recovery. Ordinary child return and light wake retain their existing paths.
2. The shipped full-feature Springboard never restores initial display brightness. Its actual controller and adapter complete six frames and a clean app launch with brightness 0 and no brightness call, for both 240×240 and 410×502, synchronous and asynchronous presentation. Native vendor panel opening really starts at brightness 0. Home permits an early launcher handoff before deferred provider promotion; native demand-retention tests confirm unpromoted release stops/reloads providers, while promoted handoff retains them. This provides a source-consistent early/late explanation for intermittent visibility, but matching the user's episode still requires device logs.

The new shared adapter restores the selected UI brightness once, after the first confirmed COMPLETE presentation and only when display metadata advertises brightness. Current Quick preview and torch state win; no settings write or provider-promotion timing is introduced. Later frames do not reset brightness. Failed submit, failed/uncertain completion, and failed unblank do not claim visibility. Retained lit handoff is never proactively blanked. The same source applies to native and legacy rendering.

## Exact source custody

`source-proof.json` binds full pins. The official .43 factory is SHA-256 `1a67657a58254e26f99f674e71a5d9742fb023633e16b7e412b4a0f13a73754a`; product `9db48cef`, runtime `a2c2c9fa`, and Springboard System `32639661`.

Shared Home at `87b17a3`, System dependency `99c62a06`, and Utilities dependency `cc56de34`, built with its original GCC 8.4.0 recipe (`--role default --preferences atomic --crash-volume 12`) and canonical normalization/compaction, reproduces the shipped 654,300-byte ELF exactly: `b805ddcfb80d90983eabb03022c84fc633f9529924c1bffda363825d4ad88b1d`. The production Home code was not changed for either repair. The policy-only candidate changes exactly `default.json` and `boot.json`, adds 110 encoded JSON bytes, and has 21 requirement rows and 24 grant rows within the selected 32-row bounds. Full admission against the exact .43 native ELF validates all 62 store ELFs with zero hardware/storage calls.

.43's native change from .41 is the catalog/atomic-capability dispatch repair; released application payloads were unchanged. The source defects above are inherited integration gaps, not proof that .43 introduced new sleep code.

## Earlier optimizations retained

The shared Home explicitly imported T-Watch `fb013597` (Watch 1.1.16 behavior) in `e49a3da`. Its `WATCH_FAST_WAKE` build, first-frame-before-optional-services ordering, per-phase deferred radio/broadcast/Contexts starts, input opportunities between phases, light-wake redraw before unblank, RTC reconciliation, and wake-key suppression are still present. Normal and ASan/UBSan actual-Home tests pass at both physical sizes with atomic preferences and crash reporting selected. The missing Waveshare product grant prevented correct deep/cold classification; T-Watch's existing product grant should be preserved, not duplicated.

Native `fe20777` imported wake source `267aab6d` and is an ancestor of the shipped runtime. Deep preparation/refusal rollback, native external-wake crown boundaries, and fast completed-wake-tap suppression remain. Native retained-wake, GPIO/AXP crown, deep-prepare and demand-retention tests were rerun successfully.

The `fstat` regular-file size path, bounded metadata scheduler checkpoints and session image-byte cache are retained. Rerun metadata tests preserve the documented byte/time/deadline bounds. Loader tests and cache tests pass; the cache replay reports five uncached opens/stat calls versus two cached opens/stat calls, zero seeks in both, and independent mappings. Earlier exact SPIFFS replay recorded 1,324 redundant size-query flash reads eliminated and 283 metadata waits reduced to 59 in its specific modeled store. Those are source-bound operation counts, not new timing measurements or a promise of a particular boot duration. Deep sleep still resets and runs the native boot path; light sleep resumes the existing invocation.

## Tests and reproduction

- `scripts/test_initial_frame_brightness.py --bridges <Watch at 6e82fdca> --output <directory>` and the same with `--sanitize`: 32 cases each, repeated with `--stage-profile` for the final diagnostic build (128 successful case executions across both compile variants). Dark/retained-lit opening, actual Quick preview/torch actions before and after first completion, repeated invocation, no-brightness capability, failed submit/completion/unblank; both sizes and both presentation modes. Temporary accessors expose existing private state for testing and are never added to production builds.
- `brightness-normal.json`, `brightness-sanitized.json` and `brightness-stage-profile-*.json`: exact case results. The existing async ownership/modal aggregate also passes all 206 real-adapter cases in normal and ASan/UBSan modes (async-canvas-aggregate.log).
- `blackout43-*.log`: unmodified .43 production controller/adapter reproduction with fresh dark provider; no sleep or crash was simulated to obtain the black screen.
- `home-missing-wake-grant-regression.patch`: focused extra actual-Home cases, applied only to its test harness. The shipped Home ELF was rebuilt before any production-source changes and remains byte-identical.
- `home43-*-atomic-tests.log`: complete normal/sanitized Home tests with the missing-grant reproduction, genuine cold intro and deep/return fast paths.
- `wake-policy-admission.json`: metadata, ABI and ELF admission against exact .43 native source and binary; no target instructions executed.
- Native test logs record retained wake, crown recovery, deep preparation, demand retention, metadata checkpoints, ELF loading and image caching.

## Smallest hardware evidence still needed

Capture the existing stage log across one successful swipe and one black swipe, before and after the crown is used. Preserve `RTE_SOURCE`, `boot begin reset=... wake=...`, Home's `WATCH_WAKE`/`WATCH_KEY` markers, `app launch-request`, provider stop/start or `providers retention armed`, Springboard entry, and `PANEL_BRIGHTNESS` lines. A new `boot begin` distinguishes a reset/deep boot from a running dark app. `RTE_RECOVERY` identifies crown-held or crown-exit-timeout emergency resets; sleep markers distinguish intentional sleep. A retained failure or panic/crash report is stronger evidence than the logo alone. Hardware visible-frame timing, repeated early/late swipes and actual light/deep wake remain unqualified here.

## User-provided SDR serial evidence

Message `Sentinel_0ab9330f3b6c8191b81e2d17cc18d365` provides a watchdog reset (`reset=6`, `wake=0`). This is a cold crash reboot, not a deep-wake sample, and is not attributed here to the watch-face swipe. SDR investigation remains separately queued with the parent.

The reported manifest preparation spans 722,558→4,851,868 microseconds, or 4.129310 seconds. Metadata is 4.583782 seconds; Home ELF loading is 620,383 microseconds; display-provider loading is 737,412 microseconds, including 577,327 microseconds for panel startup. An earlier Home trace reports first frame at 4,376 ms and services ready at 6,595 ms from its own Home elapsed origin. Nested intervals and different trace origins must not be added. These measurements show remaining boot/metadata costs; they do not negate the retained source optimizations or establish an instant deep wake.

The trace explicitly shows deferred provider mode and retained-wake unavailable, consistent with the source/policy findings. The repairs still require post-update physical validation. `user-serial-evidence.json` records the provided values and their attribution limits.
