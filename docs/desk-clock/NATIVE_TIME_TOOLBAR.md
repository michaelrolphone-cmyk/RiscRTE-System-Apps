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
