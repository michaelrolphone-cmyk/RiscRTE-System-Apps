# Package Manager

Package Manager is the foundational UI for inspecting installed packages and staged packages, then installing, replacing, downgrading, or uninstalling with explicit confirmation.

## Manifest

- Version: **1.1.1** (Reader 1.1.0 → 1.1.1)
- Minimum firmware: **1.2.8**
- Artifact: `package_manager.elf`
- Icon: `solid:f187`
- Categories: `System`, `Software`

## Source behavior

The app builds a bounded 64-row inventory from installed packages, legacy staged directories, and `.rte.zip` archives in `/sd/Packages/Inbox`. It can also display the online generic release catalog when the firmware exposes the size-checked `T5PackageManagerApi` online tail. Package kind, identity, installed version, dependencies, and stage state determine which actions are available.

Fresh installs, online installs, replacement or downgrade, and uninstall each pass through firmware package-manager calls. Replace/downgrade actions require matching package kind and ID, valid installed and staged generations, a distinct parseable three-part version, and confirmation. The app does not handle archive parsing, integrity validation, persistent generations, network transport, hardware access, or package activation directly.

## Current source and release evidence

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` supplies source blob `126fe86e20eaec0a92dd74872fff16def8ebf615` and manifest blob `2176963ba3b355c24c71c12118e716ebe1c2e876`. The app uses `T5PackageManagerApi.h` blob `6a2802e25dbcee5d8f31f3046250fac1c8bc3965`; its Reader regression fixture is `bd081c463fa90782c0bac2b3bc6d96383f1cd863`.

Reader's released [1.1.1 package](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-package_manager-v1.1.1/application-package_manager-1.1.1-xtensa-esp32s3.rte.zip) is 12,798 bytes with SHA-256 `392d8904c03d0270b01ae6f67373957cf0a73d9d9a7a12145928a869de857ced`. Its `package_manager.elf` member is 11,704 bytes, SHA-256 `1afd83a38e53ef5dae26a8da8cf0929aa7fdd9cd4bd28d417bb7b1458d2c4b07`. The independent Xtensa build reproduced that ELF exactly. This evidence does not establish U1 runtime installation or cutover readiness.
