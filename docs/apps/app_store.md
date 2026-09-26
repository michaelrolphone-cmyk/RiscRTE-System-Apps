# App Store

## Purpose

App Store is the RiscRTE application installation/update and SD-package management interface. Its source uses the runtime release catalog together with the package manager and shared UI APIs.

## Core workflows

The app has two primary views:

1. **Release catalog** — refreshes the remote app catalog using the runtime's saved-network implementation, shows install/update/current state, and downloads selected applications.
2. **SD Inbox** — enumerates package directories under `/sd/Packages/Inbox`, previews application packages, installs eligible packages, and exposes uninstall/recovery-related state through the package manager.

The UI also performs explicit uninstall confirmation before removing an application.

## RiscRTE interfaces

The app uses:

- `T5AppApi`
- `T5PackageManagerApi`
- `T5UiApi`

From `T5AppApi`, the app uses the catalog functions `app_catalog_refresh`, `app_catalog_count`, `app_catalog_get`, `app_catalog_download`, and, when available, `app_catalog_manifest_get`, `app_catalog_version_get`, and `installed_app_version_get`.

It also uses the app directory iterator (`dir_open`, `dir_next`, `dir_close`) to inspect `/sd/Packages/Inbox`.

From `T5PackageManagerApi`, it requires package `preview`, `install`, and `uninstall` operations and validates the package-manager ABI/version before use.

From `T5UiApi`, it requires list rendering, hit testing, event polling, and selection-index helpers.

## Catalog behavior

Release entries are capped at 64 rows in the app UI. When manifest metadata is available, display name and firmware compatibility come from the manifest. Version-aware firmware exposes available and installed versions so the app can distinguish:

- Not installed
- Installed/current
- Update available
- Requires newer firmware

The app does not implement arbitrary GitHub/network access itself; it calls firmware-owned catalog operations.

## SD package behavior

The SD Inbox expects package directories beneath `/sd/Packages/Inbox`. Each candidate directory is previewed by the package manager and only application packages are shown.

The UI reports invalid/recovery states, currently installed versions, whether installation is allowed, and dependency/version/staging blocks based on the package-manager preview result.

## Download/install presentation

During catalog refresh the app presents a connection/loading view. Download activity is shown through a dedicated progress view and is bucketed so UI redraws do not occur for every byte-level progress change.

## Safety and failure handling

The app bounds all release/package rows and copied strings. It rejects truncated package identities rather than silently converting one folder name into another. Uninstall requires confirmation. Compatibility checks are host/manfiest-driven; incompatible releases are displayed but not treated as normal install candidates.

## Source

- `Apps/app_store.c`
- `Apps/app_store.json`
