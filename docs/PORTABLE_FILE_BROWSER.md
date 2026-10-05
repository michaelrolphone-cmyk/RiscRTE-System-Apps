# NOVA-7 File Browser (portable 1.4.0)

The existing `Apps/file_browser.c` now has an explicit portable build profile.
The legacy Reader profile remains byte-for-byte identical to the pinned 1.3.2
ELF. The portable controller reuses its natural name/folder ordering and path
model, moved into `FileBrowserModel.h`, and the existing volume capability table.
It calls no Reader firmware UI/filesystem bridge and never executes a browsed
ELF. NOVA uses the same licensed Settings typography/palette and the exact
32-key/three-page Points Watch keyboard.

## Behavior

- Two touch-sized rows per page, natural ordering, directories first and a stable
  bytewise tiebreaker. Streaming page selection has constant RAM and no arbitrary
  folder-inventory cutoff. Previous/Next, crown/button movement and vertical
  scrolling are supported. Back navigates to the parent and finally Apps.
- Options offer case-insensitive filename filtering, Refresh, hidden-file toggle
  and root navigation. Keyboard Done applies; Back cancels the draft. Folder
  navigation clears its local filter. Filters and cursor positions are session
  state; no settings/NVS writes are made.
- File details show exact byte size and the full name across two explicit name
  pages if needed. Files have bounded ASCII-byte and hexadecimal previews. The
  ASCII view maps control/non-ASCII bytes to visible safe placeholders/spaces;
  it is a byte viewer, not a UTF-8 document editor. Every byte in each page is
  represented; line breaks do not silently skip bytes. Previous/Next follow
  explicit byte ranges. Empty files, changing files, short/failed reads,
  unavailable/malformed volumes and failed close/release all have explicit UI.
- Failed close retains its grant, blocks further storage work and sleeps, and
  offers Back retry. Normal callbacks leave no open file/directory across poll.
  Alarm and quick-control foreground rendering preserves the app's local state.
- No time values are displayed, so no independent 12/24-hour convention is added.

## Installed storage authority and remaining management work

Watch deployment requires Runtime 0.1.28's explicitly granted
`storage.installed-files@1`, instance 0. It reuses `RiscStorageVolumeV1` but exposes
only admitted app/provider ELF+manifest paths. Private provisioning input,
credentials, NVS and arbitrary root files are excluded. Installed storage is
read-only; write/create/remove callbacks are absent and the app says so. No
rename/delete/move option pretends to work against the executable store.

The original broader file-management request still needs a separately admitted
writable user volume with recoverable rename/move/trash semantics. Watch's
current paired layout has no such user filesystem. This increment does not
repartition, alter bank geometry, format storage or invent an NVS file quota.
A future user volume can use the generic volume table through the build's
explicit capability selector; recoverable management requires a separately
specified supporting contract. Existing Reader rename/move/delete behavior and
published ELF remain unchanged.

## Build and validation

```
ASAN_OPTIONS=detect_leaks=0 python scripts/test_portable_file_browser.py
python scripts/build_portable_file_browser.py --alarm-client --navigation
```

Omit the ASAN override on ordinary hosts/CI, where LeakSanitizer is supported.
The fixture runs the real controller and shared adapter/raster with fake storage,
including 513-item paging, natural order, filter keyboard, empty and long names,
preview byte boundaries, error/retry and release ownership. Frames use padded
stride guards. The target build uses pinned GCC 8.4, the real structural ELF
validator, exact import/export checks and per-source hashes. Hardware touch,
flash-media, sleep-power and physical Watch qualification are not claimed.

The real-adapter fixture runs both touch rotations in normal and sanitizer modes, with and without the Watch quick-controls/alarm/local-navigation profile. A queued adapter handoff from an open preview exits directly and keeps its destination; it is never treated as nested Back. Launcher tests cover the genuine File Browser, BLE Scanner and LoRa glyphs, with Springboard version 1.4.8.
