# SD Firmware Update

## Purpose and classification

SD Firmware Update is the firmware-maintenance application for installing a firmware image selected from SD storage. It is classified as a foundational System App because it performs the device's core firmware-update workflow rather than a general file-management or MCU-development task.

Manifest metadata:
- version **1.0.1**
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
