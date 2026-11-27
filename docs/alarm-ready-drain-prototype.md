# Tagged alarm ready-step drain (unselected prototype)

`PORTABLE_ALARM_READY_DRAIN` opts the shared PortableApps foreground adapter into
bounded draining of immediately completed service phases. It requires the tagged
`ALARM_SERVICE_TAGGED_V2` contract; a legacy prefix is not interpreted as tagged.
No service ABI, client structure, persistent schema, schedule, or clock policy is
changed. Omission keeps the historical pump and phase-yield behavior.

The helper returns the actual typed step result only after valid copied status.
The loop still captures input before each phase and refuses dispatch/navigation
until reconciliation resolves. ALARM_PENDING, active cues, or elapsed budget use
the existing display-progress/yield path. Lease deferral remains an early return.
Only ALARM_OK may proceed without that unconditional yield. No snapshot caching
or reload suppression is introduced. Global retained custody is checked around
new helper callbacks, with no subsequent status call after retained step.

The experimental between-phase budget is `PORTABLE_ALARM_READY_BUDGET_MS=8u`.
It is NOT a hard duration bound: a synchronous provider call, capture, or yield
can overrun it. Clock observations include the final step that becomes READY.
Backward short intervals fail conservatively; uint32 wrap is valid. The existing
64-phase work bound remains for nonprogressing services/clocks. An already-READY
ordinary call adds no loop clock observations. Logical input dispatch still
waits for authoritative reconciliation; acquisition is not dispatch.

## Integration points (available, not selected by defaults)

The generic `--alarm-ready-drain` option is registered and validated by
`scripts/portable_foreground_idle_build.py`; it requires returning foreground
alarm sleep and all its explicit tagged-client prerequisites. That common
configure path emits the define and exact budget, and records an
`alarm_ready_drain` object in the foreground receipt only when selected. System
Springboard/Settings/WiFi/Files builders already use these common options.
`portable_alarm_build.py` continues to stage the exact tagged SDK unchanged. Utilities `build_portable_apps.py` and
`build_badge_native_alarm_apps.py` must carry it through their explicit Namespace/
function arguments into that exact System helper, not fork the policy. The external Utilities parsers must also register/forward the new field; this
prototype changes only System, not those external builders or product selection.

The current nine shared-adapter apps are Springboard, Settings, WiFi Settings,
Files, Calculator, Stopwatch, Battery, Alarms and Countdown. Watch Home uses its
separate `clock_alarm.inc` loop and does NOT inherit this drain. Dedicated app
configuration/edit operations also remain their existing controllers' operations.
No claim is made for every app in the repositories.

Both owned header variants (`PortableAlarmClient.h` and
`PortableAlarmTerminalClient.h`) contain the new opt-in helper. They are System
source, not imported entries in `lib/PortableApps/SOURCES.json`; no canonical
Utilities alarm SDK header/pin/hash changes. Staged copies and compile dependency
hashes must be regenerated from the new exact System pin. Watch dependency lock
and product source lock must be repinned if integrated, without pretending Home
adopts the loop. Default profiles remain unselected.

## Evidence scope

The accompanying workspace `badge-alarm-ready-drain-evidence` contains actual
selected service + actual adapter elapsed models, simultaneous due/ACK/restore
and unknown-storage tests, extracted-loop/client fault tests and queued-display
regressions. These are software models, not NVS latency measurements or device
FPS evidence. The baseline selected 2 ms input limiter is preserved. An earlier
no-cadence countermodel is explicitly retained separately. The reported periodic
empty reload is about 24 modeled ms in the baseline zero-I/O case, not evidence
that this explains continuous slow scrolling.
