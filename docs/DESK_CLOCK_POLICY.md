# Portable desk-clock policy foundation

This is an app-owned, hardware-blind scheduling and payload slice. It does not
activate deep sleep or change any shipped Clock/Settings package. The frozen X4
0.1.6 image and Watch behavior are unchanged.

## Source parity

The behavioral reference is T5S3-Reader commit
`34d8e694d89a1e72d8854403d8592c289fae3ddc`:
`src/DeskClockSleep.cpp`, `src/util/DeskClockTime.h`,
`src/util/DeskClockFaces.h`, and `src/CrossPointSettings.h`.
No native firmware or privileged implementation is copied into this app helper.

- Stable face IDs: Segments, Sans, Serif, Minimal, Railway, Deco.
- Retain face, 12/24 format, language, flip, RTC UTC/variant/reference policy and
  the bounded 40-byte timezone string, alongside the last confirmed visible minute.
- First entry is full refresh. Timer resume uses the previous exact rendering
  configuration for differential updates, with a full refresh every 30 cycles.
- Advance the visible-minute record only after successful present completion.
- At most three frame attempts catch a slow refresh crossing the minute boundary.
- Re-read time after checked peripheral power preparation, so teardown cannot
  accumulate drift. This helper also asks for a bounded catch-up repaint if that
  teardown itself crosses a minute; failure exhausts the cycle instead of sleeping
  for another minute with a stale image.

## Contract

`PortableDeskClock.h` is pure C. The canonical little-endian payload is 80 bytes,
with type `PORTABLE_DESK_CLOCK_RECORD_TYPE` and schema 1. It fits the proposed
128-byte Runtime retained-wake capability. No native struct layout, pointer,
allocation, authority token, callback or continuation address is serialized.
The outer Runtime mechanism must check integrity and bind it to the actual
admitted app identity/version and cohort. This payload validates its own schema,
reserved zeroes, positive minute alignment, refresh range and rendering fields.
Timezone bytes must be terminated, printable and canonically zero padded;
interpreting timezone/language remains a renderer/time-policy responsibility.

A caller begins a cycle using an authorized classified timer wake and validated
record. A cold/button/other wake does not reuse that image. Configuration mismatch
also forces a new full frame. `plan_frame` returns old/new minutes and full/diff
intent. `presented(true)` is called only after checked display completion, never
merely after submit. Failed presentation cancels without advancing the checkpoint.

`prepare_record` must be called after checked panel/touch/storage/rail preparation
and a fresh qualified time read. READY returns a proposed record and a 1..60000 ms
wake duration, rounded upward by less than 1 ms to match the typed timer API.
REPAINT requires checked peripheral resume and another bounded frame attempt.
STOP requires normal recovery/UI; it never authorizes sleep. A button press can
cancel at any stage. The record remains a proposal until Runtime commits it at
validated terminal deep entry. Refused sleep, handoff or cleanup failure must
never publish it as a successful sleep checkpoint.

Time input is an epoch plus a valid subsecond part. The current rtc.clock@2 table
has whole-second calendar reads; a qualified retained/native time source or an
explicitly bounded conversion is still an integration requirement. The helper
never synthesizes subsecond precision from an unavailable source. Its 64-bit
minute calculation uses fixed 32-bit reductions, avoiding freestanding division
helper imports while supporting the full nonnegative int64 epoch range.

## Remaining product work

Runtime retained-record/native entry integration, typed panel prepare/resume,
touch/SD/rail/radio sequencing, alarm arbitration, minute-only boot graph,
qualified time restoration, six-face raster assets, settings/persistence and
physical wake/current/refresh testing remain separate work. A passing policy
suite is not a claim that the deep-sleep desk clock is available on a device.

Run `ASAN_OPTIONS=detect_leaks=0 python scripts/test_desk_clock_policy.py` on a
traced host without LeakSanitizer support. Normal and ASan/UBSan runs cover two
full-refresh periods, 60,240 minute-boundary cases, overflow-sized epochs,
serialization and malformed records, three-attempt catch-up, failed presentation,
configuration changes and cancellation. Outputs remain unmodified on rejected
encoding/decoding and non-ready sleep plans.
