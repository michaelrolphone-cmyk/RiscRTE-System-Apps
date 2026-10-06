# Settings 1.3.0: tap wake and measured calibration

The opt-in `PORTABLE_TAP_SETTINGS` row preserves all existing row IDs, RTC,
time-format, sleep-mode and alarm preferences. It uses the established Nova
44-pixel controls and neutral-before-action touch handling. It is enabled only
for deployments granting `motion.accel@1` plus the existing namespace-1 storage.
`build_portable_settings.py --tap-settings --nova-ui` builds the standalone lane.

[Actual production-rendered pages](tap-wake/calibration-contact-sheet.png).
These are rendered from the production controller/view with modeled sensor
observations, not screenshots from a physical Watch. Test-only observations are
in `test/native_apps`; they do not enter the target dependency closure.

## Interaction

- Off/On are drafts until Save; Cancel never writes
- Calibrate explains the guided procedure before Start
- Candidate hardware values are tested least-sensitive first; the app requires
  three real Bosch double-tap detections with separate quiet windows, then a
  five-second ordinary-wrist-movement trial with no detector triggers
- The result shows the measured encoded value and requires Save to apply
- Calibrating while Off keeps Off; the two sensor profiles retain separate values
- No saved values change on cancellation, noise, insufficient samples, read/setup
  failure, delayed/stalled sampling, or alarm/quick-control interruption
- Failed hardware restoration keeps its live ownership and offers Retry Restore;
  health/display/poll failure retries cleanup before returning, because Runtime
  checks its exit barrier before calling fini
- Long sessions suppress idle sleep and restart inactivity timing afterward
- An uncertain storage write/readback is labeled unconfirmed

`tap_wake` is an eight-byte checked record in existing settings namespace 1.
Missing data keeps released defaults. Invalid/unreadable data is fail-closed
for tap wake. Existing paired update/NVS preservation applies; the new feature
never seeds or clears a namespace during install/update.

## Honest measurement limits

The encoded Bosch sensitivity is selected from observed hardware detections,
not guessed from acceleration. Fresh ±2 g samples verify that the user performed
the movement trial; 150 mg of observed axis span is a movement-validity gate,
not a tap threshold. No g-to-encoded-parameter formula is claimed.

This does not add a custom equal-impact/quiet classifier during Deep sleep.
The sensor continues to use its autonomous double-tap feature. The awake
calibration checks do not establish a physical false-wake rate or guarantee
pickup/tilt rejection. Physical Watch qualification remains pending.

## Verified lanes

`test_tap_settings.py` runs 160 production UI scenarios across both sensor
profiles, both touch rotations, and normal/ASan/UBSan builds. It also runs the
1,024-record persistence and corruption/state-machine suite plus a real alarm
arbitration regression: a sub-500 ms CUE is rejected before its disturbance can
be read as the user's tap. Combined poll/restore failure is checked with the
live app authority retained until restoration succeeds.

Existing Nova Settings tests remain required (272 executions). The pinned
Xtensa 8.4.0 target ELF is structurally validated with imports and exports checked.
On the current cloud executor, ASan/UBSan ran with `ASAN_OPTIONS=detect_leaks=0`:
LeakSanitizer itself cannot operate under its ptrace-based execution environment.
CI retains default sanitizer settings. No hardware test is implied.
