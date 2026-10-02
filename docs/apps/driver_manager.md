# Driver Manager

Driver Manager handles the runtime driver catalog, SD staged driver packages, and retained install recovery. Its role is managing packages and capabilities, not programming an external MCU.

## Manifest

- Version: **1.0.8** (Reader 1.0.6 → 1.0.8)
- Minimum firmware: **1.3.10**
- Artifact: `driver_manager.elf`
- Icon: `solid:f085`
- Categories: `System`, `Hardware`, `Developer`

## Source behavior

Normal online installation uses the shared `T5PackageManagerApi` catalog and filters rows to driver packages. The SD Inbox accepts `.rte.zip` archives and legacy package directories. The app displays dependency, installed-version, update, and stage state, then delegates verified installation and explicit uninstall to firmware services. It does not activate a published driver itself.

When the driver recovery API tail is available, the app lists retained stages and offers separately confirmed retry or discard actions according to firmware-provided recovery flags. It does not directly remove retained stage files. Catalog and Inbox rows are capped at 64; the recovery list is capped at 65 plus a continue row. API version and struct-size checks gate optional operations.

## Current source and release evidence

Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` supplies source blob `b85473ec83247517421f360792212d5db9aeacf1` and manifest blob `7b12d3524b7d395fbfe524a50e87d410273334b0`. The app uses `T5PackageManagerApi.h` blob `6a2802e25dbcee5d8f31f3046250fac1c8bc3965`; its Reader regression fixture is `45dce0bfab696470d8416c7d9e740fd1f24e1a1b`.

Reader's released [1.0.8 package](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/download/app-driver_manager-v1.0.8/application-driver_manager-1.0.8-xtensa-esp32s3.rte.zip) is 12,949 bytes with SHA-256 `30591aed79ba0e85cd51235fd031cb6549ac23701f7f7737c46e4f138b48460e`. Its `driver_manager.elf` member is 11,852 bytes, SHA-256 `eea98d1d7909e27871714882cba652bdf377d2ceec309d06ca105e012ab43867`. The independent Xtensa build reproduced that ELF exactly. This evidence does not establish U1 runtime installation, target-driver behavior, hardware readiness, or cutover readiness.
