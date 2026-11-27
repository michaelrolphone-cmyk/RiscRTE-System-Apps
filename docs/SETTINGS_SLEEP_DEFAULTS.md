# Generic Settings sleep defaults

The `default` and `native-time-touch` Settings builders can expose the shared
sleep-mode preference and a single idle timer without a board-specific fork:

```sh
python3 scripts/build_portable_settings.py \
  --settings-profile native-time-touch \
  --native-time-runtime-repo ../RiscRTE \
  --display-rotation 0 --safe-viewport --full-frames --navigation \
  --return-app default.elf \
  --sleep-settings --idle-timer-settings \
  --default-sleep-mode deep --default-idle-seconds 15 \
  --output-dir /tmp/settings-deep15
```

Use the same defaults in the Home build. Settings records these selections in
`settings-build-record.json`; they are not stored merely by viewing a row.

- `--default-sleep-mode`: `light`, `deep`, or `hybrid`; requires `--sleep-settings`.
- `--idle-timer-settings`: adds one editable idle row; requires `--sleep-settings`.
- `--default-idle-seconds`: 5–3600 integer seconds; requires `--idle-timer-settings`.
- Omitted overrides preserve Hybrid and 60 seconds. Without the new row flag,
  existing profile row selections are unchanged.
- Valid saved preferences win, including saved Hybrid and a saved 60 seconds.
- Missing records show the configured defaults. Invalid/unavailable timer
  records remain marked Unconfirmed on the root row, with fallback values and
  error text in the editor. Reads never save or repair a record automatically.
- Existing explicit-save verification, cancellation, retry and custody behavior
  remain shared. The deep-transition timer record is not exposed or changed by
  the new single-idle-row option.
- Configured mode UI does not claim all apps use a five-minute Hybrid policy.
- No power, battery, radio or RTC grant is added by these options. Settings
  changes preferences; it does not implement or qualify a sleep backend.
- Desk-clock/reader profiles reject these new options and keep their own policy.

Run `python3 scripts/test_settings_sleep_defaults.py` for parser validation and
real compact root/editor/saving/cancellation/reentry coverage, normal and
ASan/UBSan. Hardware, physical wake and battery draw are not covered by host
provider fixtures.
