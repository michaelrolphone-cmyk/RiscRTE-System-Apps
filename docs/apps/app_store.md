# App Store

## Purpose and classification

App Store is the foundational RiscRTE application installation, update, and SD-package management interface. It is classified as a System App because installing and updating applications is a first-use/core software-management workflow.

## Manifest metadata

- Version: **1.0.7**
- Minimum firmware: **1.3.10**
- Artifact: `app_store.elf`
- Icon: `solid:f019`
- Categories: `System`, `Software`

## Host APIs

### `T5AppApi`

The source requires the application catalog operations `app_catalog_refresh`, `app_catalog_count`, `app_catalog_get`, and `app_catalog_download`. When present it also uses `app_catalog_manifest_get`, `app_catalog_version_get`, `installed_app_version_get`, `app_catalog_download_with_progress`, and `app_catalog_download_last_error`.

It uses `dir_open`, `dir_next`, and `dir_close` to enumerate `/sd/Packages/Inbox`, and disables normal Back-to-exit behavior while its own UI loop is active.

### `T5PackageManagerApi`

The package manager is version/size checked and must expose `preview`, `install`, and `uninstall`. It is used for offline application packages staged under the SD Inbox.

### `T5UiApi`

The app requires list rendering, event polling, hit testing, and next/previous selection helpers. The source also uses `T5_UI_LIST_ICON_COMPACT` when a row already carries an installed/update/download state icon; the current UI ABI defines that flag as bit 4 and firmware owns the rendered compact size.

## User-visible workflows

The app has two primary views.

**Release catalog:** refreshes the firmware-owned application catalog, displays compatibility and installed-version state, and installs or updates selected applications.

**SD Inbox:** enumerates package directories beneath `/sd/Packages/Inbox`, previews application packages, installs eligible staged packages, and allows uninstall of an installed package after explicit confirmation when no update is allowed.

Tapping the header from the release catalog opens and rebuilds SD Inbox. From SD Inbox, it attempts a release refresh and switches only on success. Version 1.0.7 keeps the existing SD Inbox view and rows when that refresh fails, resets selection to row 0, and reports `Release refresh failed; SD packages available`. Directional events move selection. Confirm acts on the selected row. A tap on a different row selects it; a tap on the already-selected row activates it. Back or Exit restores normal Back behavior and returns.

## Catalog state and icons

The UI is bounded to **64** rows. Current source distinguishes:

- incompatible release: `Requires newer firmware`
- installed and equal to latest: `Installed`
- installed with a different catalog version: `Update available` (the app compares version strings for equality; it does not order versions)
- not installed: `Not installed`

The source uses the shared UI row-state flags to show the firmware-rendered **installed**, **update**, and **download** state icons. Any row with one of those state icons also sets `T5_UI_LIST_ICON_COMPACT`; update rows retain value highlighting. The app does not choose a pixel size itself.

## Download/install behavior

Before catalog refresh, the app renders a connection/loading state. For install/update, it refuses incompatible manifests and no-ops when the installed version already equals the catalog version.

If the host exposes `app_catalog_download_with_progress`, the app renders a progress screen and redraws when the 10%-step bucket changes (buckets 0 through 10); the displayed percentage and byte counts are derived from the callback. Older compatible firmware falls back to `app_catalog_download`.

After a failed catalog install, the app uses `app_catalog_download_last_error` when available and otherwise reports a generic installation failure.

The app itself does not implement the network transport, endpoint selection, TLS, verification, or publishing transaction; those belong to firmware catalog/package services.

## SD Inbox behavior

Only directory entries that preview as `T5_PACKAGE_APPLICATION` are shown. Truncated package-directory identities are rejected rather than silently shortened.

Rows surface valid-installation state, installed version, update availability, fresh-install readiness, and dependency/version/stage blocks. Fresh or updated staged packages call `manager->install(folder)`. Installed packages with no allowed staged install can reach the explicit uninstall confirmation path and then `manager->uninstall(T5_PACKAGE_APPLICATION, id)`.

## Storage and persistence

The only direct filesystem enumeration in this app is `/sd/Packages/Inbox`. The source defines no app-specific persistent state file, direct hardware access, or dynamic provider capability; catalog/package operations mutate installed applications through firmware services.

## Failure handling and constraints

Missing required API tables/function pointers cause startup to return. Initial catalog refresh failure leaves the release list empty but SD packages remain accessible through the header. A failed refresh when returning from SD Inbox preserves the inbox view and data, so subsequent actions still operate on packages. Package install/uninstall refusals and catalog-download failures are reported in the status line. String and row buffers are statically bounded.

## Source and release identity

Synchronized from Reader commit `be82695ea0ecb14525c0de1ddc78cd0c77e4614b`:
- `Apps/app_store.c`: `cbf5aa30e6d5d12a093c7cc12daee4eb6a308b1a`
- `Apps/app_store.json`: `0984daf21c7f744628da793018e1e50093bf8780`

The audited upstream release-index snapshot lists [Reader release `app-app_store-v1.0.7`](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-app_store-v1.0.7), `app_store.elf`, **9,640 bytes**, SHA-256 `636678aebc3f936184da7d23a69be0d5a7e70b9ea5abafb8f47f6d564cb7fd12`. These are upstream published metadata; destination development builds are not independent releases and do not establish runtime parity.
