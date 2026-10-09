# Opt-in portable realtime client

`PortableRealtimeClient.h/.c` is an invocation-local consumer of Runtime
0.1.49's typed realtime API. It is deliberately not linked into any default
Clock, Settings, Watch, paper adapter, or product profile. This change has no
manifest or boot-policy edits. It does not include user Set Time writes, RTC
writes, time synchronization, networking, storage reads, or persistence.

The canonical `RiscRealtimeV1.h` comes from public Runtime commit
`341e6e38ce00b7c57d5daaf5f8c829c3575db931`; its SHA-256 is
`639781f728841cda6d40c3033877436b9e5d59519a4b15e11a9bc99b2365395d`.
The build test reads that exact Git object into temporary include storage rather
than inventing a second ABI or changing the existing SDK baseline. Deployment
must supply this canonical header on its include path. The existing
`RiscRuntimeV1.h` capability prefix is unchanged; no private Runtime API is used.

## Authority and app lifecycle

Initialize one `portable_realtime_client` to zero, on this app invocation's
foreground `app_main` path. Never copy, serialize, retain across restart, or
reuse its grants, opaque contexts, callback pointers, or guard context after
app return. The client rejects copies using its own object identity. Runtime
still owns authoritative task, active-entry, generation, retention and storage
barrier checks: the app helper is not a security boundary.

`portable_realtime_open(client, runtime, access, startup, guard, context)`
acquires exactly one fresh, typed grant, at instance zero:

- READER: `runtime.realtime@1`, snapshot-only authority
- CONTROL: `runtime.realtime-control@1`, snapshot and checked seed authority

Control is self-contained. There is no second read grant, raw platform grant,
libc time import, ESP-IDF import, or registration of another backend. Native
seed changes the device-wide wall clock. The app manifest and boot policy must
explicitly allow the selected capability; opening control does not itself seed.
The normal-start recovery path additionally needs its explicitly selected
`rtc.clock@2` instance in both declarations. Instance zero relies on the
broker's unique-authorized-provider rule; the client never chooses a provider
by registry order.

The caller-supplied `portable_realtime_phase_guard` is a pure local permission
check. SAFE says the current app phase permits I/O: foreground app_main, before
peripheral holds, with no known ownership uncertainty or retention. It cannot
prove private Runtime `providerStorageSafe` or ownership state. No such API is
required. Check the app's existing state and explicit sleep outcomes only;
never perform broker/provider/storage I/O inside the guard.

The helper checks the guard before and after every acquisition, native call,
RTC read, and release. A guard result other than SAFE, native CONTEXT, unknown
native status, malformed/ambiguous acquired grant, or unconfirmed release
permanently stops further I/O and release on that client. Changing the guard
back to SAFE does not revive it. An unsuccessful native-only acquisition with a clean empty grant is ordinary
UNAVAILABLE only while the guard remains SAFE. External RTC acquisition has a
stronger rule: even false plus an empty output cannot distinguish missing/denied
capability from failed provider activation/rollback with internal retention.
It therefore stops as UNCERTAIN (with operation reason UNAVAILABLE), without
releasing the existing native grant. The app phase guard cannot override that
ambiguity or prove internal rollback succeeded. A false RTC read is also
UNCERTAIN with operation reason IO, even when the guard remains SAFE: it cannot rule out provider-local retained custody. Neither
grant is released and no later read, seed, recovery or release is attempted.
A failed release is never retried; its original grant is preserved as non-retryable
evidence. A successful release must clear its grant as the pinned broker does.
An app must honor its existing Runtime-retained return path on RETAINED and
must not attempt generic provider cleanup after CONTEXT or UNCERTAIN.

`portable_realtime_close` is idempotent after clean close. It releases the
native grant and clears callbacks. That release does **not** unset native time;
SDK realtime validity survives releasing control. The independent app-owned
retained-wake record has a different lifecycle and is not handled here.
A normally closed client may reopen with a fresh native grant/context after an
ordinary sleep refusal has restored all holds. A halted client may not reopen.
`portable_realtime_stop(client, retained)` is a pure, sticky no-I/O stop for
known ownership loss or retention; it never attempts release.

## Timer-only and pre-preparation sampling

TIMER_ONLY open/read can acquire and read only native realtime. It cannot load
KV preferences, activate external RTC, touch radio/touch/storage providers, or
seed. `portable_realtime_read` returns OK for VALID or UNSET for a correctly
formed unset snapshot; negative errors leave the caller's output unchanged.
The client checks exact snapshot size, known validity, zero reserved field,
ordered monotonic bracket, native seconds 0..2147483647, nanosecond bounds, and
the pinned native backend's microsecond resolution. Epoch zero can be VALID;
a plausible date never turns UNSET into valid time.

Sample before peripheral preparation, then close successfully before taking
holds. Only after this safe point may the app begin its independent sleep
preparation. Never call native read, recovery, or a live-grant close while
peripheral holds or retained outcomes prevent I/O. Calling close on an already
closed client is pure and does nothing.

`portable_realtime_project(snapshot, elapsed_us, max_elapsed_us, estimate)` is a
pure bounded estimate for later foreground preparation work. Supply elapsed
microseconds measured in this same boot and your explicit processing budget.
If elapsed is measured since `monotonic_after_us`, the estimate retains the
sample's before/after uncertainty. It is not a new native sample. The
uint32 elapsed bound, fractional carry, and 2038 limit are checked without
64-bit division imports. Projection leaves output unchanged on failure.
Never feed a requested sleep duration, elapsed time from another boot, or a
retained old monotonic timestamp into this function. Fresh deep wake requires
a fresh native snapshot. Physical oscillator accuracy remains unqualified.

