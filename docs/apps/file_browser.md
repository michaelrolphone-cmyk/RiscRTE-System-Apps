# File Browser

## Purpose and classification

File Browser is the foundational RiscRTE file-navigation application. It browses SD storage, exposes an optional removable `storage.volume` provider as `USB Storage`, opens directories, dispatches files to registered handlers, launches native ELF applications from SD, deletes items through a firmware confirmation handoff, copies files between SD and removable storage, and on SD can rename or move items when the storage API exposes the required rename operations.

It is a System App because file navigation, file-type dispatch, and native-app launch are core device workflows.

## Manifest metadata

- Version: **1.3.0**
- Minimum firmware: **1.3.8**
- Artifact: `file_browser.elf`
- Icon: `solid:f07c`
- Categories: `Files`, `Utilities`
- Optional capability: `storage.volume` API `>=1`

## Host APIs and interfaces

### `T5AppApi`

The app uses directory enumeration for SD browsing, `set_back_exits_app(false)` while it owns hierarchical navigation, and `millis()` for the 450 ms same-row double-tap activation window.

### `T5StorageApi`

The storage API is used for the File Browser handoff/session file, SD read streams, transactional SD write streams, existence tests, and—when the API struct exposes both `exists` and `rename_file`—SD rename/move operations.

The session file is:

`/sd/System/State/Applications/file_browser/Session.txt`

The current record is four newline-delimited fields: logical path, selected entry name, pending-delete path, and storage marker (`U` for removable storage, otherwise `S`).

### `T5SystemUiApi`

The app uses `navigate_home()` at logical root. Version 1.3.0 also uses `keyboard_request` and `keyboard_take_result` for the asynchronous Rename workflow. The fixed rename handoff cookie is `0x464252454e414d45`.

### `T5FileBrowserApi`

The app consumes firmware-owned browser rendering/event handling, hidden-file policy, delete-confirmation handoff/result, SD deletion, system-reader opening, and native ELF launch handoff/result. The main browser handoff cookie is `0x4642524f57534552`.

### `T5FileOpenApi`

Registered file handlers are queried for ordinary SD files. Up to **8** handlers are shown. A single handler is selected directly; multiple handlers use an **Open with** chooser.

### `T5UiApi`

The shared UI API renders chooser/action lists, polls events, performs hit testing, and moves selection indices.

### `T5ProviderCapabilityApi` / `RiscStorageVolumeV1`

The app optionally acquires `storage.volume` API 1. A valid provider supplies readiness/refresh, stat, directory iteration, file read/write/close, remove, and optional last-error reporting. The capability lease is released on exit.

## Filesystem model

SD logical root is `/`; host VFS access prepends `/sd`. A ready removable-volume provider appears at logical root as synthetic entry `USB Storage`, internally rooted at `/USB Storage`.

Names beginning with `.` are hidden unless firmware enables hidden files. `System Volume Information` is always filtered. Entries sort directories-first, then case-insensitive natural-name order.

## Navigation and input

Directional events move selection. A tap on a different row selects it. A second tap on that same row within **450 ms** activates it. Confirm activates the selected row.

Opening a directory reloads that directory. Back away from root moves to the parent and attempts to preserve selection on the directory just exited. Back at root clears session state, navigates Home, restores normal Back-exit behavior, and returns.

## File opening

SD `.elf` files are launched through the firmware native-app handoff. Other SD files use registered handlers. System-reader handlers are passed to the File Browser host API; application handlers use `T5FileOpenApi`.

Files on removable storage are not directly opened or launched; the UI instructs the user to copy them to SD first.

## Item actions

The selected-item action menu is context-sensitive.

On SD, when `T5StorageApi` exposes `exists` and `rename_file`, **Rename** and **Move** are available for files and directories. Rename launches the firmware keyboard, validates the returned name, refuses collisions, then calls `rename_file`. Move uses the SD destination picker, rejects moving an item into itself or an existing destination, and uses `rename_file` for the final operation.

**Copy** is available for files between SD and removable storage. Directories are not copied.

**Delete** uses firmware confirmation. SD files and directories may be passed to `delete_document`; this app does not define the firmware's recursive-delete implementation. On removable storage, file deletion calls the provider `remove`; removable-storage directory deletion is explicitly refused.

Rename and Move are not implemented for removable storage in this source.

## Copy behavior

The destination picker exposes **Copy here** or **Move here**, parent navigation, and child directories.

SD→USB copy streams the SD source and writes provider output in **4096-byte** chunks. USB→SD copy reads from the provider and writes through a transactional SD stream, committing only on success and aborting on failure. Existing destination files are refused rather than overwritten.

## Limits and state

Confirmed compile-time limits include:

- directory entries: **256**
- file-handler chooser: **8**
- copy chunk: **4096 bytes**
- path buffers: **512 bytes**

The implementation also uses bounded static picker/status/name storage and does not allocate an unbounded directory model.

## Failure behavior

The source reports explicit status for disconnected/unopenable removable storage, copy failures, destination collisions, unsupported USB folder deletion, invalid rename names, rename/move failures, missing file handlers, cancelled handler choice, failed handler/open/ELF launch, and delete failures. Missing required host APIs cause `app_main` to return.

## Network, persistence, and ownership boundaries

File Browser performs no direct network operations. It persists only its handoff/session record. File associations, handler registration, readers, ELF loading, delete confirmation, keyboard UI, themed rendering, and Home navigation are firmware-owned.

## Source identity

- `Apps/file_browser.c`: upstream blob `97676067f1410ed55e3f351c43c2530bc7842849`
- `Apps/file_browser.json`: upstream blob `8e14e164ea7c0867d1ad8128279e56b24d0467b8`
