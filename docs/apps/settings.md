# Settings

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
