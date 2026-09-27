# Driver Manager

## Purpose and classification

Driver Manager manages RiscRTE driver packages from the online driver catalog, the SD package Inbox, and retained recovery stages. It is a System App rather than an MCU-development tool: its primary job is managing runtime driver packages and capabilities, not communicating with or programming an external MCU.

## Manifest metadata

- Version: **1.0.5**
- Minimum firmware: **1.3.10**
- Artifact: `driver_manager.elf`
- Icon: `solid:f085`
- Categories: `System`, `Hardware`, `Developer`

## Host APIs and helpers

### `T5AppApi`

Used for SD Inbox directory enumeration, Back-exit ownership, and—when available—`millis()` to timestamp/redraw install-progress UI.

### `T5DriverManagerApi`

Required catalog operations are `catalog_refresh`, `catalog_count`, `catalog_get`, `installed_version_get`, and `install`.

When the struct exposes them, `install_with_progress` provides synchronous progress callbacks. Optional recovery support is detected by struct size and requires `recovery_refresh`, `recovery_count`, `recovery_get`, `recovery_retry`, and `recovery_discard`.

### `T5PackageManagerApi`

Used for driver packages staged under `/sd/Packages/Inbox`; the source requires `preview`, `install`, and `uninstall`.

### `T5UiApi`

Used for list rendering, event polling, touch hit testing, and selection movement.

### `T5PackageVersion.h`

`t5_package_version_compare` compares catalog and installed driver versions. The app classifies each online row as install, update, current, installed-newer, or invalid.

## Online catalog workflow

The app initially opens recovery handling when the recovery API is available, then selects the online catalog. `catalog_refresh` is called once before rows are populated. A failed online refresh falls back to the SD Inbox and reports that online access is unavailable.

Up to **64** catalog rows are retained. Each valid entry displays driver ID, capability, available version, and installed state. Current source uses the shared UI installed/update/download icons; update rows are also value-highlighted.

Activating an online row first performs the installed-version preflight. Current, installed-newer, and invalid-version rows do not install. Install/update calls `install_with_progress` when available, otherwise `install`.

The progress callback reports dependency resolution, installed-version checks, metadata fetch/check, recovery inspection, download, verification, publishing, installed/already-present, and failure stages. Download byte/percentage state is shown when totals are known. Redraw is deliberately throttled except for important phase transitions. The source explicitly clears callback context after the synchronous install returns.

Installed drivers are not activated by this app after publishing; success text states `not activated`.

## SD Inbox workflow

The app enumerates `/sd/Packages/Inbox` and only keeps directories whose preview reports `T5_PACKAGE_DRIVER`. Truncated folder identities are rejected.

Rows expose invalid/recovery state, installed version, update availability, or dependency/version/stage blocks. An allowed staged package calls `manager->install(folder)`. An installed package with no allowed install can be explicitly confirmed for `manager->uninstall(T5_PACKAGE_DRIVER, id)`; refusal text tells the user to stop the mapped driver.

## Recovery workflow

When recovery operations are available, Driver Manager lists retained stages before entering the normal catalog. Recovery states are rendered as interrupted download, verified stage ready for retry, invalid, stale, mapped, or unresolved/manual repair.

The recovery list is bounded by `RECOVERY_LIMIT = 65` entries plus a **Continue to catalog** row. A stage may expose **Retry verified stage** and/or **Discard stage** according to provider flags. Discard requires a separate confirmation. The source states that retained files are never deleted automatically.

## Navigation

The header opens a four-choice source/recovery menu: Release catalog, SD inbox, Recovery, or Cancel. Directional input moves selection. Confirm activates. Tapping a different row selects it; tapping the selected row activates it. Back/Exit restores normal Back behavior and returns.

## Network, storage, and persistence

Driver Manager contains no direct endpoint/TLS implementation. Online activity is delegated to `T5DriverManagerApi`. Direct filesystem access is limited to SD Inbox enumeration. Recovery storage layout, package verification, driver publication, capability mapping, and activation are firmware-owned and are not specified by this app.

## Failure behavior and limits

Missing required API operations causes startup to return. Online failure falls back to SD management. Install refusals point the user toward recovery. Invalid installed versions are not overwritten blindly. Static row count is **64** for catalog/Inbox and **65** retained recovery entries. Status storage is bounded to 160 bytes.

## Source identity

Current upstream source of truth:

- `Apps/driver_manager.c`: `3dd66d6ef9a5152931ad29e7ebe8b14d8ad9aaee`
- `Apps/driver_manager.json`: `66bd8af3514c0a11cf711d165739e388e558d786`
