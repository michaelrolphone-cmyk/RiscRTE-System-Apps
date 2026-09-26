# File Browser

## Purpose and scope

File Browser is the foundational SD-card navigation application. It enumerates the SD volume, presents directories before files using a case-insensitive natural sort, opens directories in place, dispatches ordinary files to registered handlers, launches native ELF applications, and supports file deletion through a firmware-owned confirmation handoff.

The implementation is bounded to 256 visible directory entries at a time and 512-byte path buffers. It does not implement arbitrary filesystem mutation beyond the firmware-provided delete operation used for selected files.

## Manifest metadata

- Display name: **File Browser**
- ELF: `file_browser.elf`
- Version: **1.1.0**
- Minimum firmware: **1.2.85**
- Icon: `solid:f07c`
- Manifest categories: `Files`, `Utilities`
- Manifest-declared optional capabilities: none

This repository classifies File Browser as a **foundational system app** because browsing files, launching ELF files, and dispatching registered file handlers are part of the base device workflow.

## Runtime interfaces

The source imports six versioned RiscRTE interfaces.

### T5AppApi ABI 1

Used for `dir_open`, `dir_next`, and `dir_close` to enumerate SD directories through the native VFS namespace, plus `set_back_exits_app(false)` so Back can navigate upward inside the browser instead of immediately terminating the ELF.

### T5StorageApi API 1

Used only for File Browser handoff/session state through `read_file`, `write_file_atomic`, and `remove_file`.

The session file is:

`/sd/System/State/Applications/file_browser/Session.txt`

Its current serialized format is three newline-delimited fields: current logical path, selected entry name, and pending delete path. This allows firmware to unload File Browser for a handoff and later relaunch it at the previous location.

### T5SystemUiApi API 1

Uses `navigate_home()` when Back is pressed at the logical root.

### T5FileBrowserApi API 1

The browser requires:

- `show_hidden_files`
- `render`
- `poll_event`
- `page_items`
- `confirm_delete_request` / `confirm_delete_take_result`
- `delete_document`
- `open_document`
- `launch_elf_request` / `launch_elf_take_result`

The app uses the fixed handoff cookie `0x4642524f57534552` for delete confirmation and ELF launch requests.

### T5FileOpenApi API 1

Used to discover and invoke file handlers registered by the system and installed applications. The app checks both `api_version` and `struct_size`.

For an ordinary selected file it queries `handler_count(path)`, reads up to eight handlers with `handler_get`, directly selects a sole handler or renders an **Open with** chooser, calls `open_request(path, app_id, cookie)`, returns so firmware can unload it, then consumes the result with `open_take_result` after relaunch.

System-reader handlers are dispatched through `T5FileBrowserApi::open_document`.

### T5UiApi API 1

Used for the multi-handler chooser. Required members are `render_list`, `poll_event`, `hit_test`, `next_index`, and `previous_index`.

## Filesystem model

The app maintains a logical SD path beginning at `/`. Firmware directory enumeration uses the VFS prefix `/sd`, so logical root maps to `/sd` and `/Books` maps to `/sd/Books`.

Hidden names beginning with `.` are omitted unless `show_hidden_files()` is enabled. The literal directory `System Volume Information` is always skipped.

Directories sort before files. Names then use a case-insensitive natural comparison in which numeric runs are compared by significant length/value rather than simple bytewise lexicographic order.

## Navigation and input

Firmware supplies browser events for previous/next row, previous/next page, row activation, open, delete, root, Back, and Exit.

Opening a directory updates the logical path and reloads entries. Away from root, Back moves to the parent directory and tries to preserve selection on the child directory just exited. At root, Back clears session state, calls `navigate_home()`, restores normal Back-exit semantics, and returns.

## File opening

### ELF files

Names ending in `.elf` case-insensitively are launched with `launch_elf_request`. File Browser saves session state and returns immediately after a successful request. On relaunch it consumes `launch_elf_take_result`; non-zero ESP errors are rendered as `Native app failed: <code>`.

### Other files

Registered handlers are queried through `T5FileOpenApi`. With no handler the status becomes **No registered app for this file type**. A cancelled multi-handler chooser reports **Open cancelled**. Handler-launch failures are shown in the browser status area.

## Delete workflow

Directories are not delete candidates in the current source. For a selected file, the app saves the logical path to session state and requests firmware confirmation. After relaunch it consumes the confirmation result and, when confirmed, calls `delete_document` on the saved path.

Selection is repaired after deletion so it remains inside the current entry range.

## Confirmed implementation limits and failure behavior

- Maximum entries: **256**
- Path buffers: **512 bytes**
- Handler chooser: **8 handlers**
- Status buffer: **128 bytes**
- Missing required API/functions cause `app_main` to return.
- Failed system-reader open, app-handler open, ELF launch, and delete operations produce explicit status text.
- A directory-open failure is returned internally by `load_files`; current source does not synthesize a separate user-facing directory-open error.

## Persistence and ownership boundaries

File Browser persists only its handoff session file. Handler registration, association generation, readers, ELF loading, delete confirmation UI, themed rendering, and Home navigation are firmware-owned services.

## Source

- `Apps/file_browser.c`
- `Apps/file_browser.json`

Authoritative upstream blobs observed during migration:

- source: `f1bc16a6b435d0b8386d9ec404c7cc6fd7a584bc`
- manifest: `69d19e01c2c9e33d1fba8852c22e1fb9cf53445b`
