# Opt-in app-owned Time Zone Settings

This is a new implementation from public base
`3bcc9b3accbd8169f0441a6874114127a69507d0`. It reconstructs the requested
functionality after the unpublished selector source was lost; it is not a
byte-identical recovery of that lost source.

## Bounded integration

Only `PORTABLE_SETTINGS_TIME_ZONE` activates the existing **Time Zone** row as
an editor. The ordinary Settings builders, manifests, product selections,
version numbers and BINs are unchanged. Settings 1.3.7 remains reserved for a
future integration. There is no RTC write, RTC-basis change, Clock wiring,
provider change, new grant, import, catalog entry, font or shared presentation
change in this slice.

The editor reuses the existing namespace-1 KV grant and the unchanged
`PortableTimeZonePreference` helper at key `time_zone`. All 419 canonical catalog
entries remain available in the Reader-derived order, through 11 regions and
paged city lists. The complete city name, including subordinate paths, wraps
without truncation. The region heading provides the first path component.

For isolated development links, define `PORTABLE_SETTINGS_TIME_ZONE` on the
production adapter and link `PortableTimeZone.c`, `PortableTimeZoneCatalog.c`
and `PortableTimeZonePreference.c`. `scripts/test_timezone_settings.py` performs
that link explicitly. The standard builders do not enable or link the feature.

## Interaction and persistence

- Touch a region to open its cities. Touching a city changes the draft only.
- Up/Down moves the selection with wrap; Left/Right and PREV/NEXT page with
  bounded endpoints. Vertical swipes page. The counters include partial last
  pages, including America's first Adak and last Yakutat entries.
- Confirm opens the selected region. On the city page, explicit Save/Confirm
  saves the current selected draft. Neutral gating prevents a held opening
  Confirm from reaching Save in the next page.
- BACK/right swipe from cities returns to regions. Back from regions exits.
  CANCEL exits the whole editor from either level. Physical/touch Home leaves
  Settings. None writes a draft. No cancellation claims to undo a prior
  uncertain Save.
- Loads are read-only. Missing storage presents a virtual UTC default. Explicit
  UTC Save with no record reports **UTC default (not saved)**. Corrupt storage
  stays invalid until explicit Save; selecting UTC may repair it.
- An unavailable table/read/missing put method before a write reports
  **Storage unavailable**. A failed put, IO-after-commit, or failed/mismatching
  readback reports **Save unconfirmed - retry** and keeps the draft.
- The root row stays **Unconfirmed** for the invocation after an uncertain Save,
  including Cancel/reopen and later read unavailability. Only a fresh explicit
  Save/Confirm followed by the helper's successful verification clears it.
  There is no automatic write retry or rollback.

The UI supports the existing 480×800 and 400×600 retaining paper profiles and
240×240 compact display. All code is removed by preprocessing when the opt-in
flag is absent. The preserved default Watch/Denver policy and timezone core
remain unchanged.

## Reproducible checks

```sh
ASAN_OPTIONS=detect_leaks=0 python scripts/test_timezone_settings.py \
  --xtensa-cc /path/to/shared/xtensa-esp32s3-elf-gcc \
  --pixels --evidence build/timezone-settings/evidence.json
python -m unittest discover -s tests -v
ASAN_OPTIONS=detect_leaks=0 python scripts/test_portable_apps.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_settings_paper.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_desk_clock_settings.py
```

The production `Apps/settings.c` and adapter/controller/rasterizer run with fake
capability providers. Both navigation and touch reach all 419 choices in paper,
short paper, compact and 180-degree compact profiles, plain and ASan/UBSan.
Another 28 cases per profile/configuration cover actual root entry, page and
selection wrap/clamps, cancellation, both Home sources, no implicit invalid-record
repair, explicit UTC repair, held Confirm across pages, interrupted/replaced
contacts, real swipes, all save failure phases and explicit later retry. Total:
6,928 controller executions. Every execution checks that no RTC or unrelated
preference was written and acquired providers clean up.

The runner builds with pinned Xtensa GCC 8.4.0 esp-2021r2-patch5 and checks
the portable Settings import contract, exact three entrypoint exports and the
actual ELF structural validator for every current Watch, paper and desk-profile
feature-off ELF, its erased counterpart, and the enabled development ELF.

The current-source isolation witness copies `Apps/` and `lib/` into a temporary
directory, physically erases the explicit timezone branches in `settings.inc`
and `settings_view.inc` (retaining their ordinary `#else` branches), and replaces
the two selector includes, three timezone C modules and two public headers with
`#error` poison. With the feature flag **enabled in that erased copy**, both
preprocessed Settings/adapter source and linked ELF bytes must exactly match
the unmodified current source with the feature flag absent. Selector identifiers
are independently forbidden in the off preprocessed source and ELF symbols.
Before erasure, enabling each poisoned profile must fail on the poison marker;
this positive control proves the feature gate actually reaches the files being
excluded. The ordinary enabled build must retain selector and persistence
symbols and pass target validation. Unit tests reject leaked selector names,
unguarded poisoned includes, byte mismatches and unsupported erasure syntax.

The original `timezone-settings-evidence/evidence.json` remains an unchanged
historical receipt: it records exact equality with public base `3bcc9b3...`
for the sources and compiler used at that integration. It is not an unconditional
whole-binary invariant for future shared-adapter work. The current receipt
includes its hash, explicitly states `replayed_this_run: false`, compares
compiler and source hashes, and reports each current ELF's equality or inequality
with its recorded historical hash without claiming a fresh base replay.
The current-source proof needs no historical Git object or network fetch.

For the 2026-10-10 decoupling source, the compiler executable still matches the
historical SHA-256 `732bebd44b70a21687ffa3ac5da55edc9b555080ad21cfd74503b2288a143d3f`.
`Apps/settings.c` and all three timezone core modules are unchanged; the adapter,
Settings integration and selector includes have changed. Watch/paper retain
manifest version 1.3.4, while the desk profile advanced from 1.3.5 to 1.3.6.
In particular, the
shared adapter now separates raw input capture from ordered logical reduction
and cooperative presentation completion. These changes affect feature-off
builds too, so their whole-binary hashes correctly differ. The receipt records
the full repository-local compiler dependency closure and build definitions.

No source worktree is changed for the comparison. `--pixels` requires Pillow and captures real
production framebuffers with lossless PNG pixel-hash verification; selected
frames are included in `timezone-settings-evidence/`.

LeakSanitizer is disabled only for the ptrace execution constraint. These tests
establish source/controller, raster, ELF and storage-contract behavior, not
execution on Xtensa silicon, RTC accuracy, physical touch, screen refresh quality,
boot policy, power or end-to-end product qualification. No product is enabled.
