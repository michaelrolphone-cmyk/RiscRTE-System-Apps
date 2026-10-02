# Manage Fonts

## Purpose and manifest

Manage Fonts is the foundational appearance/settings app for installing, updating, and removing SD font families through firmware `T5FontApi` version 2. The synchronized manifest specifies version **1.0.2**, minimum firmware **1.1.24**, artifact `font_manager.elf`, icon `solid:f031`, and categories `Settings`, `Appearance`.

## Host interfaces

`app_main` obtains `T5AppApi`, `T5FontApi`, and `T5UiApi` and returns immediately if any table or required operation is missing:
- App: `poll`
- Fonts: `refresh_catalog`, `family_count`, `family_info`, `install_family`, `delete_family`
- UI: `render_list`, `hit_test`, `next_index`, `previous_index`

The getter requests `T5_FONT_API_VERSION` **2**, although the C table type remains named `t5_font_api_v1`. The app does not itself compare the returned font table’s `api_version` or `struct_size`. Selecting the active reading font is outside this app; it does not call the font-choice operations.

## Catalog and operations

Startup refreshes the provider-owned catalog and then renders at most **64** families. Rows show the provider name/description plus **Update available**, **Installed**, or total size rounded up to KiB (displayed as `KB`). Update rows highlight the value. Values have 48-byte buffers, status has a 128-byte buffer, and family/row storage is statically bounded.

A family that is not installed or has an update calls `install_family`. The provider progress callback redraws the list with family name, one-based file index/count, and downloaded/total bytes. An installed family with no update requires two activations on the same selected row: the first displays **Press Remove again to delete …** and changes the confirmation label to **Remove**; the second calls `delete_family`.

## Navigation and confirmation safety

Up/Left and Down/Right navigate and clear pending removal. A valid row tap selects and activates that row immediately; tapping a different row clears the previous pending removal. Two taps on the same installed, current family can therefore confirm deletion. There is no timed double-tap requirement for removal.

Version **1.0.1** handles physical Confirm only on a rising edge. Holding Confirm does not repeatedly activate and cannot satisfy both removal steps by itself; another physical activation needs a release followed by a press. Release state is tracked before navigation/touch handling, including polls whose navigation or touch event consumes the rest of that iteration. A Confirm edge consumed by those higher-priority events is not replayed later while the button remains held.

Input is polled with a **50 ms** wait. Back, `exit_requested`, or a failed/ended poll exits. Selection begins at row 0 and is reset there when outside the refreshed family range.

## Errors and ownership boundaries

Provider results are surfaced as **Done**, **Network error**, **Invalid font catalog**, **Storage error**, **Checksum error**, **Invalid font file**, **Invalid font selection**, or **Font service unavailable**. A failed catalog refresh is shown in the status line and the app still renders whatever family inventory the provider exposes.

The app defines no storage path, persistent state file, network endpoint, transport, or hardware access. Catalog retrieval, font downloads, checksums, file storage, removal, and persistence are provider-owned. No dynamic provider capability is declared in the manifest. Progress rendering is synchronous with the provider callback; this source does not implement a separate cancellation loop for installation.

## Source and release identity

Synchronized from Reader commit `be82695ea0ecb14525c0de1ddc78cd0c77e4614b`:
- `Apps/font_manager.c`: `fe4852dd9adfbb7ed9903a92fa34795c224a4622`
- `Apps/font_manager.json`: `1cc6a3544a3c2fa2af9fa1bfc51612f47c795cd1`
- `lib/NativeApps/include/T5FontApi.h`: `f019bf858c9ec85d316ac35c79cf12ab8139a7c2`

The audited upstream release-index snapshot lists [Reader release `app-font_manager-v1.0.1`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-font_manager-v1.0.1), `font_manager.elf`, **4,464 bytes**, SHA-256 `cd211b29c01436a7ed7e27a5959622d29a5b2a102e683f0790eb30bcdaef2706`. These are upstream published metadata; destination development builds are not independent releases and do not establish runtime parity.


## Current manifest, source and release provenance (2026-10-02)

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` and System-Apps both declare version **1.0.2**; the application C source is synchronized without source edits. Source blob `fe4852dd9adfbb7ed9903a92fa34795c224a4622`; manifest blob `6e8535179951a2d4a96d39a52340f0bb7dedf670`. The manifest-only change from the recorded external baseline was the version field.

Reader published release [`app-font_manager-v1.0.2`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-font_manager-v1.0.2/application-font_manager-1.0.2-xtensa-esp32s3.rte.zip) has a **5531**-byte package with SHA-256 `496d51eda44d354ab8a509a4189d4e849628e87bb116b57b8a05409fcaba4c22`. Its embedded `font_manager.elf` is **4464** bytes with SHA-256 `cd211b29c01436a7ed7e27a5959622d29a5b2a102e683f0790eb30bcdaef2706`. The archive digest and size match GitHub release metadata and the downloaded Reader release workflow artifact `11209466823` (run `36965130240`). The independent external Xtensa build reproduces this ELF byte-for-byte. The release was built from Reader `f7f006f78bf1f83c28f3ce05728b8973e895956b`; the current audited source is Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.

This is upstream byte parity evidence, not an independent external publication, install/U1 runtime qualification, or cutover approval.
