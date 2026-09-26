# Springboard

## Purpose

Springboard is the RiscRTE installed-application launcher. Its manifest identifies it as **Apps** (`springboard.elf`), version **1.0.0**, minimum firmware **1.1.5**, categories `System` and `Launcher`.

## User workflow

The app refreshes the installed-app inventory, displays installed applications in a paged icon grid, lets the user move selection with directional input or touch, launches the selected compatible application, and optionally pins/unpins applications to the Home list.

A short Confirm press launches the selected app. Holding Confirm for at least 700 ms toggles Home pinning. Touching a selected icon launches it; touching another icon selects it. Bottom-screen controls page backward, toggle Home membership, or page forward.

Applications whose manifest is incompatible with the running firmware remain visible but show their required firmware version and are not launched or pinned.

## RiscRTE interfaces

Springboard uses `T5AppApi` and optionally `T5StorageApi`.

Required `t5_app_api_v1` functions include screen geometry, `clear`, `draw_text`, `fill_rect`, `present`, `poll`, `millis`, installed-app discovery functions, `request_app_launch`, `draw_icon`, and `draw_label`. It checks `struct_size` through the `draw_label` member before use.

Installed apps are obtained through `installed_apps_refresh()`, `installed_apps_count()`, and `installed_apps_get()`. Launch is a host handoff via `request_app_launch(index)`; Springboard returns after a successful launch request rather than recursively loading another ELF.

The storage API is treated as optional. When present, Springboard requires `exists`, `read_file`, and `write_file_atomic`.

## Persistence

Pinned Home apps are stored at:

`/sd/Apps/.home_apps`

The file is a newline-delimited list of app ELF filenames. The implementation supports at most 128 tracked installed apps and allocates a maximum pin-file payload of `128 * 128` bytes.

If the storage API is unavailable, launching still works but Home pinning is disabled.

## Layout and rendering

Grid geometry is calculated from the current screen size. Displays at least 700 pixels wide use four columns; narrower displays use three. Row count is derived from the available vertical area. Icons are rendered through the shared Font Awesome host renderer.

Springboard draws its own selection and pagination geometry; firmware provides drawing primitives and installed-app metadata.

## Failure handling

Missing Font Awesome glyphs are reported in the status area. Failed pin persistence restores the previous pin state. Failed launch requests display an error. No-installed-app state instructs the user to place matching `.elf` + `.json` pairs in `/Apps` on the SD card.

## Source

- `Apps/springboard.c`
- `Apps/springboard.json`
