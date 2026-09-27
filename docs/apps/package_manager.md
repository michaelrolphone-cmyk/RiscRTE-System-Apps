# Package Manager

## Purpose and classification

Package Manager is the foundational RiscRTE interface for inspecting installed managed packages and packages staged under `/sd/Packages/Inbox`, then performing verified install, replacement/downgrade, or uninstall actions. It is classified as a System App because package lifecycle management is a core software-management workflow.

## Manifest metadata

- Version: **1.1.0**
- Minimum firmware: **1.2.84**
- Artifact: `package_manager.elf`
- Icon: `solid:f187`
- Categories: `System`, `Software`

## Host APIs

### `T5AppApi`

Used for `/sd/Packages/Inbox` directory enumeration and for owning/restoring Back-to-exit behavior.

### `T5PackageManagerApi`

The source validates the API version and struct size through the `replace` member and requires `preview`, `install`, `uninstall`, `installed_refresh`, `installed_count`, `installed_get`, and `replace`.

### `T5UiApi`

Used for list rendering, event polling, touch hit testing, and previous/next index movement.

## Inventory construction

The app stores at most **64** rows in static memory.

It first refreshes installed packages and retrieves each installed record. For each installed package it attempts `preview(installed.id)`; a staged package is attached only when package kind and ID match the installed record.

It then enumerates directories under `/sd/Packages/Inbox`. A preview that succeeds and is not already represented as the staged counterpart of an installed package becomes an Inbox-only row.

Invalid installed generations are displayed as requiring recovery and cannot be mutated by this app.

## Version comparison

Replacement/downgrade decisions use a local parser that accepts exactly three dot-separated numeric components with 32-bit overflow checks. Invalid version text compares as equal for action-ordering purposes, so the app does not infer a newer/older relationship from malformed versions.

## Actions

Actions are confirmation-gated.

**Fresh install:** an Inbox-only row is actionable only when `preview.install_allowed` is true. Confirmation calls `manager->install(staged.id)`.

**Replace/downgrade:** an installed row offers replacement only when both installed and staged generations are valid, IDs/kinds match, and the parsed versions differ. The title distinguishes **Replace with staged version** from **Downgrade to staged version**. Confirmation calls `manager->replace(staged.id)`.

**Uninstall:** a valid installed row can offer uninstall. Confirmation calls `manager->uninstall(installed.kind, installed.id)`. Failure text explicitly notes that mapped/active users may need to stop first.

The UI text states that fresh install publishes verified staged bytes without activation. The app does not manipulate package generations directly.

## Navigation and input

Previous/Next move selection. Confirm opens management for the selected row. A tap on a different row selects it; a tap on the selected row opens its actions. Tapping the header refreshes the inventory and resets selection to row zero. Back/Exit restores normal Back behavior and returns.

## Storage and persistence

The only direct filesystem path is `/sd/Packages/Inbox`. The app defines no private persistent state file. Package generations, verification metadata, mappings, and recovery storage are owned by the package-manager service.

## Network and hardware

The source performs no network operations and no direct external-hardware access.

## Failure handling and implementation limits

Missing required APIs cause startup to return. Failed installed-package metadata calls are skipped. Failed/blocked package operations are reported in the status line. Confirm/cancel menus do not mutate until Confirm. Static limits include 64 inventory rows, 88-byte title buffers, 160-byte subtitle buffers, and 192-byte status text.

## Source identity

Current upstream source of truth:

- `Apps/package_manager.c`: `87ed7a414fcdadd052412d0b1d9b79234ad09b4b`
- `Apps/package_manager.json`: `87842e2b46e84a5c3701f103dc7f3adaf24d77b7`
