# Future X4 desk-clock Settings profile

This is an opt-in shared Settings client, stacked on renderer source
`3702b9a594196bb5cb546732a5d4aa668c88ff91` (PR #69). It does not enable a
product bundle, change Watch defaults, qualify an X4 sleep backend, or modify
the frozen X4 0.1.6 source/artifact. Settings only writes app-owned preferences;
Clock alone may own `x4.power` and the sleep/peripheral/retained orchestration.

## Build selection and identity

The ordinary build still reads `Apps/settings.json`, version **1.3.4**. The
new `--settings-profile x4-desk-clock` explicitly selects the separate future
**1.3.5** identity in `lib/PortableApps/profiles/x4-desk-clock-settings.json`.
It defines `PORTABLE_SLEEP_SETTINGS` and `PORTABLE_SETTINGS_X4_DESK_CLOCK`,
exposes Light and Deep Desk Clock only, and appends Desk Clock Face after all
existing rows. It leaves time/date editing, timezone/RTC policy, time-format,
Alarm Alerts and About row identities unchanged. No `x4.power` requirement
is added. No catalog, product manifest, release index, tag or bundle is changed.

Example future development build (not a deployment):

```sh
python scripts/build_portable_settings.py \
  --settings-profile x4-desk-clock --display-rotation 90 --navigation \
  --alarm-client --alarm-settings --quick-actions --wall-time \
  --return-app springboard.elf --home-app default.elf \
  --output-dir dist/desk-settings-future
```

The existing paper capability gate still requires a retaining MONO1 portrait
logical display: width 400–600, height at least 600 and greater than width.
An X4 800×480 native surface needs the explicit existing rotation-90 flag.
The future profile rejects RGB/unsupported geometry rather than leaking its
choices into Watch. Face rows adapt to the existing smaller portrait range;
no display-capability gate is broadened. The build receipt records profile,
all defines, supported sleep modes, fallback, six face IDs, preference instance
and keys, source hashes, artifact hash and version. Product integration must
select this profile only after its Clock backend and grants are qualified.

Live version check, 2026-10-07: connected GitHub reads found Settings 1.3.1 on
System Apps main, 1.3.0 on the visual-alert development branch, and 1.3.4 on
the exact renderer base. PR #67 identifies 1.3.4 as frozen X4 0.1.6's manual-light
build. The future identity is isolated from that manifest and from Watch.

## Shared preference contract

Both records use the existing explicit `storage.key-value@1` **instance 1**.
They are app-owned opaque bytes, not Runtime policy or authority. Loading,
rendering, draft selection, Back, Home and cancellation never repair or write
preferences. Each explicit Save verifies the saved bytes by readback; already
identical valid records require no write.

### Sleep mode

`PortableSleepPolicy.h` retains key `sleep_mode`, four bytes:

```text
53 01 MODE (MODE xor A5)
MODE: 0 Light; 1 Deep; 2 Hybrid
```

Legacy `portable_sleep_load/save` retain their original all-mode behavior and
Hybrid fallback. New helpers are:

```c
int portable_sleep_load_profile(const risc_key_value_v1 *kv,
    unsigned supported_mask, unsigned fallback, unsigned *mode);
bool portable_sleep_save_profile(const risc_key_value_v1 *kv,
    unsigned supported_mask, unsigned mode);
```

Masks are `PORTABLE_SLEEP_MASK_LIGHT`, `_DEEP`, `_HYBRID` and `_ALL`.
The future X4 profile uses mask **3** (Light|Deep) and fallback **0** (Light).
Loaded/Missing/Invalid/Unavailable statuses remain 0/1/2/3; status
`PORTABLE_SLEEP_UNSUPPORTED` is **4**, returned for a structurally valid mode
excluded by this profile. It returns the explicit fallback without rewriting
the record. Invalid masks/fallbacks fail validation. Saving a filtered mode
is rejected before persistence. Hybrid is absent from rendered choices and
unreachable through keyboard cycles or old touch coordinates.

Root values and the editor visibly distinguish missing, invalid, unsupported
and unavailable records. A stale Hybrid shows “Light (unsupported saved)” and
“Unsupported: Light default”; it remains unchanged until explicit Save. Failed
write/readback shows “Unconfirmed” on the root and a retry message, not Saved.

### Face

`PortableDeskClockSettings.h` provides key **`desk_clock_face`**, four bytes:

```text
46 01 FACE (FACE xor A5)
FACE: 0 Segments; 1 Sans; 2 Serif; 3 Minimal; 4 Railway; 5 Deco
```

These are the stable IDs in `PortableDeskClock.h`; the 80-byte retained clock
payload is unchanged. APIs:

```c
int portable_desk_face_load(const risc_key_value_v1 *kv, unsigned *face);
bool portable_desk_face_save(const risc_key_value_v1 *kv, unsigned face);
const char *portable_desk_face_name(unsigned face);
```

Status names `PORTABLE_DESK_FACE_LOADED/MISSING/INVALID/UNAVAILABLE` are
0/1/2/3. Missing, malformed, wrong-size/schema/checksum/out-of-range records
and read failures return **Segments**, without writing. The reader's buffer
is exactly four bytes; oversized records are invalid. Invalid saves fail
before writing. Failed writes or mismatching/unavailable readback retain an
explicit unconfirmed UI state and require another deliberate Save.

Clock integration should read these helpers at a safe foreground boundary,
use the existing namespace-1 time-format record, copy the confirmed face into
`portable_desk_config.face`, and invalidate prior-image reuse whenever any
rendering configuration changes. An unavailable read is a fallback policy
result, not proof that the user's saved choice changed. This Settings profile
does not perform RTC wake programming, power calls, checkpoint writes,
boot dispatch, automatic idle transitions or retained record cleanup.

## Software evidence

- **144 executions**: 59 real paper controller/storage scenarios in plain and
  ASan/UBSan modes, two real native-retained editor cases in both modes, and
  rejected landscape/RGB profile cases in both modes, and nine helper/choice
  cases in each mode on a 400×600 two-column paper surface. Tests cover every face,
  both sleep modes, complete root touch/keyboard routing, neutral/held-input
  suppression, dropped/poll-failed contacts, Cancel/Back, navigation and physical
  Home, malformed/missing/stale records, read/write/committed-error/verify/mismatch
  failures, failed grant release, and no provider I/O or cleanup after native
  retention. The fixture also exhausts every valid supported-mode mask/fallback.
- Existing **272 Nova Settings** executions, portable app suite, paper Settings,
  combined alarm/preference and retained suites, and **52 Python unit tests** pass.
- **30 legacy Watch sleep-selector frames** compare byte-for-byte with PR #69.
  Per-frame hashes are in `settings-evidence/watch-raster-sha256.json`.
- Default 1.3.4 target build is byte-identical to a read-only compile of PR #69
  with the same flags: 167,096 bytes,
  `2cee46c99a1c0174762218c4276571be1a87d55e3a9cdbd9d2917c441d128d62`.
  The opted-in 1.3.5 artifact is 172,672 bytes,
  `7791d885922f1d38c8cf24ee47b437c7ea8eca5520c5245cf231a52c38c13960`.
  Both default and opted-in builds pass the pinned Xtensa GCC 8.4.0 build,
  Runtime structural validator and import/export allowlists.
- Fourteen lossless 480×800 logical captures come directly from the production
  paper renderer's 800×480 MONO1 buffer, rotated without resampling. They cover
  every face selection, both sleep modes, the root pages and all sleep fallback
  notices. They are host fixtures, not device photographs; the standalone
  fixture's Alarm Alerts row uses its existing fake preference provider.
  All PNG pixels were read back and verified. See [contact sheet](settings-evidence/contact-sheet.png)
  and `settings-evidence/raster-sha256.json`.

Reproduce with `python scripts/test_desk_clock_settings.py`; add `--evidence`
(with Pillow installed) to regenerate captures. Local ASan/UBSan runs set
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer is blocked by executor ptrace;
CI keeps its normal settings. No hardware was used. Physical display,
RTC wake, deep sleep, current consumption and complete product deployment
remain integration/qualification work.

The inherited Reader face source, asset custody and all font/license notices
are unchanged. The Settings builder retains its existing font/license copying.
