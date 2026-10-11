# X4 automatic Light and low battery policy

This explicit development profile reuses the accepted Watch source
`13f5fd4bed27d77d566fa7c34b89b62445b86798` for `PortableLowBattery`, timer
records and one-shot threshold semantics. It adds no Runtime policy, GPIO
access, brownout change, hardware qualification or distributable X4 product.

Automatic idle retains the current app stack, drafts, saved frame and live
private grants. It calls the separate deployment helper
`minimal/apps/portable_idle_sleep.c`; it never reads the saved manual sleep
mode, stages a retained-wake record, reconstructs the app, or enters Deep.
Explicit Home deep desk lock and timer-only sparse Clock remain on the existing
manual helper. The latter never acquires foreground battery/preferences/radios.

## Behavior

- Idle defaults to 60 seconds. Settings exposes an explicit Save/Cancel idle
  timer, 5–3600 seconds, in five-second steps. A valid new timer controls the
  next deadline without changing the ordinary editor's draft.
- A valid battery reading below 10 percent commits the Watch-compatible edge
  marker before applying 20-second idle, the retained 60-second deep-after
  record, 15-percent brightness and disabled saved Wi-Fi/Bluetooth intent.
  The deep-after record is deliberately unused on X4: no automatic Deep or
  Hybrid lifecycle is selected or qualified.
- X4 preserves intentional backlight OFF (zero) and its saved nonzero restore
  level. Unknown, absent/profile-missing and malformed battery samples cannot
  enter or rearm the policy. Charging alone does not rearm it; a valid reading
  at or above 10 percent does. Reboot/partial writes never replay an already
  recorded edge or overwrite subsequent manual changes.
- Pending app frames, held contacts, Quick gestures/modals, alarm ownership,
  handoffs, open/uncertain storage, active Wi-Fi scan/join/link, update work and
  continuous SDR/audio capture inhibit automatic entry. Provider maintenance
  is not user activity. A safe idle entry closes consumer subscriptions and
  drains owned radio/audio work before typed peripheral preparation.
- Telemetry is paused before sleep. After clean returning restoration, the
  normal client may resume it only under confirmed saved Bluetooth/broadcast
  intent. Low-battery radio OFF, airplane mode, competing capture and retained
  cleanup still prevent it. Sleep does not save an OFF preference. Foreground
  SDR/audio capture is never automatically restarted by this adapter.
- The helper uses power17, display3, raw touch4, volume9, Wi-Fi15 and Bluetooth16.
  Alarm service is tagged API2/singleton0; shared preferences are read/write
  namespace1. Automatic-idle ordinary apps gain only explicit power/volume
  requirements, not raw GPIO, retained-wake, native control or promotion.
- Prepare closes radio hardware, reconciles alarms, blanks the backlight,
  prepares touch/panel and commits reversible SD sleep. The native duration is
  conservatively bounded by the unchanged alarm ticket and elapsed preparation
  time. Resume restores SD, panel and touch, consumes that exact Light ticket,
  restores confirmed brightness, then returns through ordinary app input and
  saved-radio restoration. Held wake input must go neutral before a new action.
- Ordinary refusal rolls back checked partial preparation. Any retained,
  unknown custody, failed grant release or unconfirmed restore returns the
  native-retained sentinel immediately. The adapter retains the invocation;
  no ordinary cleanup, provider polling, free, release or RF restart follows.

## Selection and receipts

All five builders accept these additional explicit options:

```
--x4-idle-source <X4>/minimal/apps/portable_idle_sleep.c
--x4-idle-sdk <typed-power-sdk>/sdk/driver
--x4-idle-runtime-sdk <Runtime>/sdk/driver
```

They require the existing native-time/sparse profile, tagged alarm SDK, Quick
Controls, Quick Radios and paper transitions (OFF-capable brightness). They
select `PORTABLE_X4_IDLE_POLICY`, `PORTABLE_LOW_BATTERY` and
`PORTABLE_APP_SLEEP_LOCAL`. Clock omits `PORTABLE_SLEEP_MANUAL_ONLY` only for this
profile and retains its explicit manual Home hook. Opt-out builds keep their
prior versions/requirements. Reserved selected versions are Clock0.3.12,
Springboard1.7.9, Settings1.3.17, Files1.5.12 and Wi-Fi1.1.14.

`idle_policy` receipts record the exact helper, compiled typed SDK, shared policy
source hashes, physical IDs, unused deep-after timer and hardware limitations.
Native app admission receipts use the actual staged idle SDK. Central product
composition must explicitly opt each selected app into `app_grants(...,
idle_policy=True)` and record those exact grants. No product image is built here.

## Verification

`test_x4_idle_targets.py` builds all five apps with the pinned Xtensa compiler
and each production loader/import/export check. Supply `--x4`, `--runtime`,
`--power-sdk`, `--display-sdk`, `--utilities` and `--cc`.

Host checks run both normally and with ASan/UBSan:

- `scripts/test_low_battery.py`: original Watch crossing/storage/Settings tests,
  X4 OFF and restore-level preservation, timer records and manual overrides.
- `scripts/test_x4_idle_adapter.py --candidate <settings-build>`: actual native
  Settings controller/adapter keeps a modified nested date draft across clean
  refusal and wake, preserves custody on retention, and tests admission gates.
- `scripts/test_x4_idle_gates.py --candidate <clock-build>`: real sparse adapter,
  timer-only silence, inherited held input, bypass of saved Deep preference,
  and actual Wi-Fi/update app eligibility.
- X4 `run_idle_alarm_sleep_test.py`: production helper plus production tagged
  alarm provider, 54 fault/deadline/restore scenarios per compiler mode.
- X4 `run_idle_panel_policy_test.py`: actual fast panel0.1.6 plus helper;
  2300ms resident settling, 30000ms awake maintenance, pending frame and active
  maintenance overlap. Typed prepare drains/cancels resident work; no work runs
  after preparation or resume until a new app frame.
- X4 `run_idle_grant_runtime_test.py`: actual Runtime/ProviderGraph with host
  provider bodies, X4 physical mapping and dependency edges; accepts exact
  grants and rejects wrong IDs, duplicates and missing power grants before
  activation. It does not model electrical hardware.

Hardware sleep/current, physical wake/timing and battery behavior remain
unqualified. Product source is unchanged at its base product version until
central composition assigns and verifies a new complete cohort.
