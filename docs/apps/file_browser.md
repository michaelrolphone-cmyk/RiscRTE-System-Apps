# File Browser

## Purpose and classification

File Browser is RiscRTE's foundational file-navigation app. It browses SD storage, exposes an attached optional `storage.volume` provider as `USB Storage`, opens directories, dispatches files to registered handlers, launches native ELF apps, deletes files through firmware confirmation, and copies files between SD and removable storage.

It is classified here as a foundational system app because file browsing, handler dispatch, native-app launch, and storage handoff are core device workflows.

## Manifest

- Version: **1.2.0**
- Minimum firmware: **1.3.4**
- ELF: `file_browser.elf`
- Icon: `solid:f07c`
- Categories: `Files`, `Utilities`
- Optional capability: `storage.volume` API `>=1`

## Host interfaces

### T5AppApi
Used for SD directory enumeration and for disabling normal Back-to-exit behavior while the browser owns hierarchical navigation.

### T5StorageApi
Used for the persisted handoff/session file at `/sd/System/State/Applications/file_browser/Session.txt` and for streamed SD reads / transactional streamed SD writes during SD↔removable-storage copies.

The current session record contains four newline-delimited fields: logical path, selected entry name, pending-delete path, and storage marker (`U` for removable storage, otherwise `S`).

### T5SystemUiApi
Uses `navigate_home()` when Back is pressed at logical root.

### T5FileBrowserApi
Provides hidden-file policy, browser rendering/event polling, page sizing, delete confirmation handoff/result, SD delete/open operations, and native ELF launch handoff/result.

The handoff cookie is `0x4642524f57534552`.

### T5FileOpenApi
Used for installed handler discovery and file-open handoff. Up to **8 handlers** are loaded into the chooser. A single handler is selected directly; multiple handlers use an **Open with** list.

### T5UiApi
Used for list rendering, event polling, hit testing, index movement, the handler chooser, and the copy-destination picker.

### T5ProviderCapabilityApi / RiscStorageVolumeV1
Used to acquire/release optional `storage.volume` API 1. The provider is validated before use and supplies refresh/readiness, stat, directory iteration, file read/write/close, remove, and optional last-error reporting.

## Filesystem behavior

SD logical root is `/`; host VFS access prepends `/sd`. A ready removable-volume provider appears as synthetic root entry `USB Storage`, internally rooted at `/USB Storage`.

Hidden entries beginning with `.` are omitted unless firmware enables hidden files. `System Volume Information` is always filtered.

Entries are sorted directories-first, then with a case-insensitive natural-name comparison.

## Limits

- Browser entries: **256**
- Copy destination directories: **96**
- Path buffers: **512 bytes**
- Status buffer: **160 bytes**
- Copy chunk: **4096 bytes**
- Open-handler chooser: **8 handlers**

## Navigation

Opening a directory updates the logical path and reloads entries. Back away from root moves to the parent and attempts to preserve selection on the directory just exited. Back at root clears session state, navigates Home, restores normal Back-exit behavior, and exits.

The destination picker exposes **Copy here**, optional parent navigation, and child directories.

## File opening

SD `.elf` files are launched through the firmware native-app handoff. Other SD files use registered handlers. System-reader handlers use the File Browser host API; application handlers use `T5FileOpenApi`.

Removable-storage files are not directly handed to file handlers or ELF loading in 1.2.0. The app explicitly tells the user to copy them to SD before opening.

## Copy behavior

Copy applies to files, not directories.

For SD→USB, the app streams the SD source and writes the provider destination in 4096-byte chunks. For USB→SD, it reads the provider source and writes through the storage API's transactional write stream, committing only on success and aborting on failure.

Existing destination files are rejected rather than overwritten. Provider-specific errors are surfaced through `last_error` when available.

## Delete behavior

Delete applies to files. The app persists the pending path, asks firmware for confirmation, exits for the handoff, and consumes the result after relaunch. SD deletes use `delete_document`; removable-storage deletes use the provider's `remove`.

## Failure and lifecycle behavior

Missing required host interfaces/functions cause `app_main` to return. Confirmed user-visible failures include missing handlers, cancelled handler choice, failed handler/open/ELF launch, disconnected or unopenable removable storage, copy failures, destination-exists rejection, failed delete, and trying to open a removable file before copying it to SD.

The removable-storage capability lease is released on exit.

## Source

- `Apps/file_browser.c`
- `Apps/file_browser.json`

Authoritative upstream blobs for 1.2.0:

- source: `26829771ecc0690d6c50f34c37ed604bd91da903`
- manifest: `cb767d15bfa0d92f77164dbf4739f3de896aae2e`
