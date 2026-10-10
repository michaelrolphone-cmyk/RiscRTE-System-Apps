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
