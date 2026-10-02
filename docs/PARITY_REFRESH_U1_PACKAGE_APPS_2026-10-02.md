> Follow-up: the three manifest-only updates for Button Remap, Clear Reading Cache, and Firmware Update were synchronized in [the published manifest reconciliation](PARITY_REFRESH_MANIFESTS_2026-10-02.md). The later grouped refresh reconciled the twelve remaining manifest-only rows; see [the complete manifest batch](PARITY_REFRESH_ALL_MANIFESTS_2026-10-02.md). This follow-up does not alter the PR #14 package-workflow result.

# U1 package workflow app refresh — 2026-10-02

## Source-aware scope

Read-only Reader master `82caa0997e913f01c1f5f9ab942d056bc9f04a82` includes U1 #96 and CAM #344. System-Apps main before the refresh was `28d02fb01c9006b025c0f83e829c1fe117283115` (tree `973fcb21a99f00277adc0c1f53721060fa6b8700`). Durable claim: [System-Apps issue #13](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/issues/13).

The prior audited base was Reader `ca66db298c2e735f45e5029083a9bfbd7b6740bd`. Three-way blob comparison found upstream-only source and manifest changes for App Store, Driver Manager, and Package Manager. Their previous external blobs matched that base; no external edits or conflicts existed. Current Reader and external sources now converge for those six inputs. Across the 38 tracked System-Apps inputs, 15 other upstream-only differences are app manifests without corresponding source changes in this increment; they remain visible in `source-drift.json`. The optional Camera Utility and packages owned by Drivers are outside this refresh.

## Versions and published artifacts

No additional versions were invented. The external manifests now match Reader's already released identities:

| App | Version | Reader release asset | Release ZIP | Embedded ELF and independent build |
| --- | --- | --- | --- | --- |
| App Store | 1.0.7 → 1.0.8 | [app-app_store-v1.0.8](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-app_store-v1.0.8) | 10,077 bytes; SHA-256 `bd05715b3d7e1177f9c2c399c6bdbf78367838f44e3a3455171614db63694d47` | 9,044 bytes; SHA-256 `bc57c7f92a77e273a0b061bb1d189313689717453bb3b55d87f846c4cd74ea74`; exact |
| Driver Manager | 1.0.6 → 1.0.8 | [app-driver_manager-v1.0.8](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-driver_manager-v1.0.8) | 12,949 bytes; SHA-256 `30591aed79ba0e85cd51235fd031cb6549ac23701f7f7737c46e4f138b48460e` | 11,852 bytes; SHA-256 `eea98d1d7909e27871714882cba652bdf377d2ceec309d06ca105e012ab43867`; exact |
| Package Manager | 1.1.0 → 1.1.1 | [app-package_manager-v1.1.1](https://github.com/michaelrolphone-cmyk/T5S3-Reader/releases/tag/app-package_manager-v1.1.1) | 12,798 bytes; SHA-256 `392d8904c03d0270b01ae6f67373957cf0a73d9d9a7a12145928a869de857ced` | 11,704 bytes; SHA-256 `1afd83a38e53ef5dae26a8da8cf0929aa7fdd9cd4bd28d417bb7b1458d2c4b07`; exact |

The full release ZIPs were extracted from Reader release workflow artifact `11209466823` (run `36965130240`). Their sizes and digests match GitHub's published release asset metadata; each embedded ELF matches a focused independent build with the target repository's pinned Xtensa compiler. The SDK baseline now records the exact U1 `T5PackageManagerApi.h` blob `6a2802e25dbcee5d8f31f3046250fac1c8bc3965` and current Reader regression fixture blobs for all three apps.

## Local verification and boundaries

- `python3 scripts/test_apps.py`: all 18 host fixtures passed, including the three current Reader package-workflow regressions.
- `scripts/build_all_apps.py --id` for each affected app: structurally validated ELF builds passed; each was byte-identical to the corresponding published ELF above.
- The normal independent pipeline runs on the review head and again on target main. Final exact-head and postmerge run IDs and merge evidence are recorded in Claim #13.
- Published release byte parity is **11/18**, with all 11 required rows exact. The previous nine historical artifact mismatches remain as classified; this refresh does not loosen the gate.
- `parity_ready_count` remains 0. No U1 ZIP/runtime qualification, independent external release, camera/hardware qualification, X4 work, catalog update, firmware deployment, flash, or cutover is claimed.
