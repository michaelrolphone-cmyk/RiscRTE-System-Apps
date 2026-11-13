# Settled resident alarm contract

RiscResidentAlarmV1.h adds checkpoint reason constants, not a Runtime API-table
suffix or replacement. The original RiscResidentShellV1.h types remain owned
by Runtime. Existing CHECKPOINT_ALARM retains value 3.

QUERY_ALARM_ONLY advertises an admitted validated alarm service through OK
and zero reply flags. BUSY means unsupported or temporarily unsettled.
The query performs no Home provider operations. CHECKPOINT_ALARM reconciles
the existing alarm modal without selecting idle sleep, controls or exit from
manual keys. Keys used to dismiss an actual alarm modal retain that meaning.

All operations require settled child UI/capture/background ownership and zero
request flags/reserved. They cross the normal Runtime native custody boundary;
they are not capture-safe observation calls. Never use POLL/POLICY as a fallback
for unsupported alarm-only handling.

The negotiated contract also includes QUERY_LAST_IDLE_ALARM, called immediately
after successful idle-only application and before restoring child owners.
OK/0 means no alarm input cancellation; OK/REDRAW means an alarm, cue or error
modal cancelled input during that application, including final reconciliation.
BUSY is retry, not a negative result. Queries are idempotent. New idle attempts
and child transitions invalidate old outcomes. A caller must not resume a
cancelled editor or turn this outcome into an unrelated Home-exit request.

## Compatibility evidence and limits

Public Runtime0052 accepts nonzero checkpoint reasons with zero flags/reserved
through its existing guarded dispatch. Only ordinary POLICY requires a pending
POLL handshake. Public Home b58a does not recognize these new reasons (nor
ALARM3) and returns BUSY. These statements are source-backed compatibility
facts, not results of the header test.

The owning workflow compiles and executes layout probes in strict C11 and
C++17, both with the original header alone and with the additive contracts.
It checks constant separation and compares all original client member offsets,
sizes and callback-prefix sizes. The runner verifies the original shell header
against exact Runtime0052 Git blob 26356c2b0a836c9d73efda058c80a71c3233fc38.
Normal and ASan/UBSan artifacts retain actual compiler dependencies, commands,
layout output, binaries and source archives.

These are host-compiler header/layout tests, not target ABI, actual Runtime,
production Home, hardware, alarm-output or old-host behavior tests. Actual
Runtime boundary and production Home behavior belong to their owning suites.
No service, Runtime implementation, existing HOLD provider or dependency pin
is changed by this contract-only delta.
