# Springboard

## Purpose and scope

Springboard is the foundational RiscRTE installed-application launcher. The synchronized manifest identifies it as **Apps** (`springboard.elf`), version **1.3.1**, minimum firmware **1.3.39**, icon `solid:f00a`, categories `System` and `Launcher`.

The app owns grid layout, selection, pagination, input, Home pins, and the optional cached-page slide animation. Firmware supplies installed-app discovery, drawing primitives, touch capture, video scan services, and launch handoff. Springboard does not map or execute another ELF directly.

## Startup and inventory

`app_main` obtains `T5AppApi` and `T5StorageApi`. It validates the App API through the append-only `fill_rounded_rect_tone` member, refreshes installed applications, caps the working set at **128** entries, loads Home pins when storage is available, computes grid geometry, clears edit/selection state, and attempts video initialization before rendering.

A missing required App API causes immediate return. Storage is optional for ordinary launch; Home editing requires the storage operations described below. A successful video setup changes the input-poll wait from **20 ms** to **5 ms**.

## Navigation and launch

Displays at least 700 pixels wide use four columns; narrower displays use three. Rows are `(height - 180) / 150`, with a minimum of one. Page size is columns × rows, current page is `selected / page_size`, and page count rounds up the installed-app count (one page when empty).

Physical directional presses change selection and expose its underline. Left/right wrap across the inventory; up/down clamp at its ends. A short Confirm release launches the selected compatible app in normal mode. Holding Confirm for at least **700 ms** toggles Home edit mode and suppresses the release activation.

Touch behavior:
- tapping an app cell launches immediately in normal mode or toggles its Home pin in edit mode;
- touch selection clears the physical-navigation underline;
- the top-right rounded **EDIT/DONE** control toggles edit mode;
- tapping the page-dot region advances to the next page and wraps;
- a completed horizontal swipe changes page when there is more than one page, with video drag/settle behavior when available.

`take_touch_swipe` is optional and checked by structure size and function pointer. In the ordinary raster path, paging requires horizontal travel of at least **50 pixels**, strictly greater than twice the vertical travel. Leftward swipes advance; rightward swipes go back, wrapping at either end. A completed swipe is handled separately from taps so dragging across an icon cannot activate it.

## Home edit mode and persistence

Pinned apps have a rounded black/white cell highlight in edit mode. A short Confirm release or app-cell tap toggles the selected pin. Entering or leaving edit mode clears navigation-selection visibility. Incompatible apps remain visible but cannot be launched or pinned; their required firmware is reported.

Pins are stored in `/sd/Apps/.home_apps` as newline-delimited ELF filenames, matched exactly against installed manifest `file_name`. Loading accepts CR/LF separators and ignores unmatched lines. The payload is bounded to **16,384 bytes** (`128 * 128`); loading allocates payload size plus one byte, and saving allocates capacity plus one. Writes use `write_file_atomic`. A save failure restores the previous in-memory pin state.

## Host interfaces and ownership

### `T5AppApi`

The base path requires a structure extending through `fill_rounded_rect_tone` and these operations:
- screen width/height;
- clear, text, rectangle, icon, label, and rounded-tone drawing;
- `present`, `poll`, and `millis`;
- installed-app refresh/count/get;
- `request_app_launch`.

The video path additionally size-checks through `touch_contact` and requires `copy_ui_frame`, `touch_contact`, `psram_alloc`, and `psram_free`. `copy_ui_frame(NULL, 0, &frame)` queries geometry; subsequent calls copy software-rasterized pixels into app-owned caches. The non-consuming `touch_contact` supplies oriented live contact coordinates. These appended frame/contact operations are the firmware **1.3.39** interface used by this version. `log_message` is used when present to report a fatal video exit.

### `T5StorageApi`

Storage is accepted only when its structure reaches `write_file_atomic` and exposes `exists`, `read_file`, and `write_file_atomic`.

### Hardware takeover and `T5VideoApi`

The exported `app_hardware_takeover()` requests `T5_HARDWARE_TAKEOVER_DISPLAY | T5_HARDWARE_TAKEOVER_UI_VIDEO` only when the host structure reaches `touch_contact`, both frame-copy/contact pointers exist, and the frame-geometry query succeeds. Otherwise it requests no takeover.

The video service must expose a structure through `start_format` and provide `start_format`, `backbuffer`, `can_submit`, `submit`, and `stop`. It owns its backbuffer, DMA, and scan tasks. The app requests `T5_VIDEO_PIXEL_GRAY_2BPP_MSB`, validates returned geometry/format and `T5_VIDEO_FLAG_ONE_IS_BLACK`, composes into the service backbuffer, and submits the full panel with `submit(0, 0)`.

UI-video takeover permits software rasterization and frame copies, not physical `present()` or firmware dialogs. The enabled video path never calls `present`; the ordinary non-video drawing path uses `present(false)`. Cleanup stops the video service before freeing caches and returning, after which the loader owns restoration of normal display ownership.

## Rendering and video resources

Each icon uses an **82-pixel** glossy tile built from host-provided white, light-gray, dark-gray, and black rounded rectangles: outer lip, metallic rim, highlight, bevel, black face, and upper/lower/side reflections. The icon itself is white on the black face. Labels, selection underline, edit highlight, warning text, and page dots are rasterized with the same page.

