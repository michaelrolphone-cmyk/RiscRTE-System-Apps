# Settings

Current manifest version: **1.0.1** (`settings.elf`; minimum firmware 1.1.24).

## Purpose

Settings is the native RiscRTE front end for the firmware-owned settings model. Its manifest identifies `settings.elf`; the ELF handles high-level navigation while firmware owns setting definitions, rendering, activation, persistence, and complex setting activities.

## RiscRTE interface

The app uses `T5AppApi` only. It requires the append-only settings portion of `t5_app_api_v1` through `settings_touch` and verifies the corresponding `struct_size`.

Required functions are:

- `set_back_exits_app`
- `settings_category_count`
- `settings_category_get`
- `settings_count`
- `settings_get`
- `settings_activate`
- `settings_render`
- `settings_touch`

The app does not duplicate firmware setting schemas or persistence logic.

## Navigation model

Selection index 0 represents the category tab bar; indexes 1..N represent setting rows in the active category.

- Left/Right changes category.
- Up/Down moves between the category bar and rows.
- Confirm on the category bar advances category.
- Confirm on a setting calls `settings_activate`.
- Touch is delegated to `settings_touch` and then re-rendered.
- Back from a setting returns to the category bar; Back from the category bar exits.

The app disables the default Back-to-exit behavior using `set_back_exits_app(false)` so Back can be used for internal navigation. Host Power/Home exit behavior remains outside the app.

## Complex setting actions

When `settings_activate` or `settings_touch` returns `T5_APP_SETTING_ACTION_REQUESTED`, the app exits immediately. This allows the firmware host to unload the settings ELF, run the requested firmware-owned activity, and later resume/relaunch according to host behavior.

## Compatibility behavior

If the settings API is not present at the required structure size, the app renders an explicit incompatible-firmware screen when basic drawing functions are available.

## Source

- `Apps/settings.c`
- `Apps/settings.json`


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.1**; the application C source is synchronized without source edits. Source blob `0e2b5fb00c877837789e503e91a4a220f10c83e1`; manifest blob `984129ec3823717c6476f9fea70e5d079549b5e7`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-settings-v1.0.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-settings-v1.0.1/application-settings-1.0.1-xtensa-esp32s3.rte.zip) has a **3675**-byte package with SHA-256 `4759acc4cee47963f7f46bdf323eae8eaf91a8cbc4761bb7191d7ee9e258fa1c`. Its embedded `settings.elf` is **2652** bytes with SHA-256 `57353530d9d8233fce0686a8fb94e3407e3a36292b5a2a5778b1df769bf99ff2`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.

## Portable time-format selector (development 1.2.1)

The portable Settings client now activates the existing Time Format row. Touch
selects 12-hour or 24-hour, navigation changes the draft selection, and an
explicit Save confirms it. Back, right-swipe Back, and Cancel discard the draft.
Time previews and the hour editor use the last confirmed format; RTC civil
values and the existing timezone conversion are unchanged.

`PortableTimeFormat.h` defines the shared app-side record, consumed separately
by Watch Clock: the existing `storage.key-value@1` instance **1** (app-settings),
key `time_format`, exactly four bytes `{0x54, 1, mode, mode ^ 0xa5}`. Mode 0 is
12-hour and mode 1 is 24-hour. Missing, invalid or unavailable records default
to 12-hour without writing. A save of the same effective mode also does not
write, including a missing or invalid record's 12-hour default. Save refuses a
fresh unreadable record, and a changed save requires successful put plus exact
readback. No Runtime API, SDK or timezone policy is introduced.

The client shares one namespace-1 grant with Clock Sleep Mode when enabled.
If the grant/read is unavailable, Settings still opens and reports unavailable
storage. A failed write or readback preserves the last confirmed in-session
format and displays `Save unconfirmed - retry`; the KV ABI explicitly permits
a failed put to have committed, so this does **not** promise that old bytes
survived in storage. A later successful read establishes the persisted choice.
No rollback writes are attempted.

This main-based Settings-only development increment does not contain the
separate alarm foreground/alert-mode work reserved as 1.2.0. It is not a release,
a Watch firmware build, or hardware qualification.

Validation: `scripts/test_portable_apps.py` exercises the production selector,
shared record and restart consumer at midnight, noon and 23:59; missing/corrupt
records; read/put/readback failures; cancel/back; held confirmation; navigation;
180-degree touch rotation; unchanged Denver conversion; and the existing sleep
fixtures. The pinned target builder validates ELF structure/imports/exports.
