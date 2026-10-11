# Frozen Reader presentation and preferences

Development-only opt-in Clock **0.3.1** and `x4-desk-clock` Settings **1.3.6**.
This is a focused continuation of `a78723d2eefa9f865049c0e492b62c276d61d1ee`.
No product lock, firmware BIN, merge, release or hardware qualification changes.
Delivered X4 0.1.6 and 0.1.7 images are untouched.

## Exact source and boundary

Reader reference: [`34d8e694d89a1e72d8854403d8592c289fae3ddc`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/tree/34d8e694d89a1e72d8854403d8592c289fae3ddc).
[Source receipt](reader-evidence/source-provenance.json) includes hashes for
`src/DeskClockSleep.cpp`, `lib/hal/HalClock.cpp`, the I18n generator/runtime and
all 22 translation YAML files. Source behavior:

- Valid time: ISO `YYYY-MM-DD`, centered at logical y=32.
- Valid 12-hour mode: AM/PM at height−78. Wake text at height−44.
- Before epoch 1704067200 (2024-01-01), every face renders dashes and
  `Set the clock in Settings`, with no plausible analog hands or AM/PM.
- `Press PWR to wake` and the unset caption exist only in `english.yaml` at this
  source. All languages therefore use those exact English fallbacks. Captions
  preserve literal case, using the existing embedded portable paper font.
- Existing six-face rasters, approximate analog renderer, first/every-30 clean
  refresh and checked same-lease old-image reconstruction are unchanged.

The RTC remains explicit **unconverted wall time**. This slice does not change
UTC/local interpretation, time zones, RTC variant or interpretation metadata.
Language selection does not claim to translate the complete portable Settings
UI. The bounded selector uses English language names and the exact Reader codes.

## App-owned preferences

`PortableReaderPreferences.h` owns namespace-1 four-byte records:

- `reader_language`: magic 0x4c, schema 1, Reader ID 0..21, ID XOR 0xa5.
- `reader_flip_ui`: magic 0x52, schema 1, 0/1, value XOR 0xa5.

IDs, in order: EN, ES, FR, DE, CS, PT, RU, SV, RO, CA, UK, BE, IT, PL,
FI, DA, NL, TR, KK, HU, LT, SI. These come from the frozen `_order` metadata,
including Reader's `SI` code. Missing, invalid or unavailable records fall back
to English/off without writes. Save requires successful write and exact
readback; unchanged confirmed values avoid writes. Ambiguous saves remain
unconfirmed. Cancellation and Home never save a draft. Actual Reader native
language names are preserved in the source receipt.

Settings retains prior row indexes and adds `Clock face`, `Flip UI 180°` and
`Language`. The 22-choice previous/next selector is bounded and wraps; both
languages and orientation require explicit Save. Pagination supports all rows
on 480×800 and 400×600 paper displays. A tiny degree glyph is compiled only for
opted-in paper preferences; ordinary default font rendering stays unchanged.

## Orientation and lifecycle

Desk Clock and the x4-desk-clock Settings profile opt in automatically.
Other applications may explicitly compile `PORTABLE_PAPER_PREFERENCES` in a
future product cohort. Only MONO1, retaining, supported portrait paper displays
load/apply this preference. Default Watch and default paper builds do not read
these keys. No Runtime capability or provider policy is added.

Flip is an additional physical 180° rotation, composed with the existing native
90° mapping. All paper rectangle/font/icon pixels share the same transform.
Logical touch is transformed before app gestures, Home routing or QuickActions.
Physical Home/crown identity is unchanged. Orientation changes clear pending
app/touch/navigation/replay state, gate held input until neutral, discard
previous-image and modal restoration caches, and require a clean complete next
frame. Changes refuse during active QuickActions/alarm ownership or uncertain
custody. If the initial preference grant cannot be released, the exact grant
and all resources remain retained; fini and reinitialization perform no I/O.

The retained Clock config contains the actual language and flip. A mismatching
config forces a full scene. Reconstruction uses the retained minute/config and
its transform; fresh preferences never alter old pixels. No preference I/O,
yield or cleanup is introduced during prepared peripheral holds.

## Verification

- Real Clock, normal and ASan/UBSan: all six faces ×62 fresh processes for each
  unflipped/flipped orientation, exact prior-frame comparisons, changed-config
  clean fallback, invalid-time reconstruction, 12/24-hour and retained/error
  cases. First/every-30 cadence remains asserted.
- Real Settings: 254 normal/sanitized controller executions including all 22
  IDs, every save failure type, invalid records, Home, input interruption,
  explicit Cancel, held-contact gates, repeated pixel-exact flips, touch-driven
  flip back, short display, unsupported display and all four retained editors.
- Flipped ordinary Clock/launcher: 124 normal/sanitized native/rotated paper
  QuickActions, replay/held Home, alarm interruption, navigation and background
  restoration cases.
- Existing portable apps, Nova/Watch Settings, paper Settings, default Clock,
  desk policy/faces and actual Runtime/CpuPort retained gate suites pass.
- Actual rendered PNGs were inspected. All six flipped valid scenes are exact
  180° pixel transforms. All six invalid scenes are pixel-identical dashes;
  their flips are exact. See [clock captures](reader-evidence/) and
  [Settings contact sheet](reader-settings-evidence/contact-sheet.png).
- Pinned Xtensa GCC 8.4.0 (`esp-2021r2-patch5`) targets pass actual ELF loader
  structural validation, import allowlist and exactly three existing exports.
  No new native ABI export is introduced.
- Rebuilt against the parent source, default paper Clock, default paper Settings
  and Watch Settings are byte-identical. Hashes are in the verification receipt.

ASan/UBSan ran with `ASAN_OPTIONS=detect_leaks=0` because this executor runs under
ptrace, which LeakSanitizer does not support. Explicit fixture grant, frame,
subscription and retained-resource checks remain enabled. Hardware touch/panel,
power rails/current and minute timing qualification remain open.
