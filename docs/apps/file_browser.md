# File Browser

## Purpose and classification

File Browser is the foundational RiscRTE file-navigation application. It browses SD storage, exposes an optional removable `storage.volume` provider as `USB Storage`, opens directories, dispatches files to registered handlers, launches native ELF applications from SD, deletes items through a firmware confirmation handoff, copies files between SD and removable storage, and on SD can rename or move items when the storage API exposes the required rename operations.

It is a System App because file navigation, file-type dispatch, and native-app launch are core device workflows.

## Manifest metadata

- Version: **1.3.1**
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

The current record is four newline-delimited fields: logical path, selected entry name, pending-delete path, and pending-delete storage marker (`U` for a removable-storage delete, otherwise `S`). The current browsing location itself is encoded by the logical path.

### `T5SystemUiApi`

The app uses `navigate_home()` at logical root. The source also requires `keyboard_request` and `keyboard_take_result` for the asynchronous Rename workflow. The fixed rename handoff cookie is `0x464252454e414d45`.

### `T5FileBrowserApi`

The app consumes firmware-owned browser rendering/event handling, hidden-file policy, delete-confirmation handoff/result, SD deletion, system-reader opening, and native ELF launch handoff/result. The main browser handoff cookie is `0x4642524f57534552`.

### `T5FileOpenApi`

Registered file handlers are queried for ordinary SD files. Up to **8** handlers are shown. A single handler is selected directly; multiple handlers use an **Open with** chooser.

### `T5UiApi`

The shared UI API renders chooser/action lists, polls events, performs hit testing, and moves selection indices.

### `T5ProviderCapabilityApi` / `RiscStorageVolumeV1`

The app optionally acquires `storage.volume` API 1. A valid provider supplies readiness/refresh, stat, directory iteration, file read/write/close, remove, and optional last-error reporting. An acquired invalid interface is explicitly released. The source does not explicitly release a successfully validated lease on its exit paths; any successful-lease reclamation is a host-lifetime responsibility not demonstrated here.

## Filesystem model

SD logical root is `/`; host VFS access prepends `/sd`. A ready removable-volume provider appears at logical root, when there is spare entry capacity, as synthetic entry `USB Storage`, internally rooted at `/USB Storage`.

Names beginning with `.` are hidden unless firmware enables hidden files. `System Volume Information` is always filtered. The synthetic USB root sorts first; other entries sort directories-first, then case-insensitive natural-name order.

## Navigation and input

Directional events move selection. A tap on a different row selects it. A second tap on that same row within **450 ms** activates it. Confirm first activates selection if none is active; with an active selection it opens the selected row. Page-previous/page-next events use the firmware-reported page size.

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

SD→USB copy streams the SD source and writes provider output in **4096-byte** chunks. USB→SD copy reads from the provider and writes through a transactional SD stream, committing after a complete successful transfer and aborting on transfer/read-close failure. A commit failure is reported without a further explicit abort call. Both directions require the full SD stream read/write API through `write_stream_abort`. USB destination collisions are refused via provider `stat`; SD collisions are refused when `storage->exists` is available, so this source does not independently guarantee that check on a host lacking `exists`.

Version 1.3.1 closes an opened USB input handle before rejecting a reported size greater than `SIZE_MAX` and displays `USB file is too large to copy`. The check happens before creating the SD output. This bounds USB→SD transfers by the target’s `size_t` range rather than truncating the provider’s 64-bit length. Copy loops poll with a 1 ms wait but do not act on cancellation/exit flags returned by those polls.

## Limits and state

Confirmed compile-time limits include:

- directory entries: **256**
- destination-picker child directories: **96**, plus destination/parent controls
- file-handler chooser: **8**
- copy chunk: **4096 bytes**
- path buffers: **512 bytes**

The implementation also uses bounded static picker/status/name storage and does not allocate an unbounded directory model.

## Failure behavior

The source reports explicit status for disconnected/unopenable removable storage, copy failures, destination collisions, unsupported USB folder deletion, invalid rename names, rename/move failures, missing file handlers, cancelled handler choice, failed handler/open/ELF launch, and delete failures. Missing required host APIs cause `app_main` to return.

## Network, persistence, and ownership boundaries

File Browser performs no direct network operations. Its app-specific persistent state is the handoff/session record; requested copies, renames, moves, and deletes also mutate user files through host/provider operations. File associations, handler registration, readers, ELF loading, delete confirmation, keyboard UI, themed rendering, and Home navigation are firmware-owned.

## Source and release identity

Synchronized from Reader commit `be82695ea0ecb14525c0de1ddc78cd0c77e4614b`:
- `Apps/file_browser.c`: `c7a4882d7abf12e8fb51627947cb645f296abf66`
- `Apps/file_browser.json`: `9368525ea10333b070af48f8531cc23addf4cfbb`

The audited upstream release-index snapshot lists [Reader release `app-file_browser-v1.3.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-file_browser-v1.3.1), `file_browser.elf`, **21,128 bytes**, SHA-256 `5eb80bc076739938f8b99d1f9292c6a7f471b2783c4ebebcfdf2fa1481abff2d`. These are upstream published metadata; destination development builds are not independent releases and do not establish runtime parity.
