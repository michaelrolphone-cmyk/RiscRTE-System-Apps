# Selected API1 terminal retention

`PORTABLE_ALARM_TERMINAL_RETENTION` is a deployment opt-in for catalog-backed
alarm.service API1 consumers. It requires `PORTABLE_ALARM_CLIENT` and Runtime's
append-only `retain_invocation` suffix. Unsupported Runtime tables are rejected
before any provider is acquired. The bundled Runtime ABI prefix is unchanged.

The client stores terminal state and checks both that state and the shared
adapter latch before calls. A returned `ALARM_RETAINED` (-9), or that error in a
successful copied status, promotes the Runtime fence immediately. Ordinary
STORAGE/RTC errors still reach the foreground retry UI. Failed alarm acquisition
or release also checks Runtime's memory-only API accessor: a missing current API
means Runtime retained during activation/cleanup and the adapter must latch too.

The app-local `portable_adapter_retain()` and `portable_adapter_retained()` hooks
compose with catalog AppData callers and the existing retained-sleep hook.
Retention performs no diagnostics or ordinary yields. Subsequent client calls,
foreground dispatch, quick refresh, background stop, fini, and repeated init
perform no service/provider/release I/O. Failed fence promotion preserves custody
in a call-free loop. Normal clean invocations may open and close repeatedly.

The deployment sleep owner receives a guarded API1 prefix with a guarded
optional sleep/resume suffix. This is a sleep view, not a projection capability.
The original grant's API is preserved for capability-specific consumers. The
selected Watch sleep source must also check terminal returns before hardware
restore or free; use the matching Watch `watch_alarm_sleep.h`/`portable_sleep.c`
change and the same define on every linked translation unit.

Validation:

- `python scripts/test_portable_alarm_retention.py`: 68 plain/ASan/UBSan executions
  of the real client and adapter, with and without Contexts/BLE. Covers returned
  and copied retention, refresh/ack/stop, fini, sleep/resume, missing Runtime after
  acquire/release, ordinary storage/RTC/unavailable errors and repeated use.
- `python scripts/test_alarm_retention_target.py --baseline BASE_SYSTEM --xtensa-cc CC`:
  four flag-off Xtensa adapter objects compare byte-for-byte with the delivered
  Watch20 System source; the corresponding selected profiles compile.
- Existing alarm, Contexts and broadcast adapter regressions pass normally and
  with ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0` under this traced executor).

These are source, target-compiler and host custody checks. They do not qualify
physical sleep, wake timing, storage power loss or a final firmware image.
