# Watch Settings verified RTC edit

This branch starts from the frozen Settings source actually selected by
Watch 1.1.17 (`14762601b3fa7bbeeac5ed565a6825534bf74dca`). Rebuilding that
profile reproduces the delivered Settings ELF exactly before the repair.

Only the selected build adds `PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL` and
increments Settings to 1.3.24. Pair it with alarm service 0.4.9 built with
`ALARM_CLOCK_CHANGE_CONTROL`. The flag-off path is unchanged.

The new optional API1 suffix is admitted before writing the RTC. Settings
writes once, performs its existing readback, and only then tells the alarm
service about the verified clock change. Failed writes/readbacks never call
the new control. An alarm-sync failure is reported separately from an RTC
write failure. Reconciliation does not acknowledge or rewrite saved events.

`scripts/test_settings_clock_change.py` runs the real save helper with normal
and ASan/UBSan configurations. It covers legacy-service refusal before any
write, one-write success, write/readback faults and alarm-sync failure. Leak
detection is disabled for ptrace compatibility. Physical verification remains
pending.
