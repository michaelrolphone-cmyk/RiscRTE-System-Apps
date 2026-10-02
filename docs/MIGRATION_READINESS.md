# External migration readiness — 2026-10-02

Reader remains the read-only source of truth. This audit refreshed current Reader master after U1 #96 and CAM #344 merged.

## System-Apps source and release parity

- Reader master: `82caa0997e913f01c1f5f9ab942d056bc9f04a82` (CAM #344 merge; first parent includes merged U1 #96).
- System-Apps current main: `74568c40520c21576628fb6adc46e7b66c92152f` after PR #16; the current grouped manifest PR batches the final twelve manifest-only synchronizations from the same Reader master.
- Current durable claim: [System-Apps issue #17](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/issues/17); prior claims #13 and #15 closed after postmerge verification.
- The 38-input base-aware audit is fully synchronized: 17 unchanged and 21 converged, with no upstream-only, external-only, or conflict entries. All 18 app C sources and all 18 manifests match Reader master. See [source drift](source-drift.json).
- All 18 app release identities now match the current published Reader ELF bytes exactly. The 11 required exact-byte checks are unchanged and pass. Current release URLs, source commits, archive SHA-256 values, and embedded ELF identities are recorded in `sdk/release-baseline.json` and [release parity](release-parity.json).
- The latest grouped batch is manifest-only. No extra version bumps were made: every synchronized version already exists in a published Reader release; manifests and per-app docs capture the package version and artifact provenance.
- Local validation for the latest batch: all twelve affected apps were rebuilt independently against their published package ELFs; the all-app host/unit suite and full 18-app PR CI gate provide the independent pipeline contract. Earlier 18-app local builds and successful CI remain recorded in prior batch evidence.
CAM added the optional Camera Utility and camera-driver packages. Camera Utility is outside this repository's foundational System-Apps set. Driver work remains with its owners; the open X4 Drivers PR #12 was inspected and left untouched.

## Other authorized repositories

Latest fresh main/open-work checks found: Drivers `cb0bd49d1fdd002c639c81437dce9fb0e3055d5b` with owner-owned X4 PR #12; MCU-Dev-Tools `70fd3ad39eaacdb736a16de5f351c128b208f255`; Utilities `4343b808f762b67bb14c39fb50f28d32819af07e`; Productivity `4c9ae58cf172babd4405a0d6b095d1517eb1fa5a`. No open work was found in the last three. Their already completed parity was not repeated.

## Readiness limits

`parity_ready_count` remains **0**. These source, test, build, and published-byte results do not prove U1 ZIP/runtime behavior, separate publication, device installation, physical readiness, or provider cutover. No package-format migration, release automation change, live catalog change, Reader source removal, hardware qualification, or runtime ownership switch is included.

Current final manifest batch: [all remaining manifest sync](PARITY_REFRESH_ALL_MANIFESTS_2026-10-02.md). Historical checkpoints: [U1 package workflow refresh provenance](PARITY_REFRESH_U1_PACKAGE_APPS_2026-10-02.md), [Status Bar Settings refresh](PARITY_REFRESH_2026-10-02.md), [2026-10-01 refresh](PARITY_REFRESH_2026-10-01.md), and [historical ELF lineage](HISTORICAL_ELF_LINEAGE.md).


## 2026-10-02 published manifest reconciliation

Current Reader master declares Button Remap, Clear Reading Cache, and Firmware Update at 1.0.2. System-Apps now tracks those released manifests. Their 1.0.2 ZIPs were extracted from Reader release workflow artifact 11209466823 (run 36965130240); SHA-256 and sizes match GitHub release metadata, and the embedded ELFs match the prior 1.0.1 ELF identities exactly. This refresh updates metadata and per-app provenance without changing binaries or fabricating a version. See [manifest reconciliation provenance](PARITY_REFRESH_MANIFESTS_2026-10-02.md).


## Complete app-manifest source sync

All twelve remaining manifest-only version differences from Reader `82caa0997e913f01c1f5f9ab942d056bc9f04a82` were reconciled together in the current grouped manifest PR. Each change was only the already-published version field; app C sources, remaining manifest fields, and external SDK/build customizations were preserved. See [current manifest provenance](PARITY_REFRESH_ALL_MANIFESTS_2026-10-02.md). This completes source/manifest parity for all 18 approved apps; it does not change `parity_ready_count=0` or qualify runtime/cutover readiness.
