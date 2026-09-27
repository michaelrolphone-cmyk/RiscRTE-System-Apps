# Springboard

## Purpose and scope

Springboard is the foundational RiscRTE installed-application launcher. The current manifest identifies it as **Apps** (`springboard.elf`), version **1.2.2**, minimum firmware **1.3.17**, categories `System` and `Launcher`.

The app refreshes and displays the host-provided installed-app inventory, owns grid navigation and launch handoff, and maintains the Home-screen pin list. It does not map or execute another ELF directly; launch is delegated through the host API.

## Startup and inventory

At startup the app obtains `T5AppApi` and `T5StorageApi`. It validates the App API through the append-only `fill_rounded_rect_tone` member, refreshes installed applications, caps the working set at **128** entries, loads Home pins when storage is available, computes grid geometry, clears edit/selection state, and renders.

A missing required App API causes an immediate return. Storage remains optional for ordinary launch; Home editing is unavailable without the required storage members.

## Navigation and launch behavior

Displays at least 700 pixels wide use four columns; narrower displays use three. Row count is derived from remaining vertical space and never falls below one. The current page is `selected / page_size`; page count is rounded up from the installed-app count.

Physical directional navigation changes `selected` and makes the selection underline visible. A short Confirm release launches the selected compatible app in normal mode. Holding Confirm for at least **700 ms** toggles Home edit mode.

Touch behavior:
- tapping an app cell launches immediately in normal mode or toggles its Home pin in edit mode;
- touch clears the physical-navigation underline;
- the top-right rounded **EDIT/DONE** control toggles edit mode;
- tapping the page-dot region advances to the next page and wraps to page zero.

Version 1.2.2 retains the 1.2.x top-right edit control and bottom page dots. The compact edit-label vertical position is optically adjusted by drawing the label at `y + 3`.

## Home edit mode and persistence

Pinned apps are shown with a rounded black/white cell highlight. A short Confirm release toggles the selected app's Home membership; tapping an app cell toggles it immediately. Entering or leaving edit mode clears navigation-selection visibility.

Incompatible apps remain visible but cannot be launched or pinned. The app reports the minimum required firmware instead.

Pins are stored in `/sd/Apps/.home_apps` as newline-delimited application ELF filenames. Lines are matched exactly against installed manifest `file_name`. The app tracks at most 128 installed apps and bounds the payload to `128 * 128` bytes. Provider-owned atomic write is used; save failure restores the previous in-memory pin state.

## Host interfaces

### `T5AppApi`

The app now requires the API structure to extend through `fill_rounded_rect_tone` and requires:
- screen width/height;
- clear, text, rectangle, icon, label, and present drawing;
- `poll` and `millis`;
- installed-app refresh/count/get;
- `request_app_launch`;
- `fill_rounded_rect_tone`.

The current ABI remains version 1 but adds four tone constants—white, light gray, dark gray, black—and the append-only rounded-tone primitive. Springboard uses this firmware primitive rather than app-owned stippling.

### `T5StorageApi`

The optional storage interface is accepted only when its structure reaches `write_file_atomic` and exposes `exists`, `read_file`, and `write_file_atomic`.

## Grayscale glossy icon rendering

Version 1.2.2 increases the icon tile box to 82 pixels and constructs a raised glossy frame from multiple rounded host-tone rectangles:
- black outer lip;
- light-gray metallic rim;
- white highlight layer;
- dark-gray bevel;
- black inner face;
- dark/light/white upper reflection;
- dark-gray lower and side reflection.

The actual app icon is drawn in white on the black inner face. These are semantic grayscale tones supplied to the host; the app does not allocate or own grayscale bitplanes. The new `T5AppApi.fill_rounded_rect_tone` dependency is why the manifest now requires firmware **1.3.17**.

App labels move downward to account for the larger icon tile. The conditional physical-navigation underline, edit-mode pin highlight, status warning, and page-dot rendering remain separate monochrome primitives.

## Failure handling

Confirmed behavior:
- missing required App API operations returns immediately;
- missing storage disables Home editing only;
- incompatible apps cannot be launched or pinned;
- launch-request failure is reported on screen;
- pin-save failure restores the prior pin state;
- an empty inventory displays instructions to put matching ELF/JSON pairs in `/Apps` on SD;
- `exit_requested` exits.

## Network and hardware

The source performs no network I/O and no direct external-hardware access. Discovery, grayscale composition, drawing, input, launch scheduling, and storage are host/provider responsibilities.

## Source identity

At audited upstream commit `525e32689203502a7b22f6350b7ef04f272271db`:
- `Apps/springboard.c`: `671a1a9894359cdbd799c65933a10665e3d93a2d`
- `Apps/springboard.json`: `300114bdd5face6568423146ac3e874142557efd`
- current `T5AppApi.h`: `fda810300de5cadff16e81efd42ba7efff8fe33b`

Published upstream release `app-springboard-v1.2.2` contains `springboard.elf` at **9,044 bytes**, SHA-256 `afeb6ebd179eda46dcfcae1fd57f54fda3f81298a99f507ebe7b762b211bd4d2`. This is an upstream artifact target; no destination rebuild is claimed here.
