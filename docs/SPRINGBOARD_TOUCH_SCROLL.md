# X4 Springboard horizontal pages

Springboard 1.7.18 selects a standard four-column, five-row grid with
`--time-profile x4-native-time --paper-transitions --touch-scrolling`.
Each page holds 20 apps. The selected 18-app catalog fits on one page; a
19-entry catalog including GameBoy also fits on one page; 21 entries require a
second page. Footer dots indicate the page and remain
direct page controls. There is no numeric page-count label. Vertical gestures
inside the grid cancel an icon press without moving the grid.

The selected builder accepts at most 40 entries and compiles the actual catalog
count into its identity storage (one slot for an empty catalog). The selected
18-app build therefore retains its previous identity allocation. Other profiles
keep their existing 18-entry bound. The selected GameBoy icon uses the genuine
`solid:f11b` gamepad raster from the existing pinned Font Awesome CPFont, with
its hash recorded in `fonts/SOURCES.json`; it is excluded from Watch builds.
The existing quick-session settings load
supplies the persisted 12/24 preference without additional I/O. Hours have no
leading zero; minutes have two digits. Missing or invalid preferences select
12-hour display, consistent with Settings.

## Input and presentation

`Apps/springboard_pages.h` is a fixed-storage app-local horizontal controller.
A horizontal gesture locks after eight logical pixels and more than twice the
vertical travel, provided it begins inside the grid. It then follows the finger
in either direction while clamping at the first and last pages. There is no
wrap. The original row coordinates do not change during a drag. A release
snaps to the nearest page with a bounded 180 ms ease-out. Recent flick velocity
can finish an adjacent page after at least 44 logical pixels of travel. The
last movement determines direction; paused velocity cannot launch a late flick.
Short gestures return to their page. Cancellation returns a dragged gesture to
its origin page and requires a fresh eligible contact before any activation.
Queued UP coordinates are authoritative. Snapshot-only release uses the last
observed position. All timing arithmetic tolerates monotonic-counter wrap.

Input continues while the display is transferring or BUSY. The controller
updates one latest desired position; the existing adapter owns its in-flight
frame lease. It renders the newest desired position only when ready, with no
animation queue and no rewind to a stale completed position on a second touch.
A new touch can stop/reverse a settle, but cannot activate an icon until a
subsequent fresh press on a stable, completed page. Header and footer stay fixed.
The existing Low Latency presentation intent, global flip and completed-image
Home crossfade stay in the shared adapter. No Runtime or provider ABI changes
are required.

A fresh stationary icon press paints the selected highlight. Release launches
only the filename captured from the completed image under the initial contact,
after its highlight frame completes. Insertion/reorder resolves the same
filename again; removal cancels it. Drag, contact cancellation, a new contact,
Home and reserved top-edge Quick Controls cancel stale launch intent. Failed or
superseded presentation retains custody rather than admitting a launch.

## Verification

Run the production controller/input/raster/async-adapter fixture in normal and
ASan/UBSan builds:

```
python3 scripts/test_touch_scroll_springboard.py \
  --runtime-sdk /path/to/canonical/runtime/sdk/app \
  --display-sdk /path/to/canonical/display/sdk \
  --output-dir /path/to/evidence
```

It also runs the standalone page-controller checks. Integration scenarios cover
0/1/17/18/19/20/21/22 apps, finger-tracked intermediate frames, snap and reversed flicks,
clamped bounds, short/paused gestures, axis lock, queued UP and snapshot release,
cancelled/replaced contacts, display BUSY and reversal before completion, page
dots, launch failure, catalog changes, Home, Quick Controls, persisted 12/24
format and missing/invalid/unavailable records, and a 180-degree
flip. Every submitted frame is immutable until completion and a second frame
cannot be submitted while its token is pending. Recorded submit offsets and
actual raster crops verify horizontal translation with unchanged icon rows.
Pixel checks exclude the 32-pixel leading clipped-glyph region, where the existing shared
font renderer rounds negative pen coordinates toward zero.

Use `--catalog /path/to/catalog.json --case eighteen --case tap` to capture
and test the selected 18-app deployment. Synthetic 21/22-entry fixtures verify
second-page taps and cancellation. A 40-entry selected catalog can be tested
with `--case eighteen --case swipe-left`; the case name only selects a no-input
capture when an explicit catalog is supplied. Builder acceptance at 20/21/22/40,
rejection at 41, and unchanged non-selected bounds are checked by
`python3 -m unittest discover -s tests -p test_springboard_catalog_bound.py`.

Rendered images, offsets and results are written to the evidence directory.
Host provider timing is simulated; it is not panel FPS, visible contrast,
ghosting or hardware qualification.

The Watch/NOVA suite remains `CPATH=lib/PortableApps/include
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_springboard_nova.py`. The include
path is needed by its existing standalone tap-layer fixture. Generic vertical
list behavior is checked by the unchanged PortableTouchScroll fixture and
selected Settings/File Browser/Wi-Fi integration fixtures as applicable.

Previous evidence under `docs/evidence/springboard-scroll` and
`docs/evidence/springboard-horizontal` describes the superseded vertical
viewport implementation. It is retained as history and does not establish the
behavior of 1.7.18. New target receipts and rendered evidence accompany this
source checkpoint. No hardware, frozen deployment or public release is changed.
