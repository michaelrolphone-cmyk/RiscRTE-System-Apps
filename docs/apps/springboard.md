# Springboard

## Purpose and scope

Springboard is the foundational RiscRTE installed-application launcher. The manifest identifies it as **Apps** (`springboard.elf`), version **1.2.0**, minimum firmware **1.1.5**, categories `System` and `Launcher`.

The source refreshes and displays the host-provided installed-app inventory, owns grid navigation and launch handoff, and maintains the Home-screen pin list. It does not load another ELF directly; launches are delegated through the host API.

## Startup and inventory

At startup the app obtains `T5AppApi` and the optional `T5StorageApi`. It refreshes installed applications, caps the working set at **128** entries, loads Home pins when storage is available, computes grid geometry, clears edit/selection state, and renders.

A missing required App API returns immediately. Storage is optional: launch remains usable without it, but Home editing is disabled.

## Navigation and launch behavior

Displays at least 700 pixels wide use four columns; narrower displays use three. The row count is derived from remaining vertical space and is never below one. The current page is `selected / page_size`; total pages are rounded up from the installed-app count.

Physical directional navigation changes `selected` and makes the selection underline visible. A short Confirm release launches the selected compatible app in normal mode. Holding Confirm for at least **700 ms** toggles Home edit mode.

Touch behaves differently from physical navigation:
- tapping an app cell launches it immediately in normal mode or toggles its Home pin in edit mode;
- touch does not leave the navigation underline visible;
- the top-right rounded **EDIT/DONE** button toggles edit mode;
- the page-dot hit region at the bottom advances to the next page, wrapping to page zero.

Version 1.2.0 removes the old bottom **Previous / Edit / Next** text controls and the always-visible launcher title/help footer. Page position is represented by centered page dots; the current page dot is larger. Status text is rendered near the bottom only for an explicit status or an icon-rendering warning.

## Home edit mode

Pinned apps are shown with a rounded outline/highlight around their cells. A short Confirm release toggles the selected app's Home membership; tapping an app cell toggles that app immediately. Entering or leaving edit mode clears navigation-selection visibility.

Incompatible apps remain visible but cannot be launched or pinned. Attempting either displays the required firmware version.

## Host interfaces

### `T5AppApi`

The app validates `struct_size` through `draw_label` and requires:
- screen width/height;
- clear, text, rectangle, icon, label, and present drawing;
- `poll` and `millis`;
- installed-app refresh/count/get;
- `request_app_launch`.

Installed app manifests supply file name, display name, icon, compatibility state, and minimum firmware. A successful launch request causes Springboard to return.

Input polling uses a 20 ms wait and consumes directional buttons, Confirm press/release/hold timing, taps, touch coordinates, and `exit_requested`.

### `T5StorageApi`

The optional storage table is accepted only when its structure reaches `write_file_atomic` and exposes `exists`, `read_file`, and `write_file_atomic`.

## Persistence

Home pins are stored in `/sd/Apps/.home_apps` as newline-delimited application ELF filenames. Loaded lines are matched exactly to installed manifest `file_name` values.

The implementation tracks at most 128 installed apps and bounds the pin payload to `128 * 128` bytes. Saving uses provider-owned atomic writes. On save failure the prior in-memory pin state is restored.

## Rendering details and limits

The app draws rounded icon boxes using its own rectangle helper and uses host `draw_icon`/ `draw_label` for icons and names. Edit-mode pin outlines, the top-right edit button, and page dots are also composed from host rectangle primitives.

The navigation underline is now conditional on `selection_visible`, which is set by physical directional navigation and cleared by touch/page/edit actions. If an app icon cannot be drawn, the bottom status reports that some Font Awesome icons are unavailable.

## Failure handling

Confirmed behavior:
- missing required App API operations returns immediately;
- missing storage disables Home editing only;
- incompatible apps cannot be launched or pinned;
- launch-request failure is reported on screen;
- pin-save failure restores the previous pin state;
- an empty inventory displays instructions to place matching ELF/JSON pairs in `/Apps` on SD;
- `exit_requested` exits.

## Network and hardware

The source contains no network operations and no direct external-hardware access. Discovery, drawing, input, launch scheduling, and storage are host/provider responsibilities.

## Source identity

At audited upstream commit `ff08d329489c62af107c906036d0a926f1a0241f`:
- `Apps/springboard.c`: `171df81857d694aa6cb761178f294df9da2b379b`
- `Apps/springboard.json`: `ab70214eb49ec0c98d3c9f776efd91c1a70b148b`
