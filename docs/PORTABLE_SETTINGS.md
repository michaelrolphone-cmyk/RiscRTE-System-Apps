# Portable Settings 1.2.2 (development)

The shared Settings source continues to use the original `Apps/settings.c`
navigation through the portable adapter. This development version is reserved;
it is not a published package or a deployment pin.

## Combined time-format and alert Settings

Version 1.2.2 combines the reviewed Settings 1.2.1 time-format selector with
Settings 1.2.0 alert settings and its retained-sleep-safe foreground prerequisite.
The follow-on is based on System Apps PR #24 (`f204477b287263fa6c5a2d095829caf86b327506`)
and carries the Settings changes from PR #25 (`bd1032ec02a6dc21a1ad21c67b37a4579307672a`).
It does not change the shared adapter, another app, or the canonical alarm service.
The two original PRs remain separate prerequisites/checkpoints.

**Time Format** selects 12-hour or 24-hour with explicit Save/Cancel; the time
preview and hour editor follow the confirmed mode. `PortableTimeFormat.h` is
unchanged from PR #25: namespace **1**, key **`time_format`**, exactly four bytes
`{0x54, 1, mode, mode ^ 0xa5}` where 0 is 12-hour and 1 is 24-hour. Missing/invalid
values default to 12-hour without writes. Changed saves require put plus exact
readback; failures preserve the last confirmed display choice with an unconfirmed
message. A failed IO result can already have persisted, so no rollback is promised.
Neither preference changes RTC civil values or timezone policy.

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

Time Format, Alarm Alerts and optional Sleep settings share one namespace-1
acquisition and release. Each control accesses only its own exact key.
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

The same alert test command also runs nine combined same-invocation scenarios
in raw and Denver/180-degree touch profiles, normally and with ASan/UBSan (36
runs). They cover both selectors via scrolled root-row touches, held confirmation,
Cancel, time-format IO before/after commit, readback errors/mismatch, alert IO
and mismatch, explicit retries, independent exact key contents, unchanged sleep
preference, no RTC writes, single grant cleanup, and persistence after reopening.
Four additional plain/sanitized native-retained tests enter from both selectors
and assert zero later KV/RTC/navigation/service I/O, stop calls, or grant release.
Actual rendered normal/error pages were inspected at 240x240; captions sit clear
of Save/Cancel. Physical device behavior remains unqualified.
