# Selected X4 alarm BUSY deferral

The alarm service's opt-in `ALARM_STORAGE_BUSY_RETRY` profile returns
`ALARM_PENDING` when native storage refuses an operation with typed no-I/O
BUSY. Pair it with `PORTABLE_ALARM_BUSY_DEFER` in tagged-API2 System clients.
This branch starts from delivered System `57e549ef5a1c727e8a24658e572f402e006fe82d`.

The foreground yields between deferred LOADING steps instead of exhausting its
64-phase drain and permanently showing Service error. Input remains consumed
while loading. Status and result retention fences still run before deferral;
READY is not fabricated and active-alert/error handling is unchanged. Both
ordinary and terminal-retention clients are covered. Flag-off source behavior
and the rebuilt delivered Home ELF remain byte-identical.

Run `scripts/test_alarm_busy_adapter.py --runtime RUNTIME --utilities UTILITIES`
with Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70` and Utilities
`80efb298c9ca8d0f62f9a31840c9e51d380b40f6`. Normal and ASan/UBSan cases exercise
1,000 consecutive deferred ticks at cold startup and mid-reconciliation, then
successful readiness, visible true storage faults and terminal-custody paths.
Leak detection is disabled for ptrace compatibility.

The selected Home/Clock component advances 0.4.13 to 0.4.14, retaining the exact
full delivered face/wake/Points recipe and adding only the opt-in define.
Other selected tagged System clients can add the same define on their next
source-backed build. Physical first-flash verification remains pending.