Video accepts only physical **960 × 540**, **240-byte stride**, orientation **0–3** frames. It allocates three PSRAM-only cached pages (current, previous, next), each **129,600 bytes**, totaling **388,800 bytes**, excluding the service-owned video buffers and host renderer. Cached neighboring pages refresh when page/edit mode changes; the current page refreshes when `draw` runs. No icon/font rerasterization is performed for each drag frame.

`springboard_slide.h` composes the cached 2bpp pages for all four orientations. Offsets are clamped to logical screen width and quantized to four pixels. It preserves the top/bottom 64-pixel chrome regions while translating the grid and taking incoming pixels from the adjacent page. The app submits dirty frames no more often than every **40 ms**; this is a submission cap, not a claim about physical scan rate.

## Drag and settle rules

Live horizontal drag begins at **12 pixels** of travel and requires horizontal travel strictly greater than twice vertical travel. Vertical movement over 12 pixels and at least as large as horizontal movement locks that contact out of horizontal dragging.

On release, page commitment requires a completed horizontal swipe (at least 50 pixels and the same 2:1 horizontal test), plus either no tracked live contact, travel of at least one-fifth of logical width, or travel of at least 80 pixels within **350 ms**. A drag without a qualifying completed swipe snaps back, including a contact gap/outage. Committed or cancelled drags settle with cubic easing over **180 ms**.

Dragging and settling consume input so icons cannot launch or change pins. A contact begun during settling must lift before it can become a tap. Page changes wrap in both directions.

## Failure handling and limits

- Missing base App API operations returns immediately; missing storage disables Home editing.
- Missing frame/contact/PSRAM support or an unsuccessful initial frame query makes video initialization return without enabling video, leaving the raster drawing path.
- Once those prerequisites pass, missing video operations, unsupported geometry, cache-allocation failure, or mismatched/failed surface start is fatal rather than a runtime fallback.
- Frame-copy failure or geometry change, missing/undersized video backbuffer, or continuous blocked/rejected submissions for **2,500 ms** marks video fatal and exits through cleanup.
- Allocation/start failures free acquired page caches and stop any acquired video service; normal launch/exit also runs video cleanup.
- Launch-request failure is reported; incompatible apps cannot launch; pin-save failure rolls back the pin toggle.
- An empty inventory displays instructions to put matching ELF/JSON pairs in `/Apps` on SD.
- Failed/ended polling or `exit_requested` exits.

## Network and hardware boundaries

The source performs no network I/O, direct panel-bus writes, or direct external-peripheral access. It conditionally requests display/UI-video ownership and controls frames through the host video API; hardware scan/DMA execution remains service-owned. No dynamic provider capability is declared in the manifest.

## Source and release identity

Synchronized from Reader commit `be82695ea0ecb14525c0de1ddc78cd0c77e4614b`:
- `Apps/springboard.c`: `929136ccef3e24df0f177b790cf0aee7ed5a5bec`
- `Apps/springboard.json`: `11b3f479bd7c550d261228dab534c5b05aabf4c1`
- `Apps/springboard_video.inc`: `78930d039534ae7592428ff8125c64eb67b0d946`
- `Apps/springboard_slide.h`: `4162adc873b20f20db1141d46effd6d52e4f75ac`

The audited upstream release-index snapshot lists [Reader release `app-springboard-v1.3.0`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-springboard-v1.3.0), `springboard.elf`, **12,020 bytes**, SHA-256 `6863a4a8f08b5cc5c518a0dec212e9817fce13c5dcd318ca26bc0eeb9a69a150`. These are upstream published metadata, not an independent destination release. Destination development builds do not establish an independent release or runtime parity.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.3.1**; the application C source is synchronized without source edits. Source blob `929136ccef3e24df0f177b790cf0aee7ed5a5bec`; manifest blob `db221d3198ad2a81457bf079cda2b6446c1fe649`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-springboard-v1.3.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-springboard-v1.3.1/application-springboard-1.3.1-xtensa-esp32s3.rte.zip) has a **13068**-byte package with SHA-256 `d2c16c687ec3f8f3040323feb71b2287ad38fb1c552e705c815228c0de926780`. Its embedded `springboard.elf` is **12020** bytes with SHA-256 `6863a4a8f08b5cc5c518a0dec212e9817fce13c5dcd318ca26bc0eeb9a69a150`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.

## External compact development version 1.3.4

The shared app now includes a capability-gated compact [NOVA presentation](../SPRINGBOARD_NOVA.md).
Its real app catalog, Font Awesome provenance, continuous spring motion,
held-contact drag-only handoff, cancellation rules, explicit time policy and
verification limits are documented there. This external development delta does
not change the paper layout or claim parity with an earlier published release.

## Selected X4 horizontal pages

The optional native X4 1.7.18 profile presents 20 apps per page in a standard
four-column, five-row grid. Dots indicate the page; the clock uses the persisted
12/24 preference with an unpadded hour and padded minutes. Horizontal drags move the icons with the finger, then snap
to a bounded page on release. Ordinary application lists retain vertical
scrolling. See [X4 Springboard pages](../SPRINGBOARD_TOUCH_SCROLL.md) for build
selection, completed-frame launch ownership and rendered verification.
