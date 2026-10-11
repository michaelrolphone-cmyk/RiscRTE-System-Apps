# Isolated checked Set Time client

`PortableSetTime.h/.c` is an opt-in app helper for explicit confirmed Set Time.
It does not change Settings, Clock, paper views, defaults, manifests, builders,
products, chip policy, or the selected timezone. Nothing is called on load or
normal boot. It depends only on the existing typed native realtime control,
`rtc.clock@2`, and namespace-1 `storage.key-value@1` APIs. Compile it alongside
`PortableRealtimeClient.c`, `PortableTimeZone.c`, and the frozen timezone catalog;
provide the canonical pinned `RiscRealtimeV1.h` as in the realtime client docs.

## Pure planning and user choice

`portable_set_time_prepare(request, plan)` is pure. The request contains desired
local calendar fields, an already parsed timezone rule, explicit/loaded RTC
basis, an RTC provider instance, fold choice and native readback tolerance.
The caller is responsible for obtaining that basis through a read-only load or
an explicit user choice. A missing or corrupt basis must not be silently
converted into UTC. The helper never loads, guesses, infers or migrates basis.
It preserves `stores_utc`, regardless of the prior reference epoch.

The timezone inverse requires one valid candidate or an explicit fold choice:
- `fold_choice=-1` rejects repeated local times.
- `0` selects the earlier UTC candidate; `1` selects the later one.
- Every skipped local time is rejected. There is no normalization or UTC
  fallback, including when the external RTC stores UTC.

Calendar weekday input is ignored and recomputed. Invalid Gregorian dates and
leap seconds fail. The chosen instant must fit native realtime and the explicit
RTC reference domain, 2000-01-01 00:00:00 UTC through 2038-01-19 03:14:07 UTC.
The resulting external RTC calendar must also be in 2000..2099. These are
intersecting limits: the wider RTC calendar domain never extends native 2038.
With UTC basis the RTC calendar is UTC; with local basis it is the selected
local date. A local date in late 1999 can therefore be accepted only when the
actual UTC instant and UTC-basis RTC calendar are already in 2000. Local-basis
RTC dates before 2000 are rejected. Output remains untouched on planning errors.

The catalog is the frozen Reader 2026 recurring POSIX rules, not historical
IANA rules or a live timezone-law service. See `desk-clock/PORTABLE_TIMEZONE.md`.

## Invocation-local custody and execution

Zero-initialize one `portable_set_time_client`; never copy, mutate, serialize,
retain across invocation restart, or reuse its grants/callbacks. `open` acquires
only a fresh typed native CONTROL grant. It performs no clock read/write or KV
operation. The object and pure local phase-guard context must remain alive
through its invocation. Both this client and its embedded realtime client check
object identity to reject copies. Runtime remains authoritative for task,
generation, owner, retention and storage barriers.

Call `portable_set_time_apply_confirmed` only in response to the explicit Set
Time confirmation. Each call performs this sequence:
1. Compute and validate the pure plan before any provider I/O.
2. Acquire exactly the declared RTC@2 instance. Instance zero uses Runtime's
   unique-authorized-provider rule; there is no registry-order selection.
3. Sample a checked native monotonic bracket, write the exact planned RTC
   calendar, read RTC once, and sample native again. Both VALID and well-formed
   UNSET native snapshots supply monotonic brackets. Require ordered brackets
   and a full interval from the first `monotonic_before_us` to the last
   `monotonic_after_us` of at most 250,000 microseconds. Within that bound, every
   field including weekday must match either the intended calendar or exactly
   its next Gregorian second. For a local-basis tick, the chosen UTC instant +1
   must also map to that same local calendar. A DST gap/fold jump is never
   normalized. Reversed brackets, a longer interval or a larger clock jump fail.
4. Release RTC completely before any native seed.
5. Seed the verified UTC instant (the request or request +1 for an accepted
   RTC tick) with `portable_realtime_seed_confirmed`, then require a checked
   VALID readback in `[verified instant, verified instant + tolerance]`.
   The caller explicitly supplies a tolerance of 0..5,000,000 microseconds.
6. Acquire only KV instance 1. Put the canonical `rtc_basis` record with the
   preserved basis and verified UTC reference, then verify exact bytes with get.
7. Release KV. The native grant remains open until explicit clean close.

There is no initial metadata read, hidden repair, unchanged optimization or
clock rollback. An explicit action overwrites this one metadata record only
after both clocks have been verified. It never writes `time_zone`, alarms,
other keys or RTC chip registers. The existing unguarded multi-call
`portable_rtc_basis_save` is deliberately not used: each provider call here
needs its own before/after phase check.

## Partial results, failure and retries

The result separates RTC, native and metadata outcomes. RTC/metadata each have
NOT_ATTEMPTED, UNCONFIRMED and CONFIRMED states. UNCONFIRMED means a write was
attempted and may already have taken effect. `*_write_accepted` means its callback
returned success with a safe post-call phase; only exact readback marks CONFIRMED.
The pure `plan` retains the requested values; `verified_utc_epoch` identifies
what was actually verified and seeded. `rtc_window_us`, `rtc_window_confirmed`
and `rtc_tick_accepted` expose the timing proof and any accepted tick.
Native has separate attempted, accepted-seed and verified flags and a checked
snapshot when available. `stage` identifies the latest attempted stage, `reason`
records the operation failure and `cleanup` records any cleanup/custody failure.
The return status prioritizes a cleanup/custody failure. A verified value remains
reported as verified if its subsequent grant release becomes uncertain.

