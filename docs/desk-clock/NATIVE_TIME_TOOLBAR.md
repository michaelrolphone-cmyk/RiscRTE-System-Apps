# Opt-in app-owned native toolbar time

`PORTABLE_NATIVE_TIME_TOOLBAR` selects one private callback for both the shared
paper clock field and Quick Actions clock. It requires
`PORTABLE_NATIVE_CUSTODY_FENCE` and the complete canonical Runtime retention SDK
(`RiscRuntimeV1.h` and `RiscRealtimeV1.h`, baseline 602ae9bd). It is not selected
by ordinary builders, Settings native-time, or sparse Clock.

The application includes `PortableNativeTimeToolbar.h` and provides:

```c
bool portable_app_native_local_time(twatch_rtc_time_v1 *out);
```

The declaration has hidden visibility. A missing implementation is a link
error, including a target shared ELF; it is not a new Runtime import/export.
The application owns one-shot realtime READER acquisition, fresh snapshot
validation, its explicit namespace 1 timezone rule, projection into local civil
fields, and checked release before returning. Multiple UI consumers must share
that owner or take fully closed per-sample grants. The adapter never acquires
native time on the callback's behalf and never retains its sample or authority.

A true result supplies already-local Gregorian fields in 1600..9999, including
a correct Sunday=0 weekday. The adapter checks month/day/leap-year consistency,
weekday, hour, minute and second without changing or normalizing any field.
Malformed or false results leave the caller's outputs unchanged and display
unavailable. Native validity and the source epoch range remain the app's
responsibility; a plausible local date never establishes native validity.
The broad civil range permits valid local 1969 around native epoch zero.

The adapter checks its pure retained-state flag immediately after the callback,
including callbacks that return true after fencing. `portable_adapter_retained()`
is available to the app's pure helper phase guard.
`portable_adapter_retain()` invokes the canonical owner-only native fence once,
pins invocation resources and makes copied drawing callbacks inert. No clock
fallback, battery read, render, poll, release or free follows retained custody.
Generic startup/finalization uses the same checked broker/KV proxies and ordered
cleanup as the native Settings profile. Typed persistence failures remain
ordinary failures; CONTEXT/unknown statuses or uncertain acquisition/release
fence the invocation.

The feature rejects `PORTABLE_SETTINGS_APP`, `PORTABLE_SETTINGS_NATIVE_TIME`,
`PORTABLE_DESK_CLOCK`, `PORTABLE_DESK_CLOCK_SPARSE_START`,
`PORTABLE_RTC_UTC8_DENVER` and `PORTABLE_RTC_WALL_TIME`. These are separate source
owners; combining them cannot silently select a different clock. The new route
has no external RTC acquisition/read and never calls `portable_time_forward`.
Existing Settings and sparse Clock continue to use their current app-owned
source hooks without this flag.

## Reusable foreground source

`lib/PortableApps/src/PortableNativeTimeSource.c` is an optional, separately
linked implementation of the private callback for foreground apps that have no
native reader of their own. The adapter does not include it automatically.
Springboard, File Browser, BLE and general utilities can select this source in
a future explicit X4 deployment. Points and Alarms keep their own callbacks;
linking two implementations is an error. This change activates no product or
builder, and flag-off Watch code stays byte-identical.

Compile this source and the adapter with both `PORTABLE_NATIVE_TIME_TOOLBAR`
and `PORTABLE_NATIVE_CUSTODY_FENCE`. Link the application-local helpers
`PortableRealtimeClient.c`, `PortableTimeZone.c`, `PortableTimeZoneCatalog.c`
and `PortableTimeZonePreference.c`. Stage canonical `RiscRuntimeV1.h` and
`RiscRealtimeV1.h` from Runtime
`30dcec5ce6ce33223f2b203a2399283e1f758567`; do not replace the repository's
compatibility header globally. The same mutually exclusive source flags listed
above apply. Existing Quick flags and dependencies remain the caller's choice.
With the toolbar flag absent the new source emits no code or symbols.

The selected deployment must explicitly grant:

- `runtime.realtime@1`, instance 0, the readonly native reader.
- `storage.key-value@1`, instance 1, for the selected `time_zone` record.
- The adapter's usual display/input/battery and optional Quick dependencies.

Do not add `runtime.realtime-control` or `rtc.clock` for this source. It never
sets/seeds native time, reads or writes an external RTC, writes preferences, or
recovers an unset clock. Any time initialization remains a separate owner.

Each sample acquires checked namespace-1 KV, loads and validates the canonical
IANA record, and closes that grant before acquiring native realtime. An absent timezone record uses Reader’s virtual UTC default without a write.
Invalid or unreadable records display unavailable. Explicit saved UTC is valid. Native time is read freshly,
validated by the production helper and released before projection/output. No
sample, selected timezone or provider pointer is cached across clean samples.
Epoch zero can project into local 1969; the native maximum is 2147483647.

Typed KV `NOT_FOUND`, `BUFFER_SMALL`, `INVALID` and `IO`, typed ordinary native
errors, malformed snapshots/tables, and a clean-empty unavailable native
acquisition display unavailable and permit the next sample to retry. KV
`CONTEXT`/unknown statuses, native `CONTEXT`/unknown statuses, malformed grants,
uncertain acquisition/release, or a phase guard reporting retention fence the
invocation. Boolean KV acquisition failure is uncertain even with an empty
grant because external-provider rollback is not established. Unresolved grants
remain in invocation-local static storage. There is no further provider I/O,
release, drawing, polling or freeing after the fence. Call the source only in
the initialized adapter's synchronous foreground phase, before device holds or
finalization; its guard is a pure retained-state check, not an independent
Runtime ownership proof.

## Verification

```sh
python scripts/test_native_toolbar.py \
  --runtime-sdk /path/to/Runtime/sdk/app \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc
```

The production adapter runs with strict host doubles normally and under
ASan/UBSan. Cases include valid Gregorian boundaries, epoch-zero-adjacent local
dates, malformed fields, unavailable and retained callback results, repeated
paper views, Quick clock/modal/action interactions, and no subsequent I/O or
frees after custody is fenced. The runner verifies conflicting flags, absent
custody/SDK and missing callback linkage, then links pinned Xtensa shared
harnesses. Host callbacks model app ownership; they do not prove a downstream
app correctly closes its native grant. The downstream app must test its actual
callback/helper composition separately. No product/BIN/hardware qualification
or publication is implied.

Qualify the reusable owner composed with the actual adapter and helpers:

```sh
python scripts/test_native_time_source.py \
  --runtime /path/to/Runtime \
  --xtensa-cc /path/to/xtensa-esp32s3-elf-gcc
```

The owner tests run paper and Quick profiles normally and under ASan/UBSan in
fresh processes. They cover selected-zone changes, DST transitions, explicit
UTC, epoch bounds, missing/invalid storage, malformed native time, typed
ordinary failures and retries, malformed grants/tables, retained acquisition,
read and release, copied UI callbacks, live display-frame pinning, and Quick
modal success/retention. Provider doubles reject overlapping KV/native grants,
external RTC or control acquisition, and any preference write. Two pinned
Xtensa shared harnesses validate allowed imports/exports and loader structure;
flag-off Watch adapter objects and complete harness ELFs are compared exactly
with System Apps `81f884b8a053cf917054fb1433c7850714cd0c48`. These remain host and
link qualification, not downstream product manifests, BINs or hardware tests.
