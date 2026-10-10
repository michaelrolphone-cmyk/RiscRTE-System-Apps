# Selected Files touch scrolling

Files 1.5.9 is selected by `--time-profile x4-native-time --paper-transitions
--touch-scrolling`. It builds on the qualified 1.5.8 SD/cleanup fix and retains
its exact `storage.volume@1` instance 9, `file.open@1` instance 0, native time,
API2 alarm and Quick Actions contracts. The default Watch and motion-only
profiles remain 1.5.8 and reproduce their previous ELF bytes exactly.

The selected paper directory, File Options, File Actions, destination and
Open With lists use the shared bounded touch/momentum controller. List page
buttons are replaced with fixed Back/Open or Cancel/Up/Here controls. Preview
and keyboard character pages keep their existing purpose and behavior. The
small-paper Delete confirmation keeps its warning above the footer.

## Bounded data and identity

The directory controller keeps 16 naturally sorted rows, plus bounded submitted
and completed-image snapshots. It streams provider entries to refill a visible
window in either direction, closes each enumeration before normal polling, and
keeps failed closes in the existing owned-handle retry path. It never loads the
whole folder into memory. Directory metadata fingerprints detect changes during
refill and reset the viewport rather than applying an old ordinal to new rows.
A rebased coordinate window preserves unsigned absolute row identity without
limiting folder enumeration to the shared controller's finite Q8 pixel range.

A touch captures the completed image's offset and item identity at Down. If a
new frame is pending, the tap still resolves the visible file or handler. Files
are checked by path, type and size before a touch activation; handler identities
are re-read before dispatch. A changed item is left for a fresh selection.
Physical Confirm/Open first reveals an off-screen selection and waits for its
completed image. Returning from actions restores the selected directory item.

The viewport clips both rectangle/text and icon raster writes. Pending display
buffers remain immutable while later touches update only desired scroll state.
Back, Home, mode changes, Quick Actions, alarms, sleep and failed cleanup cancel
contact and velocity. A stopping touch cannot activate a file. The existing
explicit rename/copy/move/delete/handler controllers continue to own operations.

## Verification

`python scripts/test_touch_scroll_files.py --sdk <selected-target>/native-time-sdk/include`
runs 124 fresh-process cases through the real app controller, adapter, MONO1
renderer and helper sources at 480x800 and 400x600, normally and with ASan/UBSan.
Coverage includes:

- Repeated drags, coast/deceleration, reverse movement and both content bounds.
- Forward/backward directory cache refills, natural ordering, empty folders,
  changing inventories, directory read errors and retained-close retries.
- File/action/options/destination/handler overflow and exact visible selection.
- A 500 ms simulated display transfer with 23 newer touch samples; three list
  submissions suffice and the old completed row is selected correctly.
- Back/Home/horizontal Back, replaced contacts, queued UP coordinates, footer
  rejection, Quick interruption and retained input-provider failure.
- Hidden-selection Confirm, action-to-directory return, stale file/handler
  rejection and explicit Delete confirmation without performing deletion.
- Filter/rename keyboard editing and cancellation, including a character-page
  change while an earlier keyboard frame is pending.
- Coordinate rebasing and unsigned row boundaries up to UINT32_MAX inventory
  count, with no large-folder throughput claim.

Pixel assertions protect fixed content and pending framebuffer bytes. Actual
production captures were inspected at both geometries, including partial rows,
window boundaries, action/handler lists, destinations and Delete confirmation.

Existing Watch browser, paper browser and full file-operation/cleanup suites
pass. All 75 repository Python tests pass. The real Runtime acquisition and
file.open projection runs 15 cases normally and under ASan/UBSan, including
nested paths and admission failures. That Runtime fixture projects storage and
dispatch only; the selected UI paths are covered by the separate native fixture.
The target passes import/export checks and the real loader's structural ELF
validator. SDKs, capabilities, grants and all non-scrolling defines are unchanged.
Selected Wi-Fi 1.1.11 also reproduces its previously qualified bytes exactly.

This is host/target build evidence, not physical display cadence or touch-latency
qualification. A directory refill still enumerates the provider's folder to
preserve natural ordering; real SD enumeration latency remains a hardware check.
No app was connected to hardware or a network, no product metadata was changed,
and no source or release was published.
