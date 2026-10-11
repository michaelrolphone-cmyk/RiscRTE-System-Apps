# Wi-Fi scan latency and shared-adapter regressions

These host tests use deterministic copied data. They do not access radios,
networks, user credentials, devices, or publication endpoints.

## Returned long-call display timeout

From the System checkout:

```sh
python3 scripts/test_wifi_scan_latency.py \
  --runtime /path/to/Runtime \
  --watch /path/to/provider-source \
  --sdk /path/to/tagged-alarm-headers \
  --output /path/to/new-test-output --expect-fixed --sanitize
```

`--system` can select a separate System checkout; its default is this repository.
`--runtime`, `--watch`, and `--sdk` are mandatory local source inputs. The SDK
folder contains `AlarmServiceV1.h` and `AlarmServiceV2.h`. The provider source
contains the production `drivers/twatch_wifi` and `drivers/twatch_ble` sources.
The fixture derives from Runtime's cross-layer host test. It builds the actual
System app/adapter, provider, CpuPort, and NativeRadio against fake outer grants,
display, clock, storage, and vendor SDK calls.

The preserved synchronous-path qualification uses Runtime
`274bc66f193cbe29018d2a85c9400cb0dce8aacc` and the recovered provider recipe whose
Wi-Fi driver SHA-256 is
`85fe797b910aab2530bb0c8bcb68cba445ab55f014d77d80303aede0ca3d0f57`.
This is the legacy compatibility and display-timeout test. It does not replace
the Runtime async-worker qualification.

The 20 cases include 30 successful/cancelled scans, missing-event timeout,
cleanup retention, and a pending display token whose clock is aged by an
injected 12-second SDK start call. With `--expect-fixed`, the adapter must query
an aged token exactly once: verified COMPLETE may continue; ACTIVE, a failed
status callback, FAILED, and SUPERSEDED must retain. Without that option, the
oracle expects the old pre-query timeout. `--expect-fixed` against the baseline
System tree fails, which is the negative control.

This reproduces a returned-long-call false timeout. It does not establish the
cause of any physical-device freeze. A never-returning synchronous SDK call is
a separate bounded-responsiveness problem covered by the new async path.

## Shared-adapter temporary deferral

```sh
python3 scripts/test_wifi_adapter_deferral.py \
  --sdk-include /path/to/native-tagged-sdk/include \
  --output /path/to/new-adapter-output --sanitize --terminal
```

This SDK folder additionally contains `RiscRealtimeV1.h`. The runner copies the
current System public headers before adding the three explicit native/tagged
SDK inputs. Omit `--terminal` to exercise the ordinary tagged alarm client too.

The 17 cases exercise the production shared adapter and native time owner:

- KV BUSY is forwarded without retention or backend work.
- A validated civil display sample remains frozen during BUSY; cold BUSY does
  not invent time or a timezone, and the next good read uses the actual zone.
  A definitive timezone/read failure invalidates the cache; subsequent BUSY
  cannot resurrect an earlier sample.
- Broadcast step/pause is deferred for the full owned Wi-Fi operation.
- Alarm calls require an actual non-yielding native resource lease. BUSY leaves
  loading work and copied refresh/ACK intent pending; healthy connected Wi-Fi
  can service alarms without cancellation. Retention forbids further callbacks,
  including lease-end. Every fake yield asserts that no lease is held.
- Compatible alarm-failure cleanup polls 100 pending turns through proven
  quiescence before exactly one alarm output shutdown. Native failure fencing
  remains terminal.
- Quick opening, manual action payload, handoff, Home, and return survive checked
  pending shutdown. A control overwrites the live UI action bytes before replay
  to prove the queued explicit intent is copied independently.
- Low-battery entry waits for checked shutdown before changing radio policy.
  High/rearm samples remain pending through BUSY and use a short service lease
  without cancelling healthy Wi-Fi. The observed edge is not consumed early.

Each runner writes commands, per-case logs, and source-hashed evidence. Host
sanitizer runs disable LeakSanitizer because intentionally retained custody is
part of the test oracle. Hardware and publication remain outside these tests.
