# Opt-in sparse Clock startup and native time

`PORTABLE_DESK_CLOCK_SPARSE_START` is selected only by the explicit
`build_paper_clock.py --desk-clock --sparse-start` development profile. It emits
Clock **0.3.2**. Ordinary desk Clock remains 0.3.1; default Watch/paper builds,
product locks, the 32-ELF product and delivered BINs are unchanged. Version 0.3.2
was checked against live branches, tags and issue/PR claims on 2026-10-07 and
reserved for this local slice. Publication remains a separate approval gate.

The slice preserves adapter `40b90180aab331f2190a1665695751e264154343` and
presentation `a51eee665157747ee727fa3af6ce9fa250314f39` as separate merge parents,
on public integration `d52a74bfb4acc01cc3f0a9dda2c95ba2ba679ee9`.
The actual local sleep source is public X4 PR12 head
`1cc18a9c998a3d041cf25364fb15c98449fe99d3`.

## Lifecycle and authority

- Module init validates/copies the canonical Runtime table and initializes only
  software. It acquires no provider, reads no preferences and performs no I/O.
- `app_main` reads the classified app-owned retained record before opening any
  provider. A valid TIMER record must contain a resolvable timezone and must be
  accompanied by a fresh VALID native realtime sample. An old monotonic sample
  is never taken from retained storage.
- The timer opens only the adapter's display and alarm-service grants. The
  actual X4 client borrows those, and temporarily obtains X4 power17, matching
  display3, and native retained-wake0. The live alarm dependency closure remains
  active: board power, frontlight, panel, power, alarm, RTC and I2C. This is not a
  claim of display-only activation or hardware inactivity.
- TIMER directly reads no KV, external RTC, touch, navigation, battery, radio or
  SD provider. Alarm reconciliation retains its own declared service closure.
  Timer config, date, language, format, orientation and face come entirely from
  the validated retained record. The native UTC minute and retained timezone
  reconstruct the exact previous panel image, including DST transitions.
- Invalid/cold/GPIO wake, UNSET/unavailable/malformed native time, or ordinary
  timer refusal takes one explicit `runtime.provider-promotion@1` instance0
  transition before foreground startup or any temporary RTC recovery grant.
  The same live display mapping/grant survives timer-to-foreground upgrade.
  Promotion OK/ALREADY_READY permits upgrade. FAILED may leave successful
  prefix boot pins; the controller retries at most twice and otherwise exits.
  No promotion attempt can reenter the sparse timer path. RETAINED/unknown or
  unsafe-context outcomes enter the native invocation fence immediately.
- The complete normal cohort is pinned before RTC recovery can release a last
  transient reference. The adapter owns the one foreground battery grant;
  Clock does not acquire a duplicate battery or toolbar RTC grant.

The selected manifest has **14** distinct requirements: the existing 12 plus
`runtime.realtime-control@1` and `runtime.provider-promotion@1`. Both require
explicit instance0 boot policy grants for the configured default app. A reader
realtime requirement is unnecessary: the single control grant is opened in
TIMER_ONLY client mode for timer reads, which forbids recovery and seed.
Fixture high-water is 6 live app grants in timer work and 12 in foreground deep
sleep, within Runtime's unchanged live limit16. These figures count app grants,
not graph dependency grants or physically started providers.

## Native custody is mandatory

Sparse builds stage the **complete canonical** `RiscRuntimeV1.h`, including its
existing `confirm_boot` suffix before the new `retain_invocation` field. They
never append a field to the old shared prefix or overwrite the default SDK.
Startup checks `RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE` and the callback. Missing
or shortened tables fail before any provider operation.

The canonical public Runtime baseline is
`602ae9bd618e13407b5b94bcad86cdabc23c99ea` (0.1.51, PR41), tree
`fc660ad52df494befd87ba8b7599b77e8c00b82e`, identical to the tested local
`623ca49402841b779e2963f7c04453269f400080`. Runtime0.1.50
`7e79a8f06c1ee3b71214408c2477d9aaca985834` promotion semantics remain the contract,
but that version alone cannot qualify app teardown after provider-local
retention. The new owner-only fence is required for this profile.

Every app-local retained/uncertain path invokes the synchronous native fence
once, then makes copied callbacks inert and performs no further provider call,
release, cleanup, diagnostic or yield. The fence may acknowledge an already
Runtime-retained current owner. An impossible rejected owner-fence call uses a
no-I/O fail-stop instead of returning into unverified teardown. Foreground alarm
pump/failure/modal actions and finalization also fence uncertain output results;
they do not reuse the legacy yielding failure loop under this flag.

A local phase guard is only permission to attempt an operation. It cannot prove
Runtime graph storage safety, ownership or cleanup. The dedicated real Runtime
retention tests and Runtime/CpuPort gate regression remain independent required
checks; host provider doubles alone do not establish those properties.

## Time and recovery

Each native sample opens and closes an invocation-local `PortableRealtimeClient`.
Sampling/control close succeeds before staging or peripheral preparation. With
holds active, the controller only reads same-boot Runtime health uptime and
performs pure bounded projection of its just-closed snapshot. It never reads or
releases native time, external RTC or KV, and never cooperatively yields there.

