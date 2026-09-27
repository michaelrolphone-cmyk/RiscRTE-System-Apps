# Springboard

## Purpose and scope

Springboard is the foundational RiscRTE installed-application launcher. The manifest identifies it as **Apps** (`springboard.elf`), version **1.1.0**, minimum firmware **1.1.5**, categories `System` and `Launcher`.

The source establishes three responsibilities: refresh and display the host-provided installed-app inventory, provide grid navigation and application launch handoff, and maintain the Home-screen pin list. The app does not load another ELF directly; launch requests are delegated to the host.

## User-visible workflow

At startup the app refreshes installed applications, caps the working set at 128 entries, loads Home pins when storage support is available, computes its grid, and renders the first page.

In normal mode, directional input moves selection. A short Confirm release launches the selected compatible app. Holding Confirm for at least 700 ms enters Home edit mode when storage is available. Touching an app cell selects it and immediately requests launch. The bottom controls page backward, enter **Edit**, or page forward.

In Home edit mode the title changes to **Apps - Edit Home**. Pinned apps are marked with a rounded highlight band around their cells. A short Confirm release toggles the selected app's Home membership; touching an app cell selects it and toggles membership. The center bottom control becomes **Done**. Holding Confirm again also toggles edit mode.

Home edit mode is unavailable without the optional storage API. Incompatible apps remain visible but cannot be launched or pinned; the status displays the required firmware version.

## Manifest metadata

Confirmed from `Apps/springboard.json`:
- version: `1.1.0`
- minimum firmware: `1.1.5`
- display name: `Apps`
- ELF: `springboard.elf`
- icon: `solid:f00a`
- categories: `System`, `Launcher`

No capability list is declared by this manifest.

## Host interfaces

Springboard includes `T5AppApi.h` and `T5StorageApi.h`.

It obtains `t5_app_api_v1` through `t5_app_get_api(T5_APP_ABI_VERSION)`, verifies `struct_size` through `draw_label`, and requires screen geometry, clear/text/rectangle/present drawing, input polling, `millis`, installed-app refresh/count/get, launch request, icon drawing, and label drawing.

Installed apps are read through `installed_apps_refresh`, `installed_apps_count`, and `installed_apps_get`. The app consumes the manifest filename, display name, icon, compatibility flag, and minimum-firmware string. A successful `request_app_launch(selected)` causes Springboard to return from `app_main`.

Input is polled with a 20 ms wait and uses directional buttons, Confirm press/release/hold timing, taps, touch coordinates, and the exit-request flag.

The optional `t5_storage_api_v1` is requested through `t5_storage_get_api(T5_STORAGE_API_VERSION)`. The app requires the provider structure through `write_file_atomic` and uses `exists`, `read_file`, and `write_file_atomic`.

## Persistence

Home pins are stored in `/sd/Apps/.home_apps` as a newline-delimited list of application ELF filenames. Each loaded line is matched exactly to an installed app's `file_name`.

The implementation tracks at most 128 installed apps and limits the pin payload to `128 * 128` bytes. Saves use atomic-write semantics supplied by the storage provider. On save failure the previous in-memory pin state is restored.

## Layout and rendering

Displays at least 700 pixels wide use four columns; narrower displays use three. Row count is calculated from the remaining vertical space and has a minimum of one. The app draws rounded icon boxes and the edit-mode pin highlight with host rectangle primitives. App icons and labels are rendered through shared host functions.

If any app icon cannot be drawn, the footer reports that some Font Awesome icons are unavailable.

## Failure handling

Confirmed behavior:
- a missing required App API causes an immediate return;
- missing storage disables Home editing but not launching;
- incompatible apps cannot be launched or pinned;
- launch-request failure is reported on screen;
- pin-save failure restores the previous state;
- an empty installed-app inventory is reported with instructions to place matching ELF and JSON pairs in the SD Apps directory;
- `exit_requested` exits the app.

## Network and hardware

The source performs no network operations and contains no direct external-hardware access. Discovery, drawing, input, launch scheduling, and storage are host/provider responsibilities.

## Source files

- `Apps/springboard.c`
- `Apps/springboard.json`

Anything outside these source/interface boundaries is not established here.
