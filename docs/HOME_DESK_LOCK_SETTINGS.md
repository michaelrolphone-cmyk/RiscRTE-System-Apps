# Selected Home desk-lock Settings

Settings 1.3.14 adds `--home-desk-lock` to the explicit `x4-native-time`,
`--paper-transitions`, `--touch-scrolling` selection. Row 4 remains in place,
but displays **Top-right key / Desk clock** as a readonly value. Direct,
touch, and repeated key activation return no change without opening the
sleep editor or reading or writing `sleep_mode`.

This field describes the selected Clock/Home behavior. Settings supplies
no sleep backend. Existing sleep preference bytes remain available to older
or future policies; the selected Settings build offers no manual Light/Deep
selector. Its build receipt records no editable sleep modes and no sleep
fallback, plus an explicit readonly Home desk-lock field.

The flag is off by default. Existing row indices, face, landscape direction,
flip, language, time, alarm, toolbar, and timezone scrolling behavior remain
on their existing paths. Without the flag, the prior 1.3.13 scrolling,
1.3.12 paper motion, and 1.3.4 Watch selections preserve ELF and manifest
bytes exactly.

## Focused verification

After building the selected target, run:

```
python scripts/test_home_desk_lock_settings.py --target-dir build/home-desk-lock-target
python -m unittest discover -s tests -p 'test_home_desk_lock_build.py'
```

The native harness uses the target receipt's feature selection and verifies
the exact staged Runtime and tagged alarm SDK headers. It covers the actual
Settings get/activate/touch API and root navigation with Light, Deep, Hybrid,
invalid, missing, and unavailable saved preferences. All six preserve sleep
bytes and perform zero sleep preference reads/writes. Neighboring editors
and selected timezone drag, selection, Back, and flipped-input paths run at
480×800 and 400×600, normally and under ASan/UBSan. Providers are simulated;
no hardware qualification or publication is claimed.