This is not an atomic transaction. RTC can change before native seed fails;
both clocks can change before metadata fails; KV IO can mean the new record
persisted even when no confirmation is possible. After failed native seed or
readback, metadata remains unattempted and its prior reference may now be stale.
Runtime native IO can invalidate time: prior native snapshots are not safe
substitutes for a fresh confirmed result. No result claims the old RTC/native/KV
value survived a failed write, and no result claims simultaneous clock updates.

The phase guard is checked before and after every external acquire, write,
read, seed and release. It is pure and local: SAFE is permission for this app
phase, not proof of Runtime's hidden provider custody. These outcomes permanently
stop every later I/O, including close/release, on this client:
- Non-SAFE local phase, typed native CONTEXT, or unknown native status.
- Any failed/ambiguous provider acquisition, even false plus an empty grant:
  the bool broker cannot certify successful provider-activation rollback.
- Any false RTC write/read. False cannot prove clean provider ownership.
- Malformed acquired provider table or grant.
- KV CONTEXT or an unknown KV result. Documented typed persistence errors are
  handled separately below; no error implies the old record survived.
- Any unsuccessful release or release that fails to clear the full grant.

Original uncertain grants are preserved, including when a failed release
mutates its temporary argument. A changed guard does not revive a halted client.
Never retry cleanup, reinitialize the same object, or create another client to
get around a sticky stop. The controller must invoke the supported Runtime
retained-invocation fence before returning, preserve app/provider custody and
perform no subsequent rendering, cleanup or other I/O. This isolated helper
exposes the sticky outcome; it does not call an unpinned retention callback or
claim that returning from the helper/app alone retains the invocation. Final
controller integration requires the canonical feature-gated Runtime header.

A documented KV IO, INVALID, NOT_FOUND or BUFFER_SMALL result preserves the
unconfirmed metadata outcome and stops that save attempt. It makes no further
get/put call, performs one checked KV release, and permits a fresh explicit
attempt only if that release and local phase remain safe. A failed put never
triggers readback. IO after commit may already have persisted the new record;
there is no rollback or automatic retry. KV CONTEXT/unknown status and any
uncertain release still halt with the original grant intact.

This distinction follows canonical Runtime 602ae9bd `keyValueGet`/`keyValuePut`,
which translate backend failures to typed IO without setting retained custody,
and `NvsKeyValue.h`, whose RAII Handle closes NVS on every return. Its IO after
commit means persistence is unconfirmed. Runtime's real KV lifecycle fixture
also continues through IO to checked release and fresh-grant acquisition.
The local phase guard and checked release remain required; this is not a claim
that an arbitrary provider's boolean error proves safe ownership.

A clean verification mismatch, ordinary typed KV failure, known typed native
INVALID/IO, or validated native readback error with a safe phase allows an
explicit new Set Time attempt.
There is never an automatic retry. A successful call can also be repeated only
as a new confirmed action; RTC and KV acquire fresh grants each time. A clean
close is idempotent and permits opening fresh native authority. Pure planning
may be repeated without I/O. Native-only `open` can retry a clean unavailable
native grant. Provider uncertainty never takes that native-only shortcut.

## Verification

Run with the already installed pinned toolchain and public Runtime checkout:

```sh
python scripts/test_portable_set_time.py \
  --runtime-repo /path/to/RiscRTE \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc \
  --evidence receipts/portable-set-time.json
python scripts/test_portable_realtime.py --runtime-repo /path/to/RiscRTE
python scripts/test_rtc_basis.py
python scripts/test_portable_timezone.py
python -m unittest discover -s tests
```

The production sources run normally and under ASan/UBSan. The Set Time suite
includes every frozen-catalog DST interval in 2000..2038: first/last affected
seconds, midpoint, immediate neighbours, all explicit fold choices, and both RTC
bases. The independent Python regex/datetime oracle generates 294,840 vectors;
each is executed against the actual C planning helper in both builds. The host
lifecycle fixtures cover exact successful ordering, <=250ms RTC tick proof,
leap/month/year tick rollover, local gap/fold rejection, UTC-basis DST ticks,
2038 +1 rejection, reversed and uint64-wide brackets, failed native bracket
reads with an owned RTC grant, malformed short tables,
false/empty broker results with hidden retention, RTC IO before/after commit,
KV IO before/after commit, typed native failures, no reads/releases after
uncertainty, every before/after guard boundary, output partial states, 2038 and
calendar boundaries, copied/stale authority and explicit safe retries.

The pinned Xtensa GCC 8.4.0+2021r2-patch5 link-only ELF passes the actual structural
validator and mutation tests. Imports are only `memcpy`, `memset`, and
`risc_runtime_get_api`. The source-hashed receipt records the SDK, toolchain,
generated vector and ELF identities. No privileged, libc time, ESP-IDF time or
compiler 64-bit division imports are introduced. The strengthened 123-scenario
realtime client suite passes, with RTC-basis and timezone regressions and
repository contracts checked separately.

The harness is not a product image, deployable app or hardware qualification.
It does not prove physical RTC tick/write behavior, oscillator accuracy,
real-device provider activation, sleep retention, or final controller rendering.
