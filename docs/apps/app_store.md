# App Store

## Purpose and classification

App Store is the foundational RiscRTE application installation, update, and SD-package management interface. It is classified as a System App because installing and updating applications is a first-use/core software-management workflow.

## Manifest metadata

- Version: **1.0.5**
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

The app requires list rendering, event polling, hit testing, and next/previous selection helpers.

## User-visible workflows

The app has two primary views.

**Release catalog:** refreshes the firmware-owned application catalog, displays compatibility and installed-version state, and installs or updates selected applications.

**SD Inbox:** enumerates package directories beneath `/sd/Packages/Inbox`, previews application packages, installs eligible staged packages, and allows uninstall of an installed package after explicit confirmation when no update is allowed.

Tapping the header toggles between the two views. Directional events move selection. Confirm acts on the selected row. A tap on a different row selects it; a tap on the already-selected row activates it. Back or Exit restores normal Back behavior and returns.

## Catalog state and icons

The UI is bounded to **64** rows. Current source distinguishes:

- incompatible release: `Requires newer firmware`
- installed and equal to latest: `Installed`
- installed with newer catalog version: `Update available`
- not installed: `Not installed`

Version 1.0.5 uses the shared UI row-state flags to show the firmware-rendered **installed**, **update**, and **download** state icons. Update rows also retain value highlighting.

## Download/install behavior

Before catalog refresh, the app renders a connection/loading state. For install/update, it refuses incompatible manifests and no-ops when the installed version already equals the catalog version.

If the host exposes `app_catalog_download_with_progress`, the app renders a progress screen and redraws on ten coarse progress buckets; the displayed percentage and byte counts are derived from the callback. Older compatible firmware falls back to `app_catalog_download`.

After a failed catalog install, the app uses `app_catalog_download_last_error` when available and otherwise reports a generic installation failure.

The app itself does not implement the network transport, endpoint selection, TLS, verification, or publishing transaction; those belong to firmware catalog/package services.

## SD Inbox behavior

Only directory entries that preview as `T5_PACKAGE_APPLICATION` are shown. Truncated package-directory identities are rejected rather than silently shortened.

Rows surface valid-installation state, installed version, update availability, fresh-install readiness, and dependency/version/stage blocks. Fresh or updated staged packages call `manager->install(folder)`. Installed packages with no allowed staged install can reach the explicit uninstall confirmation path and then `manager->uninstall(T5_PACKAGE_APPLICATION, id)`.

## Storage and persistence

The only direct filesystem enumeration in this app is `/sd/Packages/Inbox`. The source defines no app-specific persistent state file.

## Failure handling and constraints

Missing required API tables/function pointers cause startup to return. Catalog refresh failure leaves the release list empty but SD packages remain accessible through the header toggle. Package install/uninstall refusals and catalog-download failures are reported in the status line. String and row buffers are statically bounded.

## Source identity

Current upstream source of truth:

- `Apps/app_store.c`: `c1c6e3ef056214b6fc459372c2e3f8702b444432`
- `Apps/app_store.json`: `58152184d974282b3216d5c427da6a81c7503153`
