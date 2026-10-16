# First valid RTC Save: composed host qualification

This regression adds no firmware change. It composes the real Settings
controller/adapter, both production RTC providers and the legacy Watch alarm
service. The physical I2C bus, clock progression, display/input and storage/output
boundaries are host models. Target instructions and hardware are not executed.

## What is exercised

The matrix has two devices (PCF85063 and PCF8563), normal plus ASan/UBSan,
raw plus UTC8/Denver Settings profiles, and four initial register states:
valid, oscillator-stop/voltage-low, corrupt BCD, and invalid month.

- The original UI input fixture opens the editor and performs one valid Save.
  Both the basic RTC path and the clock-change/alarm path run the real UI.
- The alarm path selects Points, volume, DND, UTC8/Denver and
  `ALARM_CLOCK_CHANGE_CONTROL`; Settings selects its corresponding suffix.
  An explicit empty Points schedule isolates clock repair from unrelated cues.
- An invalid cold clock legitimately presents the existing RTC-error modal.
  The UI fixture presses Back to reach the editor, then saves once. It never
  presses Retry. The alarm service subsequently reaches READY with the saved
  RTC value.
- Direct save cases inject one transient read failure, four failed verification
  reads, a failed write, and a failed alarm-side verification read. Failure
  messages stay explicit; no case silently performs a second editor write.
- An unannounced clock jump reproduces ALARM_RTC before a valid editor Save
  reanchors the same live service without calling refresh/Retry.
- All successful saves verify the real provider's decoded register value.
  Normal resource releases and bounded verification timing remain asserted.

There are 256 process-level cases. The runner records source hashes, dependency
commits and each result in `build/rtc-first-attempt/evidence.json`. The existing
Settings clock-change regression additionally checks incompatible service
suffix rejection before writing, and an alarm BUSY response.

## Reproduce

Run:

```
python3 scripts/test_rtc_first_attempt.py \
  --runtime RUNTIME --utilities UTILITIES --twatch TWATCH --waveshare WAVESHARE
python3 scripts/test_settings_clock_change.py
python3 -m unittest discover -s tests
```

Pinned source sets are in `.github/workflows/rtc-first-attempt.yml`.
The initial local run used System `cb483470887b5198e2f3cd2457586ef6622142a9`.
All tracked source and test blobs equal System main
`769cde61ae0c69b602843ce293985deb2c4fc187`; only two unrelated workflow files
differed. The qualification PR is based on main and preserves those workflows.
The local Waveshare receipt names unpublished source `b5931645`; CI pins
published `92be7bd0b3666dfdf63d468f9206762cd1804053`. Their driver/include blobs
are identical; the only two tree differences are release-enablement documents.

## Delivered Settings lineage and limits

The saved T-Watch build receipt at product source
`aa91c1cd7eb664d9a5bd78df05cab53b2c683f18`,
`docs/t054/settings-command.json`, enables
`PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL` for Settings 1.3.24.
Its `source-pins.json` identifies System `9dcb2bbef44853e533d980a658a814d2c787a4fa`.
The complete `editor_save_raw` function in that source is byte-identical to the
qualified function: SHA-256
`d6c5423b50840a2e82669091121f881be9ecf1dbf86c66d3b2a7883dd8c1bf65`.

Both preserved product Settings ELFs contain the clock-change-specific
incompatible-service and alarm-sync failure strings. That supports the
selected suffix, but does not reproduce the entire ELF. Their observed hashes:

- T-Watch: `fa1c87a1e971d1d64a5d65d8e655d898bbd227a42364f639517ff53ea2966e3e`
- Waveshare: `a7558b3bd2cd0c2a0edee95e4a8a69477a63772e8300e33e8e7332a2558f8d41`

No claim is made that these tests reproduce every shipped Settings feature,
prove oscillator startup or persistence, execute target machine code, or close
the physical first-attempt task. No source pin, ELF, firmware, store or release
is changed by this regression.

## Remaining device qualification

On each intended firmware build, record its version and existing clock state.
From a cold start, open Settings and dismiss any RTC-error notice with Back.
Enter a valid date/time and tap Save once. It must report Saved to RTC without
Retry or a second Save; reopen the editor and confirm readback. Confirm the
alarm service settles without RTC ERROR, and that the clock continues advancing.
Capture available RTC write/read diagnostics and any failure before retrying.
Hardware confirmation and whole-ELF source reproduction remain separate gates.
