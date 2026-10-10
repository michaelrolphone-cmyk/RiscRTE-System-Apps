# Full-wipe cold Clock/alarm regression

`test_sparse_clock_cold_alarm.py` composes the production sparse Clock, adapter,
X4 sleep client, and actual tagged native-UTC visual Points alarm provider. It
starts a fresh process for every case with no retained record, a power-on or
reset boot cause, native time UNSET, and every preference/alarm key missing.
The missing-key cases do not inject a stored timezone, RTC basis, disabled
Points schedule, or fabricated retained record.

The runner first extracts delivered System commit
`d305e6b718f51d42ffcf6339e05ce4b56398f68c` from local Git. That negative control
must reproduce `ALARM_STATE_BLOCKED / ALARM_RTC`: no RTC read or native seed
occurs when either or both interpretation preferences are absent. Its rendered
RTC-error modal must match the current-source explicit RTC-error control.

The current-source cases require one checked RTC read and native seed before
first alarm reconciliation, then `ALARM_STATE_READY` with no error modal. A
loaded native snapshot must bypass RTC recovery. Missing RTC basis is tested
with Denver local time and against both an unchosen DST fold and a DST gap;
these cases detect accidentally changing the virtual default to UTC basis.
Corrupt/unreadable preferences, invalid RTC data, and seed I/O errors stay
explicit errors. RTC acquisition/read/release failures, seed/native-release
custody failures, and alarm dependency context failures retain the invocation
without subsequent I/O or grant cleanup. Ordinary paths close every grant,
subscription and frame; provider quiescence performs no storage/time I/O.

Clock recovery must never write the RTC or preferences. The real provider's
existing virtual factory Points schedule separately compacts expired events
into `points_utc_occ` after time recovery. The fixture permits that one ledger
key, preserves its bytes for the provider's actual verification read, and never
pre-populates it. Treating all provider writes as forbidden would either hide
this production behavior or require a nonempty starting state.

Run both Quick configurations normally and with ASan/UBSan:

```sh
python3 scripts/test_sparse_clock_cold_alarm.py \
  --utilities /path/to/utilities-git \
  --x4 /path/to/x4-git \
  --runtime /path/to/runtime-git \
  --sdk /path/to/selected-driver-sdk \
  --evidence /tmp/cold-alarm-evidence.json
```

`--system` can select another current-source worktree. The script always takes
the delivered negative control and provider/client/Runtime SDK inputs from their
exact recorded Git objects, ignoring unrelated working-copy changes. It pins
Utilities `637e13b0bce62ad49b756bec2468a6271d163fc7`, X4 sleep client
`e2281ade9eab04e8252c9861cda01a228d613859`, and Runtime0.1.54 SDK
`7fa39e01d7214a4a341032a15ab467a58ae22cec`. The supplied selected driver headers
and tested System sources are hashed in the receipt.

The 108 cases are host composition tests. Broker/native-time/hardware boundaries
are strict doubles; actual Runtime Graph custody, RTC electrical behavior,
Xtensa execution, full-device wipe/flash, and physical panel behavior require
separate qualification. This runner does not build or publish a device image.
