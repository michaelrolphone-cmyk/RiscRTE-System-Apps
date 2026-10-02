# SD Firmware Update

## Purpose and classification

SD Firmware Update is the firmware-maintenance application for installing a firmware image selected from SD storage. It is classified as a foundational System App because it performs the device's core firmware-update workflow rather than a general file-management or MCU-development task.

Manifest metadata:
- version **1.0.2**
- minimum firmware **1.1.23**
- artifact `sd_firmware_update.elf`
- icon `solid:f021`
- categories `Firmware`, `Files`
- supported file type `.bin`

## Host interfaces

### `T5AppApi`

The app requires `poll` and uses raw app input for confirmation, cancellation, and dismissal loops. Poll waits are 50 ms.

### `T5SdFirmwareApi`

The app obtains the selected image path with `selected_path`, validates it with `validate`, installs it with `install(progress_callback, context)`, and requests restart with `restart_after_update` after success.

The progress callback reads `image_size()` and `written_size()` and redraws only when the integer percentage changes. The source assumes those two progress accessors exist when the install callback is invoked; `app_main` does not separately null-check them.

### `T5UiApi`

The app requires `render_list` and `hit_test`. All screens are one-row list views with firmware chrome.

## User workflow

1. The firmware-provided selected path is copied into a fixed `T5_SD_FIRMWARE_PATH_MAX` buffer. If no selected path is available, the app returns.
2. A **Validating firmware** screen is shown and `fw->validate()` is called.
3. Validation failure shows **Update failed** with a source-mapped error string and waits for exit, tap, Back, or Confirm.
4. Successful validation shows **Update firmware?** with the selected file's basename and requires Confirm or a valid row tap. Back cancels.
5. The app calls `fw->install(render_progress, NULL)`. During installation it shows percentage and written/total byte counts and explicitly warns not to power off.
6. Installation failure shows a dismissible failure screen.
7. Installation success shows **Update complete / Restarting** and calls `restart_after_update()`.

## Error mapping

The source maps provider result values to user-visible text for file-open failure, image-too-large, image-too-small, write failure, and invalid image. Other non-success values use the generic **Firmware update unavailable** text.

## Storage, network, and ownership

The app performs no direct file reads or writes and implements no flash-writing algorithm itself. Image selection, validation, firmware write semantics, integrity rules, image-size limits, and restart behavior belong to `T5SdFirmwareApi`.

The source performs no network operations and defines no persistent app-specific state.

## Buffers and rendering constraints

- selected path buffer: `T5_SD_FIRMWARE_PATH_MAX`
- status text buffer: 96 bytes
- progress redraw suppression: no redraw when the integer percent is unchanged
- basename display: derived by taking text after the final `/`

## Source identity

At audited upstream commit `ff08d329489c62af107c906036d0a926f1a0241f`:
- `Apps/sd_firmware_update.c`: `060753f86e85022c901f6f6b5c0542a9daf4eb7f`
- `Apps/sd_firmware_update.json`: `e445e18c14ef70dd9280897c62b4380400d1b0b6`


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.2**; the application C source is synchronized without source edits. Source blob `060753f86e85022c901f6f6b5c0542a9daf4eb7f`; manifest blob `d8dbd0821ab0116a5d6d9174a11955f26155638e`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-sd_firmware_update-v1.0.2`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-sd_firmware_update-v1.0.2/application-sd_firmware_update-1.0.2-xtensa-esp32s3.rte.zip) has a **5494**-byte package with SHA-256 `fe630b7736149bb7466c6efc72a90d41b2cb25c1838d6862477bf448d0d1d6b0`. Its embedded `sd_firmware_update.elf` is **4340** bytes with SHA-256 `3a5c85210a75fc9051ff3eeff9ea6d8661d1dfdafdc62125f0d50fde065ce528`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
