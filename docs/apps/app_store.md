# App Store

App Store installs and updates first party application packages and exposes the SD Inbox workflow. It is one of the foundational system applications.

## Manifest

- Version: **1.0.8** (Reader 1.0.7 → 1.0.8)
- Minimum firmware: **1.3.10**
- Artifact: `app_store.elf`
- Icon: `solid:f019`
- Categories: `System`, `Software`

## Source behavior

The app uses the versioned `T5PackageManagerApi` for its online release list and staged packages. It filters the online catalog to application packages, then displays compatibility, installed-version, update, and blocked-stage state. The SD Inbox accepts `.rte.zip` archives and legacy package directories under `/sd/Packages/Inbox`; it previews packages before offering installation. Removal is an explicit confirmed action when the selected package has no allowed staged update.

The source checks the API version and the struct size before using the archive and online API tail. It bounds both catalog and Inbox rows to 64, accepts only safe Inbox basenames, and delegates network transfer, archive validation, package publishing, and persistent state to firmware services. This UI does not activate installed packages directly.

## Current source and release evidence

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` supplies source blob `25b362620a5c7866a4526080363acb6c8527a9cf` and manifest blob `71274d3a079caaa7b87c2b7838425f58a14d9534`. The app uses `T5PackageManagerApi.h` blob `6a2802e25dbcee5d8f31f3046250fac1c8bc3965`; its Reader regression fixture is `0690e33c54d4bbcb511f7e3606767233cc5d7041`.

Reader's released [1.0.8 package](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-app_store-v1.0.8/application-app_store-1.0.8-xtensa-esp32s3.rte.zip) is 10,077 bytes with SHA-256 `bd05715b3d7e1177f9c2c399c6bdbf78367838f44e3a3455171614db63694d47`. Its `app_store.elf` member is 9,044 bytes, SHA-256 `bc57c7f92a77e273a0b061bb1d189313689717453bb3b55d87f846c4cd74ea74`. The independent Xtensa build reproduced that ELF exactly. This evidence does not establish U1 runtime installation or cutover readiness.

## Portable capability build

`PORTABLE_UPDATE_APP` with `PORTABLE_UPDATE_FIRMWARE=0` selects the existing
shared [portable update controller](../PORTABLE_UPDATES.md). It requests only
`software.update.apps@1`, saved-profile namespace 6 and foreground Wi-Fi.
Watch schema-1 ELF/manifest rows are checked against the current admitted native
inventory. Current, unsupported and changed-authority rows cannot be installed.
The two historical clock filename/identity mappings are explicitly validated.

Only strictly newer versions of already authorized apps can reach native
admission. Confirmation, cancellable staging, progress, verified activation and
Restart use the shared portable presentation. It does not install new apps,
grants, drivers or Reader `.rte.zip` bundles. Ordinary builds without the define
retain the original Reader package/SD workflow and published ELF bytes. Portable
development manifest version is 1.1.0; no legacy manifest or live index changes.

Springboard deployment manifest advances from 1.4.2 to 1.4.3 for the App Store
launcher entry. The shared Springboard rendering/legacy implementation is unchanged.