## Explicit normal-start recovery

Call `portable_realtime_recover_rtc` only when the app chooses this policy.
It always reads native realtime first. A VALID snapshot returns OK immediately,
without looking at RTC policy or acquiring RTC. If time is UNSET, only a
NORMAL_START control client may continue; timer-only and reader clients return
DENIED and include the UNSET snapshot in the result.

The caller supplies a `portable_realtime_recovery_policy` containing a tested
parsed `PortableTimeZone` rule, explicit/loaded `portable_rtc_basis`, the RTC
instance, a fold choice (-1 rejects ambiguity, 0 earlier, 1 later), and an
explicit `allow_basis_change` flag. Invalid rule/basis/choice is rejected before
RTC acquisition. The helper never reads or writes the metadata's KV namespace.
Missing or corrupt persisted metadata must be resolved by the caller rather
than passed as a silently inferred default.

On permitted recovery the helper:

1. Acquires the declared `rtc.clock@2` instance and checks the full table.
2. Reads exactly one calendar, never writes RTC or alarms, and uses the tested
   pure `portable_timezone_interpret_rtc` policy.
3. Reports malformed dates, local-calendar gaps, unchosen local folds, unusable
   sources, and native out-of-range dates distinctly. It does not use the
   interpretation core's possible UTC fallback to hide a local gap/fold.
4. Reports inferred UTC/local-basis changes. Without `allow_basis_change`,
   returns BASIS_CHOICE without seeding. Even when allowed, it never persists
   the inference. The result provides interpretation and mode-change details
   for a separately authorized caller decision.
5. Releases RTC completely before invoking native checked seed(epoch, 0).
   A failed or unconfirmed RTC release prevents seed and every later I/O.
6. Reads native realtime once to confirm VALID. A successful seed followed by
   a failed/unset read is not reported as successful recovery.

The reused Reader interpretation policy regards external RTC dates before
2000 as UNUSABLE even when they fit native seconds. Negative chosen epochs and
dates after the native 2038-01-19 03:14:07 UTC limit return RANGE. This does not
restrict valid native snapshots from 1970 onward. UTC-basis RTC calendars do
not acquire a fictitious local DST ambiguity; local-basis calendars require
explicit fold selection and cannot normalize gaps. Calendar weekday is ignored
and recomputed by the pure timezone core.

Recovery returns RECOVERED only after checked seed and VALID readback. Its
result also provides the operation `reason`, `cleanup` status, interpretation
availability/details, last validly copied snapshot, and whether seed was
confirmed successful before readback. If cleanup fails, the function returns
that failure while preserving the preceding operation reason. No unconfirmed
seed/cleanup outcome is automatically retried or hidden by subsequent success.
Native IO may mean the native clock has been invalidated by a failed set; the
caller must not continue using earlier sampled time as though recovery worked.

## Focused verification

From this repository, with the existing public Runtime objects available:

```sh
python scripts/test_portable_realtime.py \
  --runtime-repo /path/to/RiscRTE-Runtime \
  --xtensa-cc /path/to/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc \
  --evidence receipts/portable-realtime-client.json
python scripts/test_portable_timezone.py \
  --evidence receipts/portable-realtime-timezone-regression.json
python scripts/test_rtc_basis.py
```

The production client runs 123 deterministic scenarios normally and with
ASan/UBSan, with `ASAN_OPTIONS=detect_leaks=0` for traced executors. Coverage
includes reader/control grants, unset/valid timer isolation, stale and copied
clients, missing capabilities, physically short API headers, malformed tables
and snapshots, IO/CONTEXT/unknown native failures, RTC acquisition/read/release
failure including hidden retention after read=false with a still-SAFE app guard, incomplete broker outputs, gaps/folds/basis choices, 2038 boundaries,
failed readback after seed, the real Runtime false/empty RTC-acquisition
retention semantics while the app phase guard still says SAFE, every
external-call phase boundary, duplicate close,
close-before-preparation projection, restored reopen, and permanent retained
stops. Providers and broker are strict host fixtures; these are production-client
tests, not a substitute for real Runtime or physical-device tests.

The pinned Xtensa 8.4.0+2021r2-patch5 link-only harness passes the actual ELF
structural validator and its mutation checks. It imports only `memcpy`,
`memset`, and `risc_runtime_get_api`; the latter's public export is independently
verified in the pinned Runtime source because the old Reader export snapshot
predates that portable entrypoint. No libc/ESP time symbols or libgcc division
helpers are imported. The source-hashed receipt records compiler, SDK and ELF
identity. It is not a manifest app, firmware, install catalog, or release.

Timezone regressions pass all 419 catalog entries, 55,308 round trips, 8,400
leap edges, 22,653 independent Python-oracle cases, normal/sanitized default and
Denver Watch policy, and RTC-basis corruption/uncertain-persistence fixtures.
An additional 98 existing paper Clock presentation/navigation/wall-time/failure
cases pass normally and under ASan/UBSan, with the original test matrix and
one temporary binary instead of retained images.
Default app and product sources remain unchanged. This does not qualify
physical retention, oscillator drift, wake reliability, power consumption,
provider activation on real hardware, or next-profile integration.
