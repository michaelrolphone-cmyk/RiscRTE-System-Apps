# Portable Settings 1.2.0 (development)

The shared Settings source continues to use the original `Apps/settings.c`
navigation through the portable adapter. This development version is reserved;
it is not a published package or a deployment pin.

## Optional persisted sleep choice

`--sleep-settings` / `PORTABLE_SLEEP_SETTINGS` enables the bounded Light, Deep,
and Light-then-Deep chooser through an explicit storage.key-value@1 namespace-1
grant. The client uses `PortableSleepPolicy.h`; missing, malformed or unreadable
records select its Hybrid default. Save verifies the record; a failed save is
reported as unconfirmed because persistence may already have changed. The
interface is a generic runtime contract, not the old Reader firmware-owned
CrossPointSettings bridge or removable-media filesystem API.
Deep wake restarts Clock; no ULP-coprocessor or measured power claim is made.

## Optional alarm alert preference

`--alarm-settings` / `PORTABLE_ALARM_SETTINGS` is a compile-time opt-in, off by
default; existing deployments are unchanged. It appends an **Alarm Alerts** row after About,
without changing any existing Settings, About, or Sleep row index. The bounded
page offers **Vibrate**, **Sound**, and **Both**, with explicit Save and Cancel.
Touch, keyboard navigation, Back, and right-swipe cancellation use the existing
Settings view; selecting a mode alone does not write storage or play a preview.

`PortableAlarmSettings.h` implements the Utilities alarm-service contract:

- Namespace **1**, exact key **`alert_mode`**, exactly **one byte**.
- **1 = vibrate, 2 = sound, 3 = both**.
- A missing key displays Vibrate without writing a default.
- A wrong size or value displays Invalid, with no selected mode. The user must
  explicitly select a valid mode to repair it. Read failures and invalid or
  unavailable KV tables display Unavailable; they do not become a valid default.
- Every explicit valid Save performs one one-byte put, then an exact one-byte
  reread. Both OK and uncertain IO write results are verified. Only a present,
  valid, matching reread produces **Alert mode saved**.
- A failed/mismatching reread or rejected write displays **Save unconfirmed -
  retry** and retains the chosen draft. Cancel does not roll back an uncertain
  write. The root row stays **Unconfirmed** for this invocation; reopening the
  editor preserves the draft for an explicit retry, rather than claiming the
  previously persisted mode survived. There is no automatic write retry.

If Sleep settings is compiled too, both controls reuse its one namespace-1
acquisition and release. An alert-only build acquires namespace 1 once itself.
The feature does not change boot grants, add namespace-3/4 access, request an
alarm-service/output grant, or operate haptics/audio. The service consumes this
preference independently. Deployment selection, target packaging, shared alert
integration, and physical qualification remain separate gates.

## Verification

Run `python scripts/test_portable_alarm_settings.py` for 38 production-source
scenarios in both feature combinations, each compiled normally and with
AddressSanitizer/UndefinedBehaviorSanitizer (152 runs). Coverage includes all
three choices and persisted reopen, malformed size/value, absent/invalid grants,
read errors, exact one-byte writes/readback, IO both with and without persistence,
verification corruption/missing/mismatch, Cancel and swipe, drag/held input,
keyboard wrap, root-row touch routing, same-page and later explicit retry, and
balanced shared/alert-only grant cleanup, including failed RTC initialization.
The fixture asserts that Settings never requests alarm/output capabilities and
never writes RTC while editing the alert preference.

`python scripts/test_portable_apps.py` continues to check the feature-disabled
Settings paths, existing Sleep settings, navigation, RTC, and nested retained
sleep unwinds. Under ptrace-based local execution only, LeakSanitizer requires
`ASAN_OPTIONS=detect_leaks=0`; the new script leaves ordinary CI defaults intact.
Optional `PORTABLE_ALERT_FRAME_DIR` captures actual RGB565-rendered PPMs for
visual inspection. Host tests establish controller/ABI behavior, not physical
alarm sound, wake delivery, or target hardware qualification.