The pinned ESP32-S3 Runtime's uptime and realtime monotonic sample share the
`esp_timer` boot domain. The projection uses uptime's upper microsecond endpoint
(maximum999us quantization), rejects a native sampling bracket wider than100ms,
and limits total processing age to65s. Low-word arithmetic handles the32-bit
millisecond wrap without introducing target64-bit division helpers. A crossed
minute restores providers and repaints, within the existing three-frame bound.
This qualifies software ordering, not oscillator accuracy, physical deep-wake
reliability or native timer-arm latency.

Normal startup explicitly loads namespace1 `time_zone` and `rtc_basis` without
repairing either. Missing/corrupt/unknown timezone stays visibly unavailable;
the preference helper's virtual UTC fallback is not adopted silently. A valid
native snapshot needs no external RTC. If native is UNSET, recovery additionally
requires both valid loaded records and a full promoted cohort. Recovery uses
`PortableRealtimeClient`, rejects unchosen folds/gaps and inferred basis changes,
and releases a confirmed RTC read before checked native seed/readback. No chip
layout is guessed. No RTC write, metadata write, Set Time policy, automatic UTC
normalization, network time or radio policy is introduced here.

## Verification and reproduction

```
CC=gcc-14 python scripts/test_sparse_clock_startup.py \
  --sdk /path/to/canonical-reader/sdk/driver \
  --runtime-sdk /path/to/canonical-runtime/sdk/app \
  --runtime-ref 602ae9bd618e13407b5b94bcad86cdabc23c99ea \
  --x4 /path/to/x4-sparse-sleep-client \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc \
  --evidence receipts/sparse-clock-startup.json
```

The runner hashes the exact SDK/source inputs, checks the public X4 source
object, runs real production controller + adapter + local client sources under
normal and ASan/UBSan host builds with both Quick profiles, and links the actual
production Clock ELF using pinned Xtensa8.4.0+2021r2-patch5. Production structural
ELF validation, malformed-ELF mutation tests and exact imports/exports apply.
It also rebuilds real default paper Clock and Watch Springboard ELFs against the
public integration base and requires byte identity.

Tests include native absent/invalid/unset, promotion refusal/partial/retained,
missing runtime suffix, cancellation, stage/rollback/release/cleanup failure,
forbidden timer calls, one battery acquisition, exact old-image pixels, all six
faces and both orientations, refresh rollover, retained language/config despite
poisoned live preferences, DST/local-scene equality with an independent Python
zoneinfo oracle, unknown/fold/gap unavailable pixels, preparation overrun, catch-up
and same-boot millis wrap. Native fence doubles reject every post-retention I/O.

Stage diagnostics are bounded one-line `SPARSE_CLOCK` entries for classification,
native sampling, timer readiness, promotion and recovery, using only same-boot
elapsed milliseconds before holds. Pair them with the Runtime's actual provider
start/ready logs on hardware to measure the seven-provider closure and startup
latency. Host timings are not MCU latency or physical power measurements.

### Verified local checkpoint (2026-10-07)

`receipts/sparse-clock-startup.json` records the clean production checkpoint
`002f88a61f9a717f3e2cfcf0250fc9a708d771cd` and exact canonical Runtime0.1.51
public SDK/tree. The independent checked RTC-read hardening is included from
`0e26b8972d47b1aabed1dea841471b1990f6826e`; a failed RTC read is sticky uncertain
custody, never an ordinary IO error followed by release.

- 2,180 fresh-process real controller/adapter/X4-client cases pass over both
  normal/ASan+UBSan and Quick-enabled/disabled builds.
- The separate sparse adapter suite passes60 normal and60 ASan/UBSan cases,
  including native-clock Quick Actions and retained no-I/O outcomes, plus three
  pinned target link harnesses and default token/ELF compatibility checks.
- Real Runtime/Graph/CpuPort gate cases pass normally and with ASan/UBSan.
- Existing default paper Clock passes98 normal/sanitized cases.
- Sparse no-Quick target:167,848 bytes,
  SHA-256`1bf2cab4f48f6b380dd2b1a6c5f6522c5d918d5794658bac500779056a442cea`.
- Sparse Quick target:204,368 bytes,
  SHA-256`242d049ac72e250e1b6b1c6579c862d74ca240a19730a79b081c3f8e8c37cbbd`.
- Actual default paper Clock is byte-identical to publicd52a74b:
  SHA-256`e75843904da745dc410b99a3fc5326b89efdb9f53ead2c8cc5c485052f6a7f4b`.
- Actual default Watch Springboard is byte-identical to publicd52a74b:
  SHA-256`7a6e8c44348eec67acb870c27341c3e3579c466c5144354916b6a00aa908934c`.

ELFs are development validation artifacts. No provider implementations, product
pins, bundle, BIN, GitHub branch or remote publication changed in this worktree.
Physical startup/current/keepalive, RTC interpretation and display wake still
require device measurements with the real selected providers.
