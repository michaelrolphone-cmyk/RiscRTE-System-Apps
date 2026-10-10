# Opt-in tagged alarm consumer

`ALARM_SERVICE_TAGGED_V2` selects the shared portable foreground client's
`alarm.service@2` route. Provide Utilities' production `AlarmServiceV2.h` and
its adjacent `AlarmServiceV1.h` on the include path. The tagged header is the
only descriptor authority; the client requests API 2 explicitly and checks
its complete tag, version, masks, feature/callback consistency and every base
callback before dispatch. There is no API 1 downgrade or suffix inference.

Native Points combines this flag with `PORTABLE_ALARM_CLIENT`,
`PORTABLE_NATIVE_CUSTODY_FENCE` and `PORTABLE_NATIVE_TIME_TOOLBAR`. Supply the
canonical Runtime SDK headers, including the invocation-retention suffix,
and the app-owned `portable_app_native_local_time` callback. Optional
`PORTABLE_QUICK_ACTIONS` reads the validated API 2 output mask. Native visual
alarm profiles expose no sound control.

`ALARM_RETAINED` (-9), whether returned directly or copied in service status,
immediately calls the native retention fence. The client also pins its own
state and rejects later callbacks, cleanup and grant release. An uncertain
output result remains retained. Typed storage and RTC errors stay ordinary
service errors: the client copies the provider's state and the foreground
can display, dismiss and explicitly retry the existing error modal.

This change does not add sleep ownership or dispatch `resume_sleep`. The
serialized sleep owner must still prepare and consume the unchanged ticket
and call the validated Utilities resume helper once after successful native
Light return. Existing Settings semantics and Timecard are unchanged.

Without the flag, API 1 behavior and headers are unchanged. The focused gate
compares complete Xtensa adapter objects and linked alarm/Quick harness ELFs
against System `b05d3f01a776ab2708832054eb7dc3c4c04da48a` with the flag off.
These are development harness comparisons, not shipped image comparisons.

Run:

```
python3 scripts/test_portable_alarm_v2.py --runtime ../x4-runtime-provider-realtime --utilities ../x4-points-utilities-utc
```

The gate pins Runtime `30dcec5ce6ce33223f2b203a2399283e1f758567`, compiles
against the Utilities production descriptor headers, runs the client matrix
and ten production-adapter cases normally and under ASan/UBSan, and builds
both native alarm and native alarm/Quick targets with pinned Xtensa GCC
8.4.0 2021r2-patch5. Providers are deterministic test doubles; no physical
hardware, target execution, sleep-owner change, or publication is claimed.
