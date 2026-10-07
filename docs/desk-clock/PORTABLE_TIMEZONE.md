# Isolated app-owned timezone core

This change prepares Reader-equivalent timezone policy for an opt-in X4 desk
clock. It adds no Settings/Clock/adapter wiring, product image, default activation,
Runtime service, native import, driver contract or RTC chip layout. Existing
Watch Denver/default policy files are byte-for-byte unchanged.

## Source and license

The 419 ID/POSIX/region triples, including order, are copied from
[T5S3-Reader 34d8e694d89a1e72d8854403d8592c289fae3ddc](https://github.com/michaelrolphone-cmyk/T5S3-Reader/tree/34d8e694d89a1e72d8854403d8592c289fae3ddc)
`lib/hal/TimeZoneData.cpp`. Catalog behavior is ported from
`lib/hal/TimeZoneCatalog.{h,cpp}`; RTC interpretation derives from
`lib/hal/HalClock.cpp` lines 102–115 and 165–218. The root MIT LICENSE retains
Copyright (c) 2025 Dave Allie. Exact source Git blobs, SHA-256 values and preserved
Watch-file hashes are recorded in `lib/PortableApps/time/TIMEZONE_PROVENANCE.json`.

The frozen generator selected representative **2026 recurring POSIX rules**.
This is not a historical IANA timezone database, an up-to-date legal rules
service, or a guarantee against future timezone-law changes. For example,
Casablanca's frozen catalog entry is a fixed +01 rule. The port intentionally
preserves these Reader values instead of silently substituting host tzdata.

Reproduce/verify the exact import from a read-only Reader checkout:

```sh
python scripts/generate_portable_timezones.py --reader /path/to/Reader --check
```

The converter/parser/calendar code is new standalone C. It does not copy
Reader's global `TZ`/`tzset`/`mktime` implementation. The calendar arithmetic is
integer Gregorian era decomposition. No external time/date implementation or
additional license is bundled.

## C interface and validity

Include `PortableTimeZone.h` and compile `PortableTimeZone.c` plus
`PortableTimeZoneCatalog.c`. Include/compile `PortableTimeZonePreference.h/.c`
only for app-owned preferences. No heap, global mutable state, current clock,
process environment, OS timezone APIs or hidden cache is used.

- `find(id, capacity)` validates a fully terminated ID shorter than 40 bytes.
  `Etc/UTC` and `Etc/GMT` canonicalize to catalog entry zero, `UTC`.
- `resolve` returns `FALLBACK` for unknown, unterminated or missing IDs while
  supplying a usable UTC0 rule. Validation and safe fallback remain distinct.
- `parse(rule, capacity)` requires a full NUL-terminated rule shorter than 96
  bytes. It supports every actual frozen form: alphabetic and angle-bracket
  abbreviations (including Reader numeric/colon names), signed h[:m[:s]] offsets,
  optional daylight offset and `Mm.w.d[/time]` transitions. Missing daylight
  offset means +3600 seconds. Missing transition time means 02:00 local wall
  time **before** that transition. Signed transition times up to 167:59:59 are
  supported; unsupported J/n rules, incomplete strings and trailing data fail.
- Offsets exposed by the API are seconds **east of UTC**, reversing POSIX signs.
  Half-hour, quarter-hour, half-hour DST and non-one-hour DST work directly.
- Civil input/output is Gregorian 1600–9999, seconds 0–59. No leap-second
  normalization. Input weekday is ignored; output weekday is recomputed with
  Sunday=0. Epochs are signed 64-bit seconds since 1970-01-01 UTC.
- `utc_to_local` returns `RANGE` if UTC or resulting local time is outside that
  domain. It uses adjacent-year transitions for year crossings and southern
  seasons. Checked arithmetic never evaluates unbounded epoch additions.
- `local_to_utc` returns OK/one candidate, FOLD/two ascending UTC candidates, or
  GAP/zero candidates. It verifies possible offsets through the forward
  converter. It never silently picks a fold or shifts a nonexistent time.
  Invalid fields, rules and domain edges have distinct negative errors.
- Parsing and forward conversion only publish outputs on success. Inverse
  conversion clears `count` on errors; candidates beyond `count` are unspecified.
- `region_count` and `city_index` enumerate the complete catalog. America exceeds
  the old 96-row UI limit; callers must paginate or scroll without truncating.
  Invalid catalog/city indices return NULL/-1, not a deceptively valid UTC row.

## RTC storage interpretation

`interpret_rtc` receives already decoded calendar fields, a `stores_utc` hint,
a uint32 reference epoch, and an explicit fold choice (or -1 to reject local
ambiguity). The provider retains chip register, validity-bit and layout ownership.

UTC and local-calendar interpretations are considered usable from
2000-01-01 (946684800). A reference is used only if also usable. If both are
usable, a mode switches only when one absolute distance plus **1800 seconds is
strictly less** than the other. Equality does not switch. Without a usable
reference, the hint is kept unless exactly one interpretation is usable. The
2024 valid-time threshold is separately exposed for callers matching Reader's
RTC-write policy; it does not replace the boot usable threshold.

An unresolved fold/gap cannot supply a usable local candidate; a valid UTC
interpretation can still be selected by the one-usable rule. The result exposes
`local_status`, both candidate usability flags, selected mode and `mode_changed`;
callers can require an explicit decision rather than hide the ambiguity. An
explicit fold choice allows either UTC candidate. Both interpretations unusable
returns `UNUSABLE` with diagnostics. Other invalid inputs leave output untouched.

This is a **pure decision**. It does not set system time, write the RTC, save a
reference/mode, sync the network, or apply drift calibration. The Reader reference
is a migration-disambiguation anchor, never an invented calibration setting.

## Preference contract

The caller supplies its existing `storage.key-value@1` grant for namespace
instance 1. This code does not acquire grants, inspect namespaces or extend ABI.
Key `time_zone` stores an opaque 44-byte record: `T`, `Z`, version 1, one XOR
integrity byte (seed 0xa5 over all other bytes), and a 40-byte canonical IANA ID
with zero padding. This checksum detects accidental corruption, not tampering.
All records are below the existing 64-byte provider limit.

Load returns LOADED, MISSING, INVALID_RECORD or UNAVAILABLE and defaults to UTC
without ever writing. It rejects missing terminators, unknown IDs, noncanonical
aliases, nonzero padding, bad size/version/checksum and backend read errors.

Save is for **explicit user-confirmed selection**. A matching loaded record
returns UNCHANGED. Missing UTC returns VIRTUAL_DEFAULT and does not claim persisted
UTC. An explicit valid selection can replace an invalid record, including with
UTC. Unknown IDs are rejected; valid UTC aliases are canonicalized. The helper
requires both put=OK and exact get readback. Put=IO is always WRITE_FAILED even
if the data happened to commit; failed, malformed or stale readback is
VERIFY_FAILED. There is no rollback, hidden repair, automatic default persistence
or claim that old storage survived an IO-after-commit failure.

## Verification and integration boundaries

```sh
python scripts/test_portable_timezone.py \
  --xtensa-cc /path/to/pinned/xtensa-esp32s3-elf-gcc \
  --evidence docs/desk-clock/timezone-test-evidence.json
```

The script bounds generated files within a temporary directory and deletes its
own outputs when done. Evidence records:

- Plain and ASan/UBSan C tests: 419 entries, 55,308 round trips, 8,400 leap/year
  edges, all ID/rule truncation lengths, known northern/southern boundaries,
  repeated/skipped times, half/quarter-hour offsets, invalid records and
  save/readback faults, and strict migration-threshold boundaries.
- 22,653 independent Python regex + Gregorian calendar vectors across every
  catalog entry, its actual recurring transitions ±1 second, seasonal dates and
  forward/inverse fold/gap candidates in nine representative years.
- Pinned Xtensa GCC 8.4.0 esp-2021r2-patch5 objects and a link-only shared ELF,
  stripped and accepted by the repository's actual ELF structural validator.
  Dynamic imports are limited to already-exported `memcpy`/`memset`. No compiler
  64-bit divide helper or blanket libgcc linkage is needed.
- Preserved Watch policy SHA-256 checks plus unchanged default/Denver host
  regressions in plain and ASan/UBSan modes (876,600 / 876,585 hourly round trips).
- All 54 repository unittest contracts pass, including frozen catalog/import checks.

LeakSanitizer is disabled for the executor's ptrace constraint; address and
undefined-behavior sanitizers run. The link harness is not a product app and does
not prove execution on Xtensa silicon, RTC electrical behavior, clock accuracy,
deep-sleep retention, Settings rendering or end-to-end device integration.
Those remain the responsibilities of the later opt-in app/provider integration.
