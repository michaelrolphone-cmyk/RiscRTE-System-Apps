# Selected alarm terminal guard

Select `PORTABLE_ALARM_TERMINAL_RETENTION` with the existing
`PORTABLE_NATIVE_CUSTODY_FENCE` for catalog consumers. API1 and tagged API2 clients
then use `PortableAlarmTerminalClient.h`; the existing implementation remains the
flag-off path. No API1/API2 service ABI is replaced.

Every client entry checks the shared adapter latch as well as client state.
API1 now rejects successfully copied error=-9, matching the API2 custody check.
Ordinary copied STORAGE/RTC errors remain recoverable. A failed acquire or release
checks Runtime's memory-only API accessor and latches if activation/cleanup
already retained Runtime. Selected adapter refresh/ack/pump dispatch uses these
client guards. Retained sleep return and subsequent idle entry are also fenced.

`portable_adapter_retain_silent()` shares the existing Runtime latch while
skipping the stage log. Use it after a terminal capability result or missing
current Runtime API: the normal stage logger calls Runtime health/diagnostic and
is not safe once custody is terminal. The ordinary retention helper preserves
its existing diagnostic behavior. Home's projection bridge must check
`portable_adapter_retained()` before entry and use the silent helper on -9.

Validation:

- `python scripts/test_portable_alarm_terminal.py --runtime RUNTIME --utilities UTILITIES`:
  40 real API1/API2 adapter cases plus descriptor/client matrices, plain and
  ASan/UBSan, with stage logs enabled. Includes copied retention, global retention
  by another owner, ordinary errors and activation/release retention.
- `python scripts/test_alarm_terminal_target.py --baseline BASE_SYSTEM --runtime RUNTIME --utilities UTILITIES --xtensa-cc CC`:
  four API1/API2 plain/Quick Xtensa flag-off objects compare byte-for-byte; four
  selected profiles compile.

The selected X4 native idle helper already checks returned and copied service
retention before cleanup. This patch guards its shared-adapter entry and result;
it does not replace that board-local implementation. These tests do not qualify
physical hardware or a deployed image.

## Public compatibility source

This focused public port is qualified on commit
`16058588cd8e64021b8c89c4fbad3f2ecc736a5a`. The original selected X4 implementation
is `b1d539df71c7e2c32bf3be6c4ed4228d13f45e0b`, whose newer base remains separate.
The guarded client headers, tests and runners match that implementation exactly;
the adapter changes are applied to the existing public files.

This earlier public adapter already has a quiet ordinary retention helper, so
the new silent helper delegates to it. The selected sleep guard calls that fence
before returning. Both decisions preserve the public base's exact flag-off
bytes. No later Home, touch-recovery, scrolling, or other adapter feature is
copied into this port. The source comparison lists exact matching inputs and
the five adapter files adapted to the public base; it makes no whole-tree
equivalence claim or substitution for the original X4 build pin.
